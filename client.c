#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>

#define GREEN  "\033[0;32m"
#define RESET  "\033[0m"

int main(){
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in address = {
        AF_INET,
        htons(9999),
        inet_addr("127.0.0.1")
    };

    connect(socketfd, (struct sockaddr*)&address, sizeof(address));

    struct pollfd fds[2] = {
        { 0, POLLIN, 0 },        // stdin
        { socketfd, POLLIN, 0 }  // server socket
    };

    for(;;){
        char buffer[256] = {0};
        poll(fds, 2, 5000);

        // client typing -> notify server
        if(fds[0].revents & POLLIN){

            int n = read(0, buffer, 255);
            buffer[n-1] = '\0'; // remove newline

            send(socketfd, buffer, strlen(buffer), 0);
        }

        // server message -> print in green
        if(fds[1].revents & POLLIN){
            int n = recv(socketfd, buffer, 255, 0);
            if(n <= 0){
                printf("Server disconnected.\n");
                return 0;
            }
            buffer[n] = '\0';

            if(strcmp(buffer, "/typing") == 0){
                printf("%s[Server is typing...]%s\n", GREEN, RESET);
            } else {
                printf("%sServer:%s %s\n", GREEN, RESET, buffer);
            }
        }
    }

    return 0;
}
