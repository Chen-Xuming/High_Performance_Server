//
// Created by chen on 2022/9/16.
//

// [客户端] 通过非阻塞connect连接服务器
// 实现：使用select并设置有限的超时时限

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>

int set_nonblocking(int fd);
int unblock_connect(const char *ip, int port, int time);

int main(){
    const char *ip = "114.132.59.100";
    const int port = 12345;

    int sockfd = unblock_connect(ip, port, 10);
    if(sockfd < 0) return 1;

    close(sockfd);
    return 0;
}

/*
 *   设置fd的状态为非阻塞模式，并返回旧的状态
 */
int set_nonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);            // F_GETFL: 返回fd的状态标志
    int new_option = old_option | O_NONBLOCK;       // 添加新的状态：设置非阻塞I/O
    fcntl(fd, F_SETFL, new_option);                 // F_SETFL: 设置fd状态
    return old_option;
}


/*
 *  超时连接函数
 *
 *  ip: 服务器ip
 *  port: 端口号
 *  time: 超时时间（秒）
 *
 *  返回：成功 = sockfd      失败 = -1
 */
int unblock_connect(const char *ip, int port, int time){
    sockaddr_in sockaddr_server{};
    sockaddr_server.sin_family = AF_INET;
    sockaddr_server.sin_port = htons(port);
    inet_pton(AF_INET, ip, &sockaddr_server.sin_addr);

    int sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    int fdopt = set_nonblocking(sock_fd);   // 设置sock_fd的状态为非阻塞模式，并返回旧的状态
    int ret = connect(sock_fd, (sockaddr *)&sockaddr_server, sizeof sockaddr_server);   // 由于是非阻塞式，所以立即返回
    if(ret == 0){   // 如果连接成功，则恢复sockfd原有状态，并立即返回
        printf("connect with server immediately!\n");
        fcntl(sock_fd, F_SETFL, fdopt);
        return ret;
    }
    // 如果连接没有立即建立，那么只有当errno是EINPROGRESS时才表示连接还在进行，否则出错。
    else{
        if(errno == EINPROGRESS){
            printf("still connecting, please wait.\n");
        }else{
            printf("unblock connection failed.\n");
            return -1;
        }
    }

    fd_set readfds;
    fd_set writefds;
    timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(sock_fd, &writefds);     // 告诉内核监听sock_fd的写事件
    timeout.tv_sec = time;
    timeout.tv_usec = 0;

    ret = select(sock_fd + 1, NULL, &writefds, NULL, &timeout);     // timeout之前返回监听结果
    // select发生错误
    if(ret == -1){
        printf("error occurs when calling select(): %s\n", gai_strerror(errno));
        return -1;
    }
    // 连接超时未响应
    else if(!FD_ISSET(sock_fd, &writefds)){
        printf("connection timeout.\n");
        close(sock_fd);
        return -1;
    }

    // ------ 获取 sock_fd 上的错误（如果没有错误，说明连接成功） --------------
    int error = 0;
    socklen_t length = sizeof error;

    // getsockopt函数出错
    if(getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0){
        printf("get socket option failed.\n");
        close(sock_fd);
        return -1;
    }
    // 错误号不为0表示连接出错
    if(error != 0){
        printf("connection failed after select(). error: %s\n", gai_strerror(errno));
        close(sock_fd);
        return -1;
    }
    // 连接成功
    printf("connection ready after select with the socket: %d\n", sock_fd);
    fcntl(sock_fd, F_SETFL, fdopt);     // 还原sock_fd状态
    return sock_fd;
}



















