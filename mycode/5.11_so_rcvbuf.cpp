//
// Created by chen on 2022/9/9.
//

// 修改TCP接收缓冲区大小（服务器端）

#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int const BUFFER_SIZE = 1024;

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("too few arguments.\n");
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    int recvbuf = atoi(argv[3]);
    int len = sizeof recvbuf;

    // 设置发送缓冲区的大小，然后读取新值
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &recvbuf, sizeof recvbuf);
    getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &recvbuf, (socklen_t*)&len);
    printf("TCP receive buffer size: %d.\n", recvbuf);

    int ret = bind(sock, (sockaddr*)&address, sizeof address);
    assert(ret != -1);

    ret = listen(sock, 5);
    assert(ret != -1);

    sockaddr_in client;
    socklen_t client_addrlength = sizeof client;
    int connfd = accept(sock, (sockaddr*)(&client), &client_addrlength);
    if(connfd < 0){
        printf("connection failed.\n");
    }
    else{
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0', BUFFER_SIZE);
        while(true){
            int count = recv(connfd, buffer, BUFFER_SIZE-1, 0);
            printf("[receive] %d bytes: %s.\n", count, buffer);
            if(count == 0) break;
        }
        close(connfd);
    }

    close(sock);

    return 0;
}