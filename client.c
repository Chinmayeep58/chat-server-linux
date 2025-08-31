
// client.c
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <termios.h>
#include <ctype.h>
#include <signal.h>

#define PORT 9999
#define GREEN  "\033[0;32m"
#define BLUE   "\033[0;34m"
#define YELLOW "\033[0;33m"
#define RESET  "\033[0m"
#define CLEAR_LINE "\33[2K\r"

static struct termios orig_termios;

static void restore_terminal(void){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
static void set_raw_mode(void){
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(restore_terminal);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1; // 100ms
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
static void on_sigint(int s){ (void)s; exit(0); }

static void now_hm(char *out, size_t n){
    time_t t=time(NULL); struct tm *lt=localtime(&t);
    snprintf(out, n, "%02d:%02d", lt->tm_hour, lt->tm_min);
}

/*************** Base64 ***************/
static const char b64tab[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static char *b64_encode(const unsigned char *in, size_t len){
    size_t outlen = 4*((len+2)/3);
    char *out = malloc(outlen+1);
    size_t i,j; for(i=0,j=0;i<len;){
        unsigned a=i<len?in[i++]:0;
        unsigned b=i<len?in[i++]:0;
        unsigned c=i<len?in[i++]:0;
        unsigned triple=(a<<16)|(b<<8)|c;
        out[j++]=b64tab[(triple>>18)&0x3F];
        out[j++]=b64tab[(triple>>12)&0x3F];
        out[j++]=(i-1>len)?'=':b64tab[(triple>>6)&0x3F];
        out[j++]=(i>len)?'=':b64tab[triple&0x3F];
    }
    out[j]=0; return out;
}
static int b64_index(char c){
    if('A'<=c&&c<='Z') return c-'A';
    if('a'<=c&&c<='z') return c-'a'+26;
    if('0'<=c&&c<='9') return c-'0'+52;
    if(c=='+') return 62; if(c=='/') return 63; return -1;
}
static unsigned char *b64_decode(const char *in, size_t len, size_t *outlen){
    if(len%4) return NULL;
    size_t olen = len/4*3; if(len>=4 && in[len-1]=='=') olen--; if(len>=4 && in[len-2]=='=') olen--;
    unsigned char *out=malloc(olen+1);
    size_t i=0,j=0;
    while(i<len){
        int a=b64_index(in[i++]); int b=b64_index(in[i++]);
        int c=in[i]=='='?-1:b64_index(in[i]); i++;
        int d=in[i]=='='?-1:b64_index(in[i]); i++;
        if(a<0||b<0) { free(out); return NULL; }
        unsigned triple = (a<<18)|(b<<12)|((c<0?0:c)<<6)|(d<0?0:d);
        out[j++]=(triple>>16)&0xFF;
        if(c>=0) out[j++]=(triple>>8)&0xFF;
        if(d>=0) out[j++]=triple&0xFF;
    }
    out[j]=0; if(outlen) *outlen=j; return out;
}
/********************************************/

static int is_mostly_text(const unsigned char *buf, size_t n){
    if(n==0) return 0;
    size_t printable=0;
    for(size_t i=0;i<n;i++){
        unsigned char c=buf[i];
        if(c==9||c==10||c==13|| (c>=32 && c<127)) printable++;
    }
    return printable*100/n >= 85;
}

static void print_prompt(const char *who_color, const char *who, const char *current){
    char tm[6]; now_hm(tm,sizeof tm);
    printf(CLEAR_LINE "%s[%s] %s%s %s", who_color, tm, who, RESET, current?current:"");
    fflush(stdout);
}

int main(void){
    signal(SIGINT, on_sigint);
    set_raw_mode();

    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr={0};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(PORT);
    addr.sin_addr.s_addr=inet_addr("127.0.0.1");

    if(connect(cfd,(struct sockaddr*)&addr,sizeof addr)<0){
        perror("connect"); return 1;
    }
    printf(YELLOW "Connected to server.\n" RESET);

    struct pollfd fds[2]={{STDIN_FILENO,POLLIN,0},{cfd,POLLIN,0}};
    char mybuf[4096]={0}; size_t mylen=0;
    char peerbuf[4096]={0}; size_t peerlen=0;

    /* File RX state */
    int receiving_file=0;
    FILE *fout=NULL;
    char recv_name[512]={0};
    size_t recv_total=0, recv_received=0;

    print_prompt(GREEN,"Client:", mybuf);

    char netin[8192]; size_t netlen=0;

    for(;;){
        poll(fds,2,50);

        /* Keyboard -> send live */
        if(fds[0].revents & POLLIN){
            char ch;
            ssize_t r=read(STDIN_FILENO,&ch,1);
            if(r>0){
                if(ch==127 || ch==8){ // backspace
                    if(mylen>0){ mylen--; mybuf[mylen]=0; }
                    dprintf(cfd,"B:\n");
                    print_prompt(GREEN,"Client:", mybuf);
                } else if(ch=='\n' || ch=='\r'){
                    dprintf(cfd,"E:\n"); // finalize remote
                    if(strncmp(mybuf,"/send ",6)==0){
                        const char *path=mybuf+6;
                        FILE *fp=fopen(path,"rb");
                        if(!fp){
                            printf(CLEAR_LINE YELLOW "[send] cannot open %s\n" RESET, path);
                            mylen=0; mybuf[0]=0; print_prompt(GREEN,"Client:", mybuf);
                        }else{
                            fseek(fp,0,SEEK_END); long sz=ftell(fp); fseek(fp,0,SEEK_SET);
                            dprintf(cfd,"FBEGIN:%s:%ld\n", strrchr(path,'/')?strrchr(path,'/')+1:path, sz);
                            unsigned char chunk[3072];
                            size_t rd;
                            while((rd=fread(chunk,1,sizeof chunk,fp))>0){
                                char *b64=b64_encode(chunk, rd);
                                dprintf(cfd,"FCHUNK:%s\n", b64);
                                free(b64);
                            }
                            fclose(fp);
                            dprintf(cfd,"FEND\n");
                            printf(CLEAR_LINE YELLOW "[send] %s (%ld bytes) sent.\n" RESET, path, sz);
                            mylen=0; mybuf[0]=0; print_prompt(GREEN,"Client:", mybuf);
                        }
                    } else {
                        char tm[6]; now_hm(tm,sizeof tm);
                        printf(CLEAR_LINE BLUE "[%s] Server:%s %s\n" RESET, tm, RESET, mybuf);
                        dprintf(cfd,"E:\n"); // ensure finalize
                        mylen=0; mybuf[0]=0; print_prompt(GREEN,"Client:", mybuf);
                    }
                } else {
                    if(mylen+1<sizeof mybuf){
                        mybuf[mylen++]=ch; mybuf[mylen]=0;
                        dprintf(cfd,"T:%c\n", ch);
                        print_prompt(GREEN,"Client:", mybuf);
                    }
                }
            }
        }

        /* Network -> read & parse line protocol */
        if(fds[1].revents & POLLIN){
            ssize_t n=recv(cfd, netin+netlen, sizeof(netin)-netlen-1, 0);
            if(n<=0){ printf("\n" YELLOW "Server disconnected.\n" RESET); break; }
            netlen+=n; netin[netlen]=0;

            char *line_start=netin;
            for(;;){
                char *nl=strchr(line_start,'\n');
                if(!nl) break;
                *nl=0;

                if(strncmp(line_start,"T:",2)==0){
                    char ch=line_start[2];
                    if(peerlen+1<sizeof peerbuf){ peerbuf[peerlen++]=ch; peerbuf[peerlen]=0; }
                    print_prompt(BLUE,"Server:", peerbuf);
                } else if(strcmp(line_start,"B:")==0){
                    if(peerlen>0){ peerlen--; peerbuf[peerlen]=0; }
                    print_prompt(BLUE,"Server:", peerbuf);
                } else if(strcmp(line_start,"E:")==0){
                    char tm[6]; now_hm(tm,sizeof tm);
                    printf(CLEAR_LINE BLUE "[%s] Server:%s %s\n" RESET, tm, RESET, peerbuf[0]?peerbuf:"");
                    peerlen=0; peerbuf[0]=0;
                    print_prompt(GREEN,"Client:", mybuf);
                } else if(strncmp(line_start,"FBEGIN:",7)==0){
                    char *p=line_start+7;
                    char *colon=strchr(p,':'); if(!colon){} else {
                        *colon=0; snprintf(recv_name,sizeof recv_name,"%s",p);
                        char *szs=colon+1; recv_total = strtoull(szs,NULL,10);
                        char outname[600]; snprintf(outname,sizeof outname,"received_%s", recv_name);
                        fout=fopen(outname,"wb"); receiving_file=1; recv_received=0;
                        printf(CLEAR_LINE YELLOW "[receive] %s (%zu bytes) ...\n" RESET, outname, recv_total);
                    }
                } else if(strncmp(line_start,"FCHUNK:",7)==0 && receiving_file){
                    const char *b64=line_start+7; size_t outn=0;
                    unsigned char *decoded=b64_decode(b64, strlen(b64), &outn);
                    if(decoded && fout){ fwrite(decoded,1,outn,fout); recv_received+=outn; }
                    free(decoded);
                } else if(strcmp(line_start,"FEND")==0 && receiving_file){
                    if(fout){ fclose(fout); fout=NULL; }
                    receiving_file=0;
                    char saved[700]; snprintf(saved,sizeof saved,"received_%s", recv_name);
                    FILE *rp=fopen(saved,"rb");
                    if(rp){
                        unsigned char peek[2048]; size_t rd=fread(peek,1,sizeof peek,rp);
                        fclose(rp);
                        if(is_mostly_text(peek, rd)){
                            printf(YELLOW "----- %s (preview) -----\n" RESET, saved);
                            fwrite(peek,1,rd,stdout);
                            if(rd==sizeof(peek)) printf("\n" YELLOW "----- (truncated) -----\n" RESET);
                            else printf("\n" YELLOW "------------------------\n" RESET);
                        } else {
                            printf(YELLOW "[receive] Saved binary (likely image) to %s\n" RESET, saved);
                        }
                    }
                    print_prompt(GREEN,"Client:", mybuf);
                } else {
                    // ignore
                }

                line_start=nl+1;
            }
            size_t remain = netin+netlen - line_start;
            memmove(netin, line_start, remain);
            netlen=remain; netin[netlen]=0;
        }
    }

    close(cfd);
    return 0;
}
