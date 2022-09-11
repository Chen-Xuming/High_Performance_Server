//
// Created by chen on 2022/9/11.
//

// 使用dup将printf输出的内容重定向到连接客户端的socket

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/ipc.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include<assert.h>
#include<unistd.h>
#include<stdio.h>
#include<netdb.h>

const char *ip = "10.0.20.4";
const int port = 12345;

int main(){

    sockaddr_in server{};
    server.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server.sin_addr);
    server.sin_port = htons(port);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock > 0);

    int ret = bind(sock, (sockaddr*)&server, sizeof(server));
    assert(ret != -1);

    ret = listen(sock, 5);
    assert(ret != -1);

    sockaddr_in client{};
    socklen_t client_len = sizeof (client);
    int connfd = accept(sock, (sockaddr*)&client, &client_len);
    if(connfd < 0){
        printf("error: %s.\n", gai_strerror(errno));
    }else{
        close(STDOUT_FILENO);   // 关闭标准输出文件描述符（值为1）
        dup(connfd);            // 返回当前最小可用文件描述符，即 STDOUT_FILENO, 指向connfd
        printf("message from server.");     // printf 将重定向到connfd.
        close(connfd);
    }

    close(sock);

    return 0;
}
