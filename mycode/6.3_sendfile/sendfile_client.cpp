//
// Created by chen on 2022/9/11.
//

// 客户端接收服务器传来的文件

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>

int main(){
    const char *ip = "114.132.59.100";
    const int port = 44444;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);

    if(connect(sock, (sockaddr*)&server_addr, sizeof (server_addr)) != -1){
        char buf[2048] = {0};
        int recv_size = recv(sock, buf, sizeof(buf) - 1, 0);
        if(recv_size < 0){
            printf("receive failed.\n");
        }else if(recv_size == 0) {
            printf("peer shutdown connect\n");
        }else{
            printf("receive:\n%s\n", buf);
        }
    }else{
        printf("connection failed.\n");
    }
    close(sock);

    return 0;
}