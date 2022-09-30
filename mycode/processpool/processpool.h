//
// Created by chen on 2022/9/29.
//

// 半同步/半异步进程池

#ifndef LINUX_HIGH_PERFORMANCE_SERVER_PROCESSPOOL_H
#define LINUX_HIGH_PERFORMANCE_SERVER_PROCESSPOOL_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

// 子进程类
class process{
public:
    pid_t m_pid;        // 子进程pid
    int m_pipefd[2];    // 和父线程通信的管道

    process(): m_pid(-1){}
};

// 进程池类，模板参数是处理逻辑任务的类
// 采用单例模式
template<class T>
class processpool{
public:
    static processpool<T>* create(int listenfd, int process_number = 8);    // 单例模式
    ~processpool(){};
    void run();         // 启动进程池

private:
    processpool(int listenfd, int process_number = 8);      // 通过private构造函数构建进程池单例实例

    void setup_sig_pipe();
    void run_parent();
    void run_child();

private:
    static const int MAX_PROCESS_NUMBER = 8;            // 进程池里最多的子进程个数
    static const int USER_PER_PROCESS = 65536;          // 每个子进程最多处理的客户数量(socket 最大值)
    static const int MAX_EVENT_NUMBER = 10000;          // epoll最大处理事件数

    int m_process_number;           // 进程池中的进程数量
    int m_idx;                      // 子进程序号，从0开始
    int m_epollfd;                  // 每个进程都有一个epoll实例
    int m_listenfd;                 // 监听socket
    int m_stop;                     // 决定子进程是否停止允许
    process *m_sub_process;         // 保存所有子进程信息

    static processpool<T> *m_instance;      // 进程池实例
};

template<class T>
processpool<T> *processpool<T>::m_instance = nullptr;

// ------------------------- 辅助函数 -----------------------------------

static int sig_pipefd[2];   // 信号管道，统一事件源

static int set_nonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

static void add_fd(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

static void remove_fd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

// (伪)信号处理函数，将信号通过管道转发，统一处理
static void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}


static void add_sig(int sig, void(*handler)(int), bool restart = true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof sa);
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr) != -1);
}

// ---------------------------------------------------------------------

template<class T>
processpool<T>* processpool<T>::create(int listenfd, int process_number){
    if(!m_instance){
        m_instance = new processpool<T>(listenfd, process_number);
    }
    return m_instance;
}

/*
 *  构造函数
 *  listenfd必须在创建进程池之前创建，否则子进程无法直接引用
 */
template<class T>
processpool<T>::processpool(int listenfd, int process_number):
        m_listenfd(listenfd), m_process_number(process_number), m_idx(-1), m_stop(false){

    assert(process_number > 0 && process_number <= MAX_PROCESS_NUMBER);
    m_sub_process = new process[process_number];
    assert(m_sub_process);

    // 创建子进程，并创建它们与父进程的管道
    for(int i = 0; i < process_number; i++){
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
        assert(ret == 0);
        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >= 0);
        if(m_sub_process[i].m_pid > 0){
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }else{
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;      // 如果没有这句，子进程又创建子进程，最终有 2 ^ process_number个进程
        }
    }
}

/*
 *      统一事件源
 */
template <class T>
void processpool<T>::setup_sig_pipe() {
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);

    set_nonblocking(sig_pipefd[1]);
    add_fd(m_epollfd, sig_pipefd[0]);   // 监听信号管道读端

    add_sig(SIGCHLD, sig_handler);      // 子进程终止或停止
    add_sig(SIGTERM, sig_handler);
    add_sig(SIGINT, sig_handler);
    add_sig(SIGPIPE, SIG_IGN);
}

/*
 *      子进程
 */
template<class T>
void processpool<T>::run_child() {
    setup_sig_pipe();

    // 父进程通过管道通知子进程accept新连接，所有子进程要监听相应的管道端口
    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    add_fd(m_epollfd, pipefd);

    epoll_event events[MAX_EVENT_NUMBER];
    T *users = new T[USER_PER_PROCESS];
    assert(users);
    int number = 0;     // 就绪事件数
    int ret = -1;

    while(!m_stop){
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR){
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < number; i++){
             int sockfd = events[i].data.fd;

             // case 1: 父进程通知子进程accept新连接
             if(sockfd == pipefd && (events[i].events & EPOLLIN)){
                 int client = 0;
                 ret = recv(sockfd, (char*)&client, sizeof (client), 0);
                 if((ret < 0 && errno != EAGAIN) || ret == 0){
                     continue;
                 }else{
                     sockaddr_in client_address;
                     socklen_t client_addrlen = sizeof client_addrlen;
                     int connfd = accept(m_listenfd, (sockaddr*)&client_address, &client_addrlen);  // 注：m_listenfd是父子进程共享的
                     if(connfd < 0){
                         printf("error: %s\n", gai_strerror(errno));
                         continue;
                     }
                     add_fd(m_epollfd, connfd);
                     users[connfd].init(m_epollfd, connfd, client_address); // 初始化客户连接，在cgi_conn类里定义
                 }
             }

             // case 2: 处理信号
             else if(sockfd == sig_pipefd[0] && (events[i].events & EPOLLIN)){
                 int sig;
                 char signals[1024];
                 ret = recv(sig_pipefd[0], signals, sizeof signals, 0);
                 if(ret <= 0){
                     continue;
                 }else{
                     for(int j = 0; j < ret; j++){
                         switch (signals[j]) {
                             case SIGCHLD:
                             {
                                 pid_t pid;
                                 int stat;
                                 while((pid = waitpid(-1, &stat, WNOHANG)) > 0) continue;
                                 break;
                             }
                             case SIGTERM: case SIGINT:
                             {
                                 m_stop = true;
                                 break;
                             }
                             default: break;
                         }
                     }
                 }
             }

             // case 3: 客户端发来数据
             else if(events[i].events & EPOLLIN){
                 users[sockfd].process();
             }
        }
    }

    delete [] users;
    users = nullptr;
    close(pipefd);
    close(m_epollfd);
}

/*
 *      父进程
 */
template<class T>
void processpool<T>::run_parent() {
    setup_sig_pipe();
    add_fd(m_epollfd, m_listenfd);

    epoll_event events[MAX_EVENT_NUMBER];
    int sub_process_counter = 0;
    int new_conn = 1;
    int ret = -1;
    int number = 0;

    while (!m_stop){
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR){
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < number; i++){
            int sockfd = events[i].data.fd;

            // case 1: 有新的客户端连接，则采取Round Robin方法（轮流）分发给某个子进程
            if(sockfd == m_listenfd){
                int index = sub_process_counter;
                do{
                    if(m_sub_process[index].m_pid != -1){
                        break;
                    }
                    index = (index + 1) % m_process_number;
                } while (index != sub_process_counter);

                if(m_sub_process[index].m_pid == -1){
                    m_stop = true;
                    break;
                }
                sub_process_counter = (index + 1) % m_process_number;
                send(m_sub_process[index].m_pipefd[0], (char*)&new_conn, sizeof new_conn, 0);
                printf("send new request to child process %d.\n", index);
            }

            // case 2: 处理信号
            else if(sockfd == sig_pipefd[0] && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof signals, 0);
                if(ret <= 0){
                    continue;
                }else{
                    for(int j = 0; j < ret; j++){
                        switch (signals[j]) {
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while((pid = waitpid(-1, &stat, WNOHANG)) > 0){
                                    // 如果子进程sub_pro退出，则主进程关闭相应的管道，并设置m_pid为-1
                                    for(int sub_pro = 0; sub_pro < m_process_number; ++sub_pro){
                                        if(m_sub_process[sub_pro].m_pid == pid){
                                            printf("child process %d join\n", pid);
                                            close(m_sub_process[sub_pro].m_pipefd[0]);
                                            m_sub_process[sub_pro].m_pid = -1;
                                        }
                                    }
                                }
                                // 如果所有子进程退出，父进程也退出
                                m_stop = true;
                                for(int p = 0; p < m_process_number; ++p){
                                    if(m_sub_process[i].m_pid != -1) m_stop = false;
                                }
                                break;
                            }

                            // 如果父进程收到终止信号，那么就杀死所有子进程。
                            case SIGTERM: case SIGINT:
                            {
                                printf("kill all the child process.\n");
                                for(int sub_pro = 0; sub_pro < m_process_number; ++sub_pro){
                                    int pid = m_sub_process[sub_pro].m_pid;
                                    if(pid != -1){
                                        kill(pid, SIGTERM);
                                    }
                                }
                                break;
                            }

                            default: break;
                        }
                    }
                }
            }

            else continue;
        }
    }

    close(m_epollfd);
}

/*
 *      进程池启动
 */
template<class T>
void processpool<T>::run() {
    if(m_idx != -1){        // 父进程中的m_idx = -1, 子进程 >= 0
        run_child();
        return;
    }
    run_parent();
}


#endif //LINUX_HIGH_PERFORMANCE_SERVER_PROCESSPOOL_H
