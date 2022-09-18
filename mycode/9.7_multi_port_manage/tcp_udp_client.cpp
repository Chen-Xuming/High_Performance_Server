//
// Created by chen on 2022/9/17.
//

// 客户端也有 TCP 和 UDP 两个socket，使用epoll监听。

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>

#define MAX_EVENT_NUMBER 100
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
    event.events = EPOLLIN | EPOLLET;   // 监听数据可读事件，以边缘触发模式
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

    printf("-------------- server info ------------\n");
    printf("ip: %s\n", ip);
    printf("port: %d\n", port);
    printf("---------------------------------------\n\n");

    // --------- 1. 初始化 TCP / UDP socket --------------------
    sockaddr_in server_address{};
    socklen_t server_addr_len = sizeof server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_address.sin_addr);

    int tcp_fd = socket(PF_INET, SOCK_STREAM, 0);
    int udp_fd = socket(PF_INET, SOCK_DGRAM, 0);
    assert(tcp_fd >= 0);

    // TCP要连接，UDP不用
    if(connect(tcp_fd, (sockaddr*)&server_address, sizeof server_address) < 0){
        printf("tcp connection failed. error: %s\n", gai_strerror(errno));
        close(tcp_fd);
        return -1;
    }else{
        printf("tcp connected successfully.\n");
    }

    // --------------- 2. epoll初始化 --------------------------
    epoll_event events[MAX_EVENT_NUMBER];
    int epoll_fd = epoll_create(1);
    assert(epoll_fd != -1);
    addfd(epoll_fd, tcp_fd);
    addfd(epoll_fd, udp_fd);

    // -------------- 3. 监听socket，处理相关事务 ---------------
    while(1){
        // -------------- 先发数据 -----------------
        int ret = -1;
        const int input_size = 512;
        char input[input_size];
        memset(input, '\0', input_size);
        printf("please input something: ");
        std::cin.getline(input, input_size);
        if(strcmp(input, "quit") == 0) break;
        ret = send(tcp_fd, input, input_size - 1, 0);    // tcp 发送
        assert(ret != -1);
        printf("[TCP send] %d bytes.\n", ret);
        ret = sendto(udp_fd, input, input_size -1 , 0, (sockaddr*)&server_address, server_addr_len);     // udp 发送
        assert(ret != -1);
        printf("[UDP sendto] %d bytes.\n", ret);

        // -------------- 再接收服务器回射的数据 -------------------
        int num_ready = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1);
        if(num_ready < 0){
            printf("epoll failure.\n");
            break;
        }

        for(int i = 0; i < num_ready; i++){
            int sock_fd = events[i].data.fd;

            // case 1: TCP
            if(sock_fd == tcp_fd && (events[i].events & EPOLLIN)){
                char tcp_buf[TCP_BUFFER_SIZE];
                while(1){
                    memset(tcp_buf, '\0', TCP_BUFFER_SIZE);
                    ret = recv(sock_fd, tcp_buf, TCP_BUFFER_SIZE - 1, 0);
                    if(ret < 0){
                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            break;
                        }
                        close(sock_fd);
                        break;
                    }
                    else if (ret == 0) close(sock_fd);
                    else{
                        printf("[TCP receive]: %s\n", tcp_buf);
                    }
                }
            }

            // case 2: UDP
            else if(sock_fd == udp_fd && (events[i].events & EPOLLIN)){
                char udp_buf[UDP_BUFFER_SIZE];
                memset(udp_buf, '\0', UDP_BUFFER_SIZE);
                sockaddr_in address{};
                socklen_t addr_len = sizeof address;
                ret = recvfrom(udp_fd, udp_buf, UDP_BUFFER_SIZE-1, 0, (sockaddr*)&address, &addr_len);
                if(ret < 0){
                    printf("[UDP recvfrom] error: %s\n", gai_strerror(errno));
                }else if(ret > 0){
                    printf("[UDP receive]: %s\n", udp_buf);
                }
            }
        }
    }

    close(tcp_fd);

    return 0;
}

















