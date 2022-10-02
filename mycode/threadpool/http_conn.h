//
// Created by chen on 2022/9/30.
//

// HTTP请求解析

#ifndef LINUX_HIGH_PERFORMANCE_SERVER_HTTP_CONN_H
#define LINUX_HIGH_PERFORMANCE_SERVER_HTTP_CONN_H

#include "../locker/locker.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>

class http_conn{
public:
    static const int FILENAME_LEN = 200;            // 文件名的最大长度
    static const int READ_BUFFER_SIZE = 2048;       // 读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024;      // 写缓冲区大小

    // http请求方法，但这里只支持GET方法
    enum METHOD{
        GET = 0,
        POST, HEAD, PUT, DELETE,
        TRACE, OPTIONS, CONNECT, PATCH
    };

    // 解析客户请求时，主状态及所处的状态
    enum CHECK_STATE{
        CHECK_STATE_REQUESTLINE,    // 正在分析请求行
        CHECK_STATE_HEADER,         // 正在分析头部字段
        CHECK_STATE_CONTENT         // 读取消息体
    };

    // 从状态机的状态（行的读取状态）
    enum LINE_STATE{
        LINE_OK,        // 读取到一个完整的行
        LINE_BAD,       // 行出错
        LINE_OPEN       // 行数据尚不完整
    };

    // 服务器处理HTTP请求的结果
    enum HTTP_CODE{
        NO_REQUEST,             // 请求不完整，需要继续读取客户数据
        GET_REQUEST,            // 获得一个完整的客户请求
        BAD_REQUEST,            // 请求的语法出错
        NO_RESOURCE,            // 资源不存在
        FORBIDDEN_REQUEST,      // 无权限
        FILE_REQUEST,           // 请求文件成功
        INTERNAL_ERROR,         // 服务器内部错误
        CLOSED_CONNECTION       // 客户端已关闭连接
    };

public:
    http_conn() = default;
    ~http_conn() = default;

    void init(int sockfd, const sockaddr_in &addr);     // 初始化新接受的连接
    void close_conn(bool real_close = true);            // 关闭连接
    void process();                                     // 处理客户请求
    bool read();                                        // 非阻塞读
    bool write();                                       // 非阻塞写

private:
    void init();                            // 初始化连接
    HTTP_CODE process_read();               // 解析HTTP请求
    bool process_write(HTTP_CODE ret);      // 填充HTTP应答

    // ------------- HTTP请求分析辅助函数 ------------------
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line();
    LINE_STATE parse_line();

    // ------------- 填充HTTP应答的辅助函数 ----------------
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    // 所有socket上的事件都被注册到同一个epoll实例上，所以将epoll文件描述符设置为静态的
    static int m_epollfd;
    static int m_user_count;    // 统计用户数量

private:
    int m_sockfd;               // HTTP连接的socket
    sockaddr_in m_address;      // 对方的socket地址

    char m_read_buf[READ_BUFFER_SIZE];          // 读缓冲区
    int m_read_idx;                             // 指向读缓冲区中有效字节的下一个位置
    int m_checked_idx;                          // 当前正在分析的字符下标
    int m_start_line;                           // 当前正在解析的行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE];        // 写缓冲区
    int m_write_idx;                            // 写缓冲区中待发送的字节数

    METHOD m_method;            // 请求方法
    CHECK_STATE m_check_state;  // 主状态其当前所处的状态

    /*
     *  客户请求的目标文件完整路径： m_real_file = doc_root + m_url
     *  doc_root 是网站根目录
     */
    char m_real_file[FILENAME_LEN];
    char *m_url;                            // 客户请求目标文件的文件名
    char *m_version;                        // HTTP版本。仅支持HTTP/1.1
    char *m_host;                           // 主机名
    int m_content_length;                   // HTTP请求的消息体长度
    bool m_linger;                          // HTTP请求是否要求保持连接

    char *m_file_address;                   // 客户请求的目标文件被mmap到内存的其实位置
    struct stat m_file_stat;                // 目标文件状态：文件是否存在；是否为目录；是否可读；获取文件大小...

    // writev系统调用的参数
    struct iovec m_iv[2];   // 写内存块
    int m_iv_count;         // 内存块数量
};

#endif //LINUX_HIGH_PERFORMANCE_SERVER_HTTP_CONN_H
