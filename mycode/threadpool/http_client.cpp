//
// Created by chen on 2022/10/2.
//
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<unistd.h>
#include <stdio.h>
#include <string>

int main(int argc, char *argv[]){
    if(argc <= 3){
        printf("usage: %s ip_address port_number url\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    const int port = atoi(argv[2]);
    std::string url(argv[3]);

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server.sin_addr);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    if(connect(sock, (sockaddr*)&server, sizeof (server)) != -1){
        std::string request_data;
        request_data.append("GET ").append(url).append(" HTTP/1.1\r\nConnection: close\r\n\r\nHELLO WORLD.");
        const char *request_str = request_data.c_str();
        int ret = send(sock, request_str, strlen(request_str), 0);
        if(ret != -1){
            printf("[send] %s\n", request_str);
        }
        char recvbuffer[2048];
        memset(recvbuffer, '\0', 2048);
        ret = recv(sock, recvbuffer, 2048-1, 0);
        if(ret != -1){
            printf("[receive] %s\n", recvbuffer);
        }
    }

    close(sock);
    return 0;
}

/*
 *
 *      http://114.132.59.100/http_test_response.txt
 *
 *
 */