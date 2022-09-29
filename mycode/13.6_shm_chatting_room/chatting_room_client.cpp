//
// Created by chen on 2022/9/17.
//

/*
 *  网络聊天室客户端
 *  主要功能：
 *  1. 从标准输入终端读取用户输入的数据，并发送到服务器
 *  2. 往标准输出终端打印服务器发来的数据
 */

#define _GNU_SOURCE 1   // 在各个头文件中启动某些选项. 这里是为了用 POLLRDHUP 选项，它由GNU引入
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>

#define BUFFER_SIZE 512

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    // ---------- 1. 初始化连接服务器的socket --------------
    const char *ip = argv[1];
    const int port = atoi(argv[2]);

    sockaddr_in sockaddr_server{};
    sockaddr_server.sin_family = AF_INET;
    sockaddr_server.sin_port = htons(port);
    inet_pton(AF_INET, ip, &sockaddr_server.sin_addr);

    int sock_server = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock_server >= 0);
    if(connect(sock_server, (sockaddr*)&sockaddr_server, sizeof sockaddr_server) < 0){
        printf("connect to the server failed.\n");
        close(sock_server);
        return -1;
    }else{
        printf("welcome to the chatting room.\n");
    }

    // ----------- 2. 设置poll要监听的socket及事件 ------------
    pollfd fds[2];

    // fds[0] = 标准输入（fd=0）
    fds[0].fd = 0;
    fds[0].events = POLLIN;     // 监听数据可读
    fds[0].revents = 0;

    // fds[1] = 与服务器连接的socket
    fds[1].fd = sock_server;
    fds[1].events = POLLIN | POLLRDHUP;     // 监听数据可读、TCP连接被对方关闭、对方关闭写操作
    fds[1].revents = 0;

    // 后面将使用splice的方式将数据从标准输入零拷贝到sock_server上，用管道中继
    // sock_stdin ----> pipefd[1] ----> pipefd[0] ----> sock_server
    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret != -1);

    // ------------- 3. 使用poll监听socket，并处理相关事务 ---------------
    while(true){
        ret = poll(fds, 2, -1);     // 监听两个socket，无限等待
        if(ret < 0){
            printf("poll failure.\n");
            break;
        }

        // 连接关闭
        if(fds[1].revents & POLLRDHUP){
            printf("server close the connection\n");
            break;
        }

        // 接收服务器数据
        else if(fds[1].revents & POLLIN){
            memset(read_buf, '\0', BUFFER_SIZE);
            recv(fds[1].fd, read_buf, BUFFER_SIZE - 1, 0);
            printf("[receive] %s\n", read_buf);
        }

        // 发送数据给服务器
        if(fds[0].revents & POLLIN){
            ret = splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            if(ret == -1){
                printf("[splice error #1] %s\n", gai_strerror(errno));
            }else{
//                printf("[splice] %d bytes, from fd(0) to pipefd[1].\n", ret);
                fflush(stdin);
            }
            ret = splice(pipefd[0], NULL, sock_server, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            if(ret == -1){
                printf("[splice error #2] %s\n", gai_strerror(errno));
            }else{
//                printf("[splice] %d bytes, from pipefd[0] to sock_server.\n", ret);
                fflush(stdout);
            }
        }
    }

    close(sock_server);
    return 0;
}


