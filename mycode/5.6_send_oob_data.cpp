//
// Created by chen on 2022/9/8.
//

// 客户端向服务器发送带外数据（OOB, out of band, 紧急数据）

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char* argv[]){
    if(argc <= 2){
        printf("too few arguments.\n");
        return 1;
    }

    const char* ip = argv[1];   // 服务器ip
    int port = atoi(argv[2]);   // 服务器端口

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_address.sin_addr);
    server_address.sin_port = htons(port);

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    if(connect(sockfd, (sockaddr*)&server_address, sizeof(server_address)) < 0){
        printf("connection failed.\n");
    }else{
        const char *oob_data = "hello world!";
        const char *normal_data = "oob data...";
        send(sockfd, normal_data, strlen(normal_data), 0);
        send(sockfd, oob_data, strlen(oob_data), MSG_OOB);
        send(sockfd, normal_data, strlen(normal_data), 0);
    }

    close(sockfd);
    return 0;
}