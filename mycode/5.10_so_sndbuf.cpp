//
// Created by chen on 2022/9/9.
//

// 修改TCP发送缓冲区（客户端）

#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int const BUFFER_SIZE = 512;

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("too few arguments.\n");
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    int sendbuf = atoi(argv[3]);
    int len = sizeof sendbuf;

    // 设置发送缓冲区的大小，然后读取新值
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof sendbuf);
    getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuf, (socklen_t*)&len);
    printf("TCP send buffer size: %d.\n", sendbuf);

    if(connect(sock, (sockaddr*)&server_address, sizeof(server_address)) != -1){
        char buffer[BUFFER_SIZE];
        memset(buffer, 'a', BUFFER_SIZE);
        send(sock, buffer, BUFFER_SIZE, 0);
    }
    close(sock);

    return 0;
}