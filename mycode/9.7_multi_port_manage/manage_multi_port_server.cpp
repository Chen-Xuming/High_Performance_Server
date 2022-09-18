//
// Created by chen on 2022/9/17.
//

// 将 TCP socket 和 UDP socket 绑定在同一个端口，使用epoll同时监听这两个socket，以及accept返回的fd
// 对于收到的数据，全都回射给客户端

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

int set_nonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;     // 事件所从属的fd
    event.events = EPOLLIN | EPOLLET;   // 监听数据可读，以边缘触发模式
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    const char *ip = argv[1];
    const int port = atoi(argv[2]);

    // ----------- 1. 初始化 TCP socket 和 UDP socket，将它们绑定在同一个端口上 --------------------
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    // TCP socket
    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listen_fd >= 0);
    int ret = bind(listen_fd, (sockaddr*)&address, sizeof address);
    assert(ret != -1);
    ret = listen(listen_fd, 5);
    assert(ret != -1);

    // UDP socket
    // UDP是无连接的，所以不用listen
    int udp_fd = socket(PF_INET, SOCK_DGRAM, 0);    // 数据报服务
    assert(udp_fd >= 0);
    ret = bind(udp_fd, (sockaddr*)&address, sizeof address);
    assert(ret != -1);

    // -------------- 2. 创建epoll实例，注册各个事件 ----------------------
    epoll_event events[MAX_EVENT_NUMBER];       // 内核返回的就绪事件存储在这里！
    int epoll_fd = epoll_create(1);
    assert(epoll_fd != -1);
    // 注册两种socket上的可读事件
    addfd(epoll_fd, listen_fd);
    addfd(epoll_fd, udp_fd);

    // --------------- 3. 监听socket，处理相关事件 ---------------------
    while(1){
        int num_ready = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1);     // 无限等待监听
        if(num_ready < 0){
            printf("epoll failure.\n");
            break;
        }

        for(int i = 0; i < num_ready; i++){
            int sock_fd = events[i].data.fd;

            // case 1: 监听到tcp socket上有新的连接，把它加入epoll红黑树
            if(sock_fd == listen_fd){
                sockaddr_in client_address{};
                socklen_t client_addr_len = sizeof client_address;
                int connfd = accept(listen_fd, (sockaddr*)&client_address, &client_addr_len);
                if(connfd < 0){
                    printf("new connection failed. error: %s \n", gai_strerror(errno));
                }else{
                    addfd(epoll_fd, connfd);
                }
            }

            // case 2: 监听到 udp socket 有数据可读，则读取并回射给客户端
            if(sock_fd == udp_fd){
                char buf[UDP_BUFFER_SIZE];
                memset(buf, '\0', UDP_BUFFER_SIZE);
                sockaddr_in client_address{};
                socklen_t client_addr_len = sizeof client_address;
                ret = recvfrom(udp_fd, buf, UDP_BUFFER_SIZE-1, 0, (sockaddr*)&client_address, &client_addr_len);
                if(ret < 0){
                    printf("[UDP recvfrom] error: %s\n", gai_strerror(errno));
                }else if(ret > 0){
                    ret = sendto(udp_fd, buf, UDP_BUFFER_SIZE-1, 0, (sockaddr*)&client_address, client_addr_len);   // 回射
                    if(ret < 0){
                        printf("[UDP sendto] error: %s\n", gai_strerror(errno));
                    }
                }
            }

            // case 3: 客户端发来消息
            else if(events[i].events & EPOLLIN){
                char buf[TCP_BUFFER_SIZE];
                while(1){
                    memset(buf, '\0', TCP_BUFFER_SIZE);
                    ret = recv(sock_fd, buf, TCP_BUFFER_SIZE - 1, 0);
                    if(ret < 0){
                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            break;
                        }
                        close(sock_fd);
                        break;
                    }
                    else if (ret == 0) close(sock_fd);
                    else{
                        send(sock_fd, buf, ret, 0);     // 回射
                    }
                }
            }

            else{
                printf("something else happened. \n");
            }
        }
    }

    close(listen_fd);
    return 0;
}















