//
// Created by chen on 2022/9/17.
//

/*
 *   网络聊天室服务器端
 *
 *   主要功能：
 *   1. 接收客户端数据，并把客户数据发送给每一位登录到该服务器的客户端
 *
 */

#define _GNU_SOURCEC 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#define USER_LIMIT 20       //  最大用户数量
#define BUFFER_SIZE 512
#define FD_LIMIT 65535      // 最大文件描述符

// 客户端信息
struct client_data{
    sockaddr_in address;
    char *write_buf;
    char buf[BUFFER_SIZE];
};

int set_nonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    // ----------- 1. 初始化服务器端聊天程序的socket --------------
    const char *ip = argv[1];
    const int port = atoi(argv[2]);

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_address.sin_addr);

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listen_fd >= 0);

    int ret = bind(listen_fd, (sockaddr*)&server_address, sizeof server_address);
    assert(ret != -1);

    ret = listen(listen_fd, USER_LIMIT);
    assert(ret != -1);

    // ------------- 2. 初始化pollfd, 包括 1个 listen_fd，以及 USER_LIMIT 个与客户端连接的fd ---------------------
    client_data *users = new client_data[FD_LIMIT];
    pollfd fds[USER_LIMIT + 1];

    fds[0].fd = listen_fd;
    fds[0].events = POLLIN | POLLERR;   // 监听数据可读以及出错
    fds[0].revents = 0;

    for(int i = 1; i <= USER_LIMIT; ++i){
        fds[i].fd = -1;
        fds[i].events = 0;
    }

    // -------------- 3. 监听fds, 处理相关事务 --------------
    int user_counter = 0;   // 当前系统中的用户数量
    while(1){
        ret = poll(fds, user_counter+1, -1);
        if(ret < 0){
            printf("poll failure.\n");
            break;
        }

        for(int i = 0; i < user_counter + 1; i++){

            // case 1: 监听socket上有可读数据，即有新的客户端请求连接
            // 如果当前用户数量已经达到上限，就告知该客户端
            // 否则把用户添加进来，做好相关记录
            if(fds[i].fd == listen_fd && (fds[i].revents & POLLIN)){
                sockaddr_in client_address{};
                socklen_t client_addrlen = sizeof client_address;
                int connfd = accept(listen_fd, (sockaddr*)&client_address, &client_addrlen);
                if(connfd < 0){
                    printf("error(accept): %s\n", gai_strerror(errno));
                    continue;
                }
                if(user_counter >= USER_LIMIT){
                    const char *msg = "too many users, please wait.\n";
                    printf("there are too many users. reject a connection.\n");
                    send(connfd, msg, strlen(msg), 0);
                    close(connfd);
                    continue;
                }
                // 添加新用户
                user_counter++;
                users[connfd].address = client_address;
                set_nonblocking(connfd);
                fds[user_counter].fd = connfd;
                fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_counter].revents = 0;
                printf("comes a new user. current amount of user: %d\n", user_counter);
            }

            // case 2: fd出错
            // 获取并处理该错误
            else if(fds[i].revents & POLLERR){
                printf("get an error from fd #%d", fds[i].fd);
                char errors[100];
                memset(errors, '\0', 100);
                socklen_t len = sizeof errors;
                if(getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &len) < 0){
                    printf("\nget socket option failed.\n");
                }else{
                    printf(": %s\n", errors);
                }
                continue;
            }

            // case 3: 客户端断开连接
            else if(fds[i].revents & POLLRDHUP){
                users[fds[i].fd].address = {0};
                users[fds[i].fd].write_buf = NULL;
                memset(users[fds[i].fd].buf, '\0', BUFFER_SIZE);
                close(fds[i].fd);
                fds[i] = fds[user_counter];
                i--;
                user_counter--;
                printf("a client left.\n");
            }

            // case 4: 客户端发来消息
            else if(fds[i].revents & POLLIN){
                int connfd = fds[i].fd;
                memset(users[connfd].buf, '\0', BUFFER_SIZE);
                ret = recv(connfd, users[connfd].buf, BUFFER_SIZE - 1, 0);
                if(ret < 0){    // 读取出错，关闭连接
                    if(errno != EAGAIN){
                        close(connfd);
                        users[fds[i].fd].address = {0};
                        users[fds[i].fd].write_buf = NULL;
                        memset(users[fds[i].fd].buf, '\0', BUFFER_SIZE);
                        fds[i] = fds[user_counter];
                        i--;
                        user_counter--;
                    }
                }else if(ret > 0){
                    printf("receive %d bytes from socket %d: \" %s \"\n", ret, connfd, users[connfd].buf);

                    // 告诉连接其它用户的socket准备写数据
                    for(int j = 1; j <= user_counter; j++){
                        if(fds[j].fd != connfd){
                            fds[j].events |= POLLOUT;
                            fds[j].events |= ~POLLOUT;
                            users[fds[j].fd].write_buf = users[connfd].buf;
                        }
                    }
                }
            }

            // case 5: 发送消息给客户端
            else if(fds[i].revents & POLLOUT){
                int connfd = fds[i].fd;
                if(!users[connfd].write_buf) continue;
                ret = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
                users[connfd].write_buf = NULL;
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;        // 已经写完了，暂时不用监听可写事件了
            }
        }
    }

    delete [] users;
    close(listen_fd);
    return 0;
}


//  ./linux_high_performance_server 10.0.20.4 12345
//  ./chatting_room 114.132.59.100 12345
















