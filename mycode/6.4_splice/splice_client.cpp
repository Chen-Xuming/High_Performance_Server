//
// Created by chen on 2022/9/12.
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main(){

    const char* ip = "114.132.59.100";   // 服务器ip
    int port = 44444;   // 服务器端口

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    if(connect(sockfd, (sockaddr*)&server_address, sizeof(server_address)) < 0){
        printf("connection failed.\n");
    }else{
        const char *message = "Hello world! I love China!";
        int size = send(sockfd, message, strlen(message), 0);
        if(size != 0){
            printf("[send] %s\n", message);
        }
        char recvbuf[512];
        memset(recvbuf, '\0', 512);
        size = recv(sockfd, recvbuf, 500, 0);
        size = recv(sockfd, recvbuf, 500, 0);
        printf("[receive] %s\n", recvbuf);
    }

    close(sockfd);
    return 0;
}