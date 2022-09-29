//
// Created by chen on 2022/9/27.
//

// 使用共享内存的聊天室（服务器端程序）
// 这段程序是有bug的：sub_process的size是PROCESS_LIMIT=65536，但是pid_t的类型是int，当使用 sub_process[pid]是很容易越界。

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/mman.h>   // for shm_open
#include <sys/stat.h>   // for shm_open
#include <signal.h>
#include <wait.h>

#define USER_LIMIT 5
#define BUFFER_SIZE 1024
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define PROCESS_LIMIT 65536

// 每个客户连接的必要数据结构
struct client_data{
    sockaddr_in address;
    int conn_fd;            // 连接socket
    pid_t pid;              // 处理该连接的子进程
    int pipe_fd[2];         // 与父进程通信的管道
};

const char *shm_name = "/my_shm";
int sig_pipe_fd[2];
int epoll_fd;
int listen_fd;
int shm_fd;
char *share_mem = nullptr;      // mmap() 返回的内存地址
client_data *users = nullptr;   // 用户数组
int *sub_process = nullptr;     // 子进程与客户连接的映射表，用PID进行索引
int user_count = 0;             // 当前用户数量
bool stop_child = false;        // 终止子进程标志

int set_nonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void add_fd(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(sig_pipe_fd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void add_sig(int sig, void(*handler)(int), bool restart = true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof sa);
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;  // 重新调用被该信号终止的系统调用
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 资源释放：关闭文件描述符，释放申请的空间
void del_resource(){
    close(sig_pipe_fd[0]);
    close(sig_pipe_fd[1]);
    close(listen_fd);
    close(epoll_fd);
    shm_unlink(shm_name);
    delete [] users;
    delete [] sub_process;
}

// 终止一个子进程
// 绑定的信号是SIGTERM
void child_term_handler(int sig){
    stop_child = true;
}

// 子进程函数。
// idx：要处理的客户连接的编号
// share_mem：共享内存起始地址
// 使用epoll同时监听：1. 客户连接socket  2. 与父进程通信的管道
int run_child(int idx, client_data *users, char *share_mem){
    epoll_event events[MAX_EVENT_NUMBER];
    int child_epoll_fd = epoll_create(1);
    assert(child_epoll_fd != -1);
    int connfd = users[idx].conn_fd;
    add_fd(child_epoll_fd, connfd);         // 监听连接socket
    int pipefd = users[idx].pipe_fd[1];
    add_fd(child_epoll_fd, pipefd);         // 监听管道（这里用的是双工管道）
    add_sig(SIGTERM, child_term_handler, false);

    int ret = -1;

    while(!stop_child){
        int number = epoll_wait(child_epoll_fd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR){
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < number; i++){
            int sockfd = events[i].data.fd;

            // case 1: 本子进程负责的客户连接有数据到达
            if(sockfd == connfd && (events[i].events & EPOLLIN)){
                memset(share_mem + idx * BUFFER_SIZE, '\0', BUFFER_SIZE);   // 所有客户的buffer合成一大块，每个客户拥有其中一块，大小为BUFFER_SIZE
                ret = recv(connfd, share_mem + idx * BUFFER_SIZE, BUFFER_SIZE - 1, 0);
                if(ret < 0){
                    if(errno != EAGAIN){
                        stop_child = true;
                    }
                }else if(ret == 0){
                    stop_child = true;
                } else{
                    // 成功读取客户数据之后，通知主进程来处理
                    send(pipefd, (char *)&idx, sizeof idx, 0);
                }
            }

            // case 2: 主进程通知子进程将第client个客户的数据发送到子进程负责的客户端
            else if(sockfd == pipefd && (events[i].events & EPOLLIN)){
                int client = 0;

                // 接收的一个客户编号
                ret = recv(sockfd, (char *)&client, sizeof client, 0);
                if(ret < 0){
                    if(errno != EAGAIN){
                        stop_child = true;
                    }
                }else if(ret == 0){
                    stop_child = true;
                }else{
                    send(connfd, share_mem + client * BUFFER_SIZE, BUFFER_SIZE, 0);
                }
            }

            else continue;
        }
    }

    close(connfd);
    close(pipefd);
    close(child_epoll_fd);
    return 0;
}


int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage: %s ip port\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listen_fd >= 0);

    int ret = bind(listen_fd, (sockaddr *)&address, sizeof address);
    assert(ret != -1);

    ret = listen(listen_fd, 5);
    assert(ret != -1);

    user_count = 0;
    users = new client_data[USER_LIMIT + 1];
    sub_process = new int[PROCESS_LIMIT];
    for(int i = 0; i < PROCESS_LIMIT; i++){
        sub_process[i] = -1;
    }

    epoll_event events[MAX_EVENT_NUMBER];
    epoll_fd = epoll_create(1);
    assert(epoll_fd != -1);
    add_fd(epoll_fd, listen_fd);    // 监听新的客户端连接

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipe_fd);
    assert(ret != -1);
    set_nonblocking(sig_pipe_fd[1]);
    add_fd(epoll_fd, sig_pipe_fd[0]);   // 监听各种信号

    add_sig(SIGCHLD, sig_handler);      // 子进程停止或终止
    add_sig(SIGTERM, sig_handler);      // 终止
    add_sig(SIGINT, sig_handler);       // 来自键盘的中断
    add_sig(SIGPIPE, SIG_IGN);          // 忽略管道错误
    bool stop_server = false;
    bool terminate = false;

    // 创建共享内存，作为所有客户socket连接的读缓存
    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    assert(shm_fd != -1);
    ret = ftruncate(shm_fd, USER_LIMIT * BUFFER_SIZE);      // 改变文件（共享内存）大小为USER_LIMIT * BUFFER_SIZE
    assert(ret != -1);

    share_mem = (char *)mmap(NULL, USER_LIMIT * BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    assert(share_mem != MAP_FAILED);
    close(shm_fd);

    while(!stop_server){
        int number = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0 && errno != EINTR){
            printf("epoll failure\n");
            break;
        }
        for(int i = 0; i < number; i++){
            int sockfd = events[i].data.fd;

            // case 1: 新的客户连接到来
            if(sockfd == listen_fd){
                sockaddr_in client_address;
                socklen_t client_addr_len = sizeof client_address;
                int connfd = accept(listen_fd, (sockaddr*)&client_address, &client_addr_len);
                if(connfd < 0){
                    printf("errno : %s\n", gai_strerror(errno));
                    continue;
                }
                if(user_count >= USER_LIMIT){
                    const char *info = "too many users, please wait.\n";
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }

                // 保存该用户的相关数据到users[user_count]
                // 并为其创建一组pipe，用于进程通信
                users[user_count].address = client_address;
                users[user_count].conn_fd = connfd;
                ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipe_fd);
                assert(ret != -1);

                // 创建一个子进程，用来监听、接收客户端users[user_count]数据，以及发送其它用户的消息给该用户
                pid_t pid = fork();
                if(pid < 0){
                    close(connfd);
                    continue;
                }else if(pid == 0){
                    // 父进程打开的文件描述符在子进程中还是会打开，所以手动关闭它
                    close(epoll_fd);
                    close(listen_fd);
                    close(users[user_count].pipe_fd[0]);
                    close(sig_pipe_fd[0]);
                    close(sig_pipe_fd[1]);
                    run_child(user_count, users, share_mem);
                    munmap((void *)share_mem, USER_LIMIT * BUFFER_SIZE);
                    exit(0);
                }else{
                    // 这两个fd在父进程中用不上了
                    close(connfd);
                    close(users[user_count].pipe_fd[1]);

                    add_fd(epoll_fd, users[user_count].pipe_fd[0]);
                    users[user_count].pid = pid;
                    sub_process[pid] = user_count;
                    user_count++;
                }
            }

            // case 2: 处理信号
            else if(sockfd == sig_pipe_fd[0] && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(sig_pipe_fd[0], signals, sizeof signals, 0);
                if(ret <= 0) continue;
                else{
                    for(int j = 0; j < ret; j++){
                        // 子进程退出，表示某个客户端关闭了连接
                        if(signals[j] == SIGCHLD){
                            pid_t pid;
                            int stat;
                            while((pid = waitpid(-1, &stat, WNOHANG)) > 0){     // 子进程正常退出
                                int del_user = sub_process[pid];
                                sub_process[pid] = -1;
                                if(del_user < 0 || del_user > USER_LIMIT) continue;
                                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, users[del_user].pipe_fd[0], nullptr);
                                close(users[del_user].pipe_fd[0]);
                                users[del_user] = users[--user_count];
                                sub_process[users[del_user].pid] = del_user;
                            }
                            if(terminate && user_count == 0){
                                stop_server = true;
                            }
                        }
                        // 服务器程序结束
                        else if(signals[j] == SIGTERM || signals[j] == SIGINT){
                            printf("kill all the child process.\n");
                            if(user_count == 0){
                                stop_server = true;
                                break;
                            }
                            for(int k = 0; k < user_count; k++){
                                int pid = users[k].pid;
                                kill(pid, SIGTERM);
                            }
                            terminate = true;
                            break;
                        }else break;
                    }
                }
            }

            // case 3: 某个子进程向父进程写入数据，那么通知其它用户的子进程有数据要发送
            else if(events[i].events & EPOLLIN){
                int child = 0;
                ret = recv(sockfd, (char*)&child, sizeof child, 0);
                if(ret <= 0) continue;
                else{
                    for(int j = 0; j < user_count; j++){
                        if(users[j].pipe_fd[0] != sockfd){
                            send(users[j].pipe_fd[0], (char *)&child, sizeof child, 0);
                        }
                    }
                }
            }
        }
    }

    del_resource();
    return 0;
}