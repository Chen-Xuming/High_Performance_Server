//
// Created by chen on 2022/9/11.
//

// 接收服务器重定向的内容

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

const char *ip = "114.132.59.100";
const int port = 12345;

int main(){
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server.sin_addr);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    if(connect(sock, (sockaddr*)&server, sizeof (server)) != -1){
        char buffer[512];
        printf("connect successful!\n");
        if(recv(sock, buffer, sizeof (buffer), 0) > 0){
            printf("[receive] %s\n", buffer);
        } else{
            printf("error: %s\n", gai_strerror(errno));
        }
    }

    close(sock);
    return 0;
}