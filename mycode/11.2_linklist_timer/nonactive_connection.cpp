//
// Created by chen on 2022/9/20.
//

// 服务器端检测非活动连接，并主动关闭这些连接

#include "./list_timer.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/signal.h>
#include <cassert>
#include <cstdlib>

#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int pipe_fd[2];
static sort_timer_list timer_list;
static int epoll_fd = 0;

// 设置socket为非阻塞模式
int set_nonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 添加fd到epoll实例
void add_fd(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

/*
 *  统一事件源：
 *  1. 将所有信号与sig_handler函数绑定
 *  2. 此函数将信号值写入到管道
 *  3. epoll监听到管道的可读事件，将信号值读出，然后做出相应的反应
 */
void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(pipe_fd[1], (char *)&msg, 1, 0);   // 往管道写端写入信号值
    errno = save_errno;
}

// 添加信号
void add_sig(int sig){
    struct sigaction sa;
    memset(&sa, '\0', sizeof sa);
    sa.sa_handler = sig_handler;    // 将信号sig和处理函数sig_handler绑定
    sa.sa_flags |= SA_RESTART;      // flag: 重新调用被该信号终止的系统调用
    sigfillset(&sa.sa_mask);        // 处理信号时屏蔽所有信号
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 调用alarm()来定时触发信号SIGALRM，继而定时处理到期任务
void timer_handler(){
    timer_list.tick();
    alarm(TIMESLOT);
}

// 定时器回调函数：删除非活动连接socket上的注册事件，然后关闭它
void cb_func(client_data *user_data){
    if(user_data == nullptr) return;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, user_data->sock_fd, nullptr);
    close(user_data->sock_fd);
    printf("close fd %d. \n", user_data->sock_fd);
}

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage: %s ip_address port_number \n", basename(argv[0]));
        return 1;
    }

    // ----------------- 1. 初始化监听socket --------------------
    const char *ip = argv[1];
    const int port = atoi(argv[2]);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listen_fd >= 0);
    int ret = bind(listen_fd, (sockaddr*)&address, sizeof address);
    assert(ret != -1);
    ret = listen(listen_fd, 5);
    assert(ret != -1);

    // ---------------- 2. 初始化epoll实例，监听listen_fd和管道；将信号和处理函数绑定 ------------
    epoll_event events[MAX_EVENT_NUMBER];
    epoll_fd = epoll_create(5);
    assert(epoll_fd != -1);
    add_fd(epoll_fd, listen_fd);

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipe_fd);
    assert(ret != -1);
    set_nonblocking(pipe_fd[1]);    // 管道写端非阻塞
    add_fd(epoll_fd, pipe_fd[0]);   // 监听管道可读事件

    add_sig(SIGALRM);
    add_sig(SIGTERM);

    // ---------------- 3. 监听和响应各类事件 --------------------------
    bool stop_server = false;
    client_data *users = new client_data[FD_LIMIT];
    bool timeout = false;
    alarm(TIMESLOT);        // 开始定时

    while(!stop_server){
        int ready_number = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1);
        if(ready_number < 0 && errno != EINTR){
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < ready_number; i++){
            int sock_fd = events[i].data.fd;

            // case 1: 监听到新的客户端连接
            if(sock_fd == listen_fd){
                sockaddr_in client_address{};
                socklen_t client_len = sizeof client_address;
                int conn_fd = accept(listen_fd, (sockaddr*)&client_address, &client_len);
                add_fd(epoll_fd, conn_fd);
                users[conn_fd].address = client_address;
                users[conn_fd].sock_fd = conn_fd;

                // 创建定时器，设置超时时间和回调函数，然后绑定用户数据
                // 最后加入到定时器链表
                util_timer *timer = new util_timer;
                timer->user_data = &users[conn_fd];
                timer->cb_func = cb_func;
                time_t cur = time(nullptr);
                timer->expire = cur + 3 * TIMESLOT;     // 超时时长是3被TIMESLOT
                users[conn_fd].timer = timer;
                timer_list.add_timer(timer);
            }

            // case 2: 处理信号
            else if(sock_fd == pipe_fd[0] && (events[i].events & POLL_IN)){
                char signals[1024];
                ret = recv(pipe_fd[0], signals, sizeof signals, 0);
                if(ret == -1){
                    printf("recv error: %s\n", gai_strerror(errno));
                    continue;
                }else if(ret > 0){
                    for(int j = 0; j < ret; j++){
                        // 对于定时信号，由于这里的定时任务并非十分重要，
                        // 这里只是标记有定时任务需要处理，但是并非现在处理
                        if(signals[j] == SIGALRM){
                            timeout = true;
                            break;
                        }
                        else if(signals[j] == SIGTERM){
                            stop_server = true;
                        }
                    }
                }
            }

            // case 3: 接收客户端数据
            else if (events[i].events & POLL_IN){
                memset(users[sock_fd].buf, '\0', BUFFER_SIZE);
                ret = recv(sock_fd, users[sock_fd].buf, BUFFER_SIZE - 1, 0);
                util_timer *timer = users[sock_fd].timer;

                // 发生读错误， 则关闭连接，并移除其对应的定时器
                if(ret < 0){
                    if(errno != EAGAIN){
                        cb_func(&users[sock_fd]);    // 删除epoll注册事件，关闭连接
                        if(timer){
                            timer_list.del_timer(timer);
                        }
                    }
                }

                // 如果对方已经关闭连接，服务器也关闭连接，移除定时器
                else if(ret == 0){
                    cb_func(&users[sock_fd]);
                    if(timer) timer_list.del_timer(timer);
                }

                // 如果有数据可读，则重设定时器超时时间
                else{
                    printf("receive %d bytes data from fd %d: %s\n", ret, sock_fd, users[sock_fd].buf);
                    if(timer){
                        time_t cur = time(nullptr);
                        timer->expire = cur + 3 * TIMESLOT;
                        printf("adjust timer\n");
                        timer_list.adjust_timer(timer);
                    }
                }
            }
        }

        // 最后才处理定时事件
        if(timeout){
            timer_handler();    // 处理超时连接；再次调用定时函数alarm()
            timeout = false;
        }
    }

    close(listen_fd);
    close(pipe_fd[1]);
    close(pipe_fd[0]);
    delete [] users;
    return 0;
}

















