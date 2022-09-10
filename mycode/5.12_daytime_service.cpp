//
// Created by chen on 2022/9/10.
//

// 在客户端获取服务器daytime服务信息

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]){
    assert(argc == 2);

    char *ip = argv[1];
    printf("server ip: %s\n", ip);

    // 获取daytime服务的端口号
    servent *serv_info = getservbyname("daytime", "tcp");
    assert(serv_info);
    printf("daytime port: %d\n", ntohs(serv_info->s_port));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = serv_info->s_port;
    inet_pton(AF_INET, ip, &address.sin_addr);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int result = connect(sockfd, (sockaddr*)&address, sizeof (address));
    assert(result != -1);

    char buffer[128];
    result = read(sockfd, buffer, sizeof (buffer));
    assert(result > 0);
    buffer[result] = '\0';
    printf("the day time is: %s\n", buffer);
    close(sockfd);

    return 0;
}