//
// Created by chen on 2022/9/30.
//

#ifndef LINUX_HIGH_PERFORMANCE_SERVER_CGI_CONN_H
#define LINUX_HIGH_PERFORMANCE_SERVER_CGI_CONN_H

#include "processpool.h"

class cgi_conn{
public:
    cgi_conn() = default;
    ~cgi_conn() = default;

    void init(int epollfd, int sockfd, const sockaddr_in &client_addr){
        m_epollfd = epollfd;
        m_sockfd = sockfd;
        m_address = client_addr;
        memset(m_buf, '\0', BUFFER_SIZE);
        m_read_idx = 0;
    }

    void process(){
        int idx = 0;
        int ret = -1;
        while(true){
            idx = m_read_idx;
            ret = recv(m_sockfd, m_buf + idx, BUFFER_SIZE - 1 - idx, 0);
            // 如果读操作发生错误，则关闭客户连接。如果是暂时无数据可读，则退出循环
            if(ret < 0){
                if(errno != EAGAIN){
                    remove_fd(m_epollfd, m_sockfd);
                }
                break;
            }

            // 如果对方关闭连接，服务器也关闭连接
            else if(ret == 0){
                remove_fd(m_epollfd, m_sockfd);
                break;
            }

            else{
                m_read_idx += ret;
                printf("user content: %s \n", m_buf);
                // 遇到字符\r\n，则开始处理客户请求
                for(; idx < m_read_idx; ++idx){
                    if(idx >= 1 && (m_buf[idx-1] == '\r') && (m_buf[idx] == '\n')) {
                        printf("read \\r\\n.\n");
                        break;
                    }
                }
                // 如果没有遇到\r\n，则继续接收更多数据
                if(idx == m_read_idx){
                    printf("didn't read \\r\\n.\n");
                    continue;
                }
                m_buf[idx-1] = '\0';

                char *filename = m_buf;     // 可执行文件名
                printf("filename: %s\n", filename);
                // 判断客户要运行的CGI程序是否存在
                if(access(filename, F_OK) == -1){
                    printf("file %s not exists.\n", filename);
                    remove_fd(m_epollfd, m_sockfd);
                    break;
                }
                // 创建子进程来执行CGI程序
                ret = fork();
                if(ret == -1){
                    remove_fd(m_epollfd, m_sockfd);
                    break;
                }else if(ret > 0){
                    // 父进程只需要关闭连接
                    remove_fd(m_epollfd, m_sockfd);
                    break;
                }else{
                    // 子进程将标准输出定向到msockfd, 并执行CGI程序
                    close(STDOUT_FILENO);
                    dup(m_sockfd);
                    execl(filename, filename, 0);
//                    exit(0);
                }
            }
        }
    }

private:
    static const int BUFFER_SIZE = 1024;
    static int m_epollfd;
    int m_sockfd;
    sockaddr_in m_address;
    char m_buf[BUFFER_SIZE];
    int m_read_idx;     // 读缓冲区中已经读入的客户数据最后一个字节的下一个位置
};

int cgi_conn::m_epollfd = -1;


















#endif //LINUX_HIGH_PERFORMANCE_SERVER_CGI_CONN_H
