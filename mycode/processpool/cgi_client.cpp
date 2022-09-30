//
// Created by chen on 2022/9/30.
//

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include<assert.h>
#include<unistd.h>
#include<stdio.h>
#include<netdb.h>

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage: %s ip port\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    const int port = atoi(argv[2]);

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server.sin_addr);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    if(connect(sock, (sockaddr*)&server, sizeof (server)) != -1){
        const char *send_msg = "cgi\r\n";
        char recv_buffer[512];
        memset(recv_buffer, '\0', 512);
        printf("connect successful!\n");
        send(sock, send_msg, strlen(send_msg), 0);
        recv(sock, recv_buffer, 512-1, 0);
        recv(sock, recv_buffer, 512-1, 0);
        recv(sock, recv_buffer, 512-1, 0);
        printf("receive: %s\n", recv_buffer);
    }

    close(sock);
    return 0;
}