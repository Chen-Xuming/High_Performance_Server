//
// Created by chen on 2022/9/8.
//

// 服务器端接收客户端发来的数据

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int const BUFFER_SIZE = 1024;

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("too few arguments.\n");
        return -1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    int ret = bind(sock, (sockaddr*)(&address), sizeof (address));
    assert(ret != -1);

    ret = listen(sock, 5);
    assert(ret != -1);

    sockaddr_in client;
    socklen_t client_addr_length = sizeof (client);
    int connfd = accept(sock, (sockaddr*)(&client), &client_addr_length);

    if(connfd < 0){
        printf("error.\n");
    }else{
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0', BUFFER_SIZE);
        ret = recv(connfd, buffer, BUFFER_SIZE-1, 0);
        printf("[receive] %d bytes of normal data: %s\n", ret, buffer);

        memset(buffer, '\0', BUFFER_SIZE);
        ret = recv(connfd, buffer, BUFFER_SIZE-1, MSG_OOB);     // 接收紧急消息
        if(ret != -1)
            printf("[receive] %d bytes of OOB data: %s\n", ret, buffer);
        else{
            printf("[recv oob]errno is: %d\n", errno);
        }

        memset(buffer, '\0', BUFFER_SIZE);
        ret = recv(connfd, buffer, BUFFER_SIZE-1, 0);
        printf("[receive] %d bytes of normal data: %s\n", ret, buffer);

        close(connfd);
    }

    close(sock);
    return 0;
}