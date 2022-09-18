//
// Created by chen on 2022/9/16.
//

// [服务器端]
// 普普通通的与客户端连接的程序

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>

int main(){
    const char *ip = "10.0.20.4";
    const int port = 12345;

    sockaddr_in sock_server_addr{};
    sock_server_addr.sin_family = AF_INET;
    sock_server_addr.sin_port = htons(port);
    inet_pton(PF_INET, ip, &sock_server_addr.sin_addr);

    int sock_server = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock_server >= 0);

    int ret = bind(sock_server, (sockaddr *)&sock_server_addr, sizeof sock_server_addr);
    assert(ret != -1);

    ret = listen(sock_server, 5);
    assert(ret != -1);

    sockaddr_in sock_client_addr{};
    socklen_t client_addr_len = sizeof sock_client_addr;
    int conn_fd = accept(sock_server, (sockaddr*)&sock_client_addr, &client_addr_len);
    if(conn_fd < 0){
        printf("connection failed, error: %s.\n", gai_strerror(errno));
    }else{
        // 打印客户端的ip和端口号
        char *client_ip = inet_ntoa(sock_client_addr.sin_addr);
        int client_port = ntohs(sock_client_addr.sin_port);
        printf("connect successfully, client ip: %s, port: %d.\n", client_ip, client_port);
        close(conn_fd);
    }

    close(sock_server);
    return 0;
}