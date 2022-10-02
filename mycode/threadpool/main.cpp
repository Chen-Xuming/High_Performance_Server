//
// Created by chen on 2022/9/30.
//

#include "threadpool.h"
#include "http_conn.h"
#include "../locker/locker.h"

#include <netdb.h>

#define MAX_FD 65536
#define MAX_EVETN_NUMBER 1000

extern int add_fd(int epollfd, int fd, bool one_shot);
extern void removefd(int epollfd, int fd);

void add_sig(int sig, void(*handler)(int), bool restart = true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof sa);
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr) != -1);
}

void show_error(int connfd, const char *info){
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    const int port = atoi(argv[2]);

    threadpool<http_conn> *pool = nullptr;
    try {
        pool = new threadpool<http_conn>;
    }catch (...){
        printf("some error occur.\n");
        return 1;
    }

    http_conn *users = new http_conn[MAX_FD];
    assert(users);
    int user_count = 0;

    int ret = 0;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    linger tmp = {1, 0};
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof address);

    ret = bind(listenfd, (sockaddr*)&address, sizeof address);
    assert(ret >= 0);

    ret = listen(listenfd, 5);
    assert(ret >= 0);

    epoll_event events[MAX_EVETN_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    add_fd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    while(true){
        int number = epoll_wait(epollfd, events, MAX_EVETN_NUMBER, -1);
        if(number < 0 && errno != EINTR){
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < number; i++){
            int sockfd = events[i].data.fd;

            // case 1: 新连接
            if(sockfd == listenfd){
                sockaddr_in client_address{};
                socklen_t client_addr_len = sizeof client_address;
                int connfd = accept(listenfd, (sockaddr*)&client_address, &client_addr_len);
                if(connfd < 0){
                    printf("errer: %s\n", gai_strerror(errno));
                    continue;
                }
                if(http_conn::m_user_count >= MAX_FD){
                    show_error(connfd, "Server busy.");
                    continue;
                }
                users[connfd].init(connfd, client_address);
            }

            // case 2: 如果有异常直接关闭连接
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                users[sockfd].close_conn();
            }

            // case 3: 处理客户请求
            else if(events[i].events & EPOLLIN){
                if(users[sockfd].read()){
                    pool->append(users + sockfd);
                }else{
                    users[sockfd].close_conn();
                }
            }

            // case 4: 发送响应报文
            else if(events[i].events & EPOLLOUT){
                if(!users[sockfd].write()){
                    users[sockfd].close_conn();
                }
            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;
    return 0;
}















