#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>

#define GREEN "\033[0;32m"
#define RESET "\033[0m"

int main(){
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in address = {
        AF_INET,
        htons(9999),
        INADDR_ANY
    };

    bind(socketfd, (struct sockaddr*)&address, sizeof(address));
    listen(socketfd, 10);

    int clientfd = accept(socketfd, NULL, NULL);

    struct pollfd fds[2] = {
        { 0, POLLIN, 0 },        // stdin
        { clientfd, POLLIN, 0 }  // client socket
    };

    for(;;){
        char buffer[256] = {0};
        poll(fds, 2, 5000);

        // server typing -> send to client
        if(fds[0].revents & POLLIN){
            int n = read(0, buffer, 255);
            buffer[n-1] = '\0'; // remove newline

            send(clientfd, buffer, strlen(buffer), 0);
        }

        // client message -> print
        if(fds[1].revents & POLLIN){
            int n = recv(clientfd, buffer, 255, 0);
            if(n <= 0){
                printf("Client disconnected.\n");
                return 0;
            }
            buffer[n] = '\0';

            // handle typing indicator
            if(strcmp(buffer, "/typing") == 0){
                printf("%s[Client is typing...]%s\n", GREEN, RESET);
            } else {
                printf("%sClient:%s %s\n", GREEN, RESET, buffer);
            }
        }
    }

    return 0;
}
