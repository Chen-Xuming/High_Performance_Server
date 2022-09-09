//
// Created by chen on 2022/9/8.
//
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("too few arguments.\n");
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sock = socket(PF_INET, SOCK_STREAM, 0);     // 创建socket
    assert(sock >= 0);

    int ret = bind(sock, (sockaddr*)&address, sizeof address);  // 绑定地址
    assert(ret != -1);

    ret = listen(sock, 5);  // 创建socket监听队列，存放待处理的客户连接
    assert(ret != -1);

    printf("waiting for 20s...");
    fflush(stdout);
    sleep(20);  // 暂停若干秒以等待客户端连接和相关操作（掉线/退出）

    sockaddr_in client;
    socklen_t client_addrlength = sizeof(client);
    int connfd = accept(sock, (sockaddr *)&client, &client_addrlength); // 从监听队列中接受一个连接

    if(connfd < 0){
        printf("errono is: %d\n", errno);
    }else{
        char remote[INET_ADDRSTRLEN];
        printf("connected with ip: %s and port: %d\n", inet_ntop(AF_INET, &client.sin_addr, remote, INET_ADDRSTRLEN),
               ntohs(client.sin_port));
    }

    close(sock);    // 关闭socket
    return 0;
}