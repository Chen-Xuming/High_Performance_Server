//
// Created by chen on 2022/9/30.
//

#include "http_conn.h"
#include <sys/uio.h>

// ---------------- HTTP 响应的一些状态信息 ------------------------
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.";

// 网站根目录
const char* doc_root = "/tmp/http_test";

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

// ---------------------------------- 一些工具函数 ----------------------------------------
int set_nonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void add_fd(int epollfd, int fd, bool one_shot){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if(one_shot){
        event.events |= EPOLLONESHOT;       // 一个socket连接在任意时刻只能被一个线程处理
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_nonblocking(fd);
}

void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

void modfd(int epollfd, int fd, int ev){
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

char * http_conn::get_line(){
    return m_read_buf + m_start_line;
}

// ------------------------------------------------------------------------------------

/*
 *      关闭连接
 */
void http_conn::close_conn(bool real_close) {
    if(real_close && (m_sockfd != -1)){
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

/*
 *      初始化新接受的连接
 */
void http_conn::init(int sockfd, const sockaddr_in &addr) {
    m_sockfd = sockfd;
    m_address = addr;

    // 这两行为了避免TIME_WAIT状态，仅用于调试
    // int reuse = 1;
    // setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);

    add_fd(m_epollfd, sockfd, true);
    m_user_count++;
    init();
}

void http_conn::init() {
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = nullptr;
    m_version = nullptr;
    m_content_length = 0;
    m_host = nullptr;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

// ----------------------------------- HTTP 报文解析 -----------------------------------------

/*
 *      头部结构
 *      请求行： method | SP | URL | SP | Version | CRLF
 *      请求头： Field Name | .. | Field Value | CRLF
 *              ...
 *      空行结束：CRLF
 *
 *      示例
 *      GET http://www.baidu.com/index/html HTTP/1.0
 *      User-Agent: Wget/1.12 (linux-gnu)
 *      Host: www.baidu.com
 *      Connection: close
 *
 */

/*
 *      从状态机：用于解析出一行内容
 *      读到CRLF(\r\n)时返回LINE_OK，表示读完一行
 */
http_conn::LINE_STATE http_conn::parse_line() {
    char temp;
    for(; m_checked_idx < m_read_idx; ++m_checked_idx){
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r'){
            if((m_checked_idx + 1) == m_read_idx){
                return LINE_OPEN;   // 需要读更多数据
            }else if(m_read_buf[m_checked_idx] + 1 == '\n'){
                // 用'\0'替代\r\n
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
        }

        else if(temp == '\n'){
            if( m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r' ){
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }

    return LINE_OPEN;
}

/*
 *      解析请求行：获得请求方法、目标URL、HTTP版本号
 */
http_conn::HTTP_CODE http_conn::parse_request_line(char *text) {
    // 找到第一个空格或\t的位置, 即SP位置
    m_url = strpbrk(text, " \t");
    if(!m_url){
        return BAD_REQUEST;
    }
    *m_url++ = '\0';    // 用\0代替SP

    // 本程序只接受GET方法的请求
    char *method = text;
    if(strcasecmp(method, "GET") == 0){
        m_method = GET;
    }else{
        return BAD_REQUEST;
    }

    m_url += strspn(m_url, " \t");  // 此时指向url起始位置

    m_version = strpbrk(m_url, " \t");  // 指向第二个SP
    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");   // 指向version起始位置
    if(strcasecmp(m_version, "HTTP/1.1") != 0){     // 本程序仅支持HTTP/1.1
        return BAD_REQUEST;
    }

    // url合法检查
    if(strncasecmp(m_url, "http://", 7) == 0){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if(!m_url || m_url[0] != '/'){
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER;     // 请求行解析完毕。下面分析请求头
    return NO_REQUEST;                      // 需要读取更多数据
}

/*
 *      解析HTTP请求的一个头部信息（请求头的一行）
 */
http_conn::HTTP_CODE http_conn::parse_headers(char *text) {
    // 遇到空行表示头部分析完毕
    if(text[0] == '\0'){
        // 如果请求还有消息体，那么还要读取消息
        if(m_content_length != 0){
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }

    // 处理connection字段
    else if(strncasecmp(text, "Connection:", 11) == 0){
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0){
            m_linger = true;
        }
    }

    // 处理Content-Length字段
    else if(strncasecmp(text, "Content-Length:", 15) == 0){
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }

    // 处理Host字段
    else if(strncasecmp(text, "Host:", 5) == 0){
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }

    return NO_REQUEST;
}

/*
 *      读取消息体
 *      本程序只是判断消息体是否被完整地读完，并不真正解析消息体
 */
http_conn::HTTP_CODE http_conn::parse_content(char *text) {
    if(m_read_idx >= (m_content_length + m_checked_idx)){
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

/*
 *      主状态机
 */
http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATE line_state = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = nullptr;

    while((m_check_state == CHECK_STATE_CONTENT && line_state == LINE_OK) || (line_state = parse_line()) == LINE_OK){
        text = get_line();
        m_start_line = m_checked_idx;

        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST) return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret = parse_headers(text);
                if(ret == BAD_REQUEST) return BAD_REQUEST;          // false
                else if(ret == GET_REQUEST) return do_request();
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content(text);
                if(ret == GET_REQUEST) return do_request();
                line_state = LINE_OPEN;
                break;
            }
            default: return INTERNAL_ERROR;
        }
    }

    return NO_REQUEST;
}


// -----------------------------------------------------------------------------------------

/*
 *      当得到一个完整、正确地HTTP请求时，就分析文件属性。
 *      如果文件存在、不是目录且可读，那么将其映射到内存地址m_file_address
 */
http_conn::HTTP_CODE http_conn::do_request() {
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len -1);

    // 文件不存在
    if(stat(m_real_file, &m_file_stat) < 0){
        return NO_RESOURCE;
    }

    // 无权限
    if(!(m_file_stat.st_mode & S_IROTH)){       //  S_IROTH = 04, read permission
        return FORBIDDEN_REQUEST;
    }

    // 路径是一个目录
    if(S_ISDIR(m_file_stat.st_mode)){
        return BAD_REQUEST;
    }

    // 打开文件然后将其映射到m_file_address
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *) mmap(nullptr, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

/*
 *      释放内存映射区的申请空间
 */
void http_conn::unmap() {
    if(m_file_address){
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = nullptr;
    }
}

/*
 *      往缓冲区写入待发送的格式化数据
 */
bool http_conn::add_response(const char *format, ...) {
    if(m_write_idx >= WRITE_BUFFER_SIZE){
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);     // 使arg_list指向第一个可变参数的地址
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 * m_write_idx, format, arg_list);  // 将可变参数格式化输出到一个字符数组
    if(len >= WRITE_BUFFER_SIZE - 1 - m_write_idx) return false;
    m_write_idx += len;
    va_end(arg_list);       // 结束可变参数列表获取
    return true;
}

bool http_conn::add_status_line(int status, const char *title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_headers(int content_length) {
    return add_content_length(content_length) && add_linger() && add_blank_line();
}

bool http_conn::add_content_length(int content_length) {
    return add_response("Content-Length: %d\r\n", content_length);
}

bool http_conn::add_linger() {
    return add_response("Connection: %s\r\n", m_linger ? "keep-alive" : "close");
}

bool http_conn::add_blank_line() {
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char *content) {
    return add_response("%s", content);
}

/*
 *      根据HTTP请求的结果，决定返回内容
 */
bool http_conn::process_write(HTTP_CODE ret) {
    switch (ret) {
        case INTERNAL_ERROR:
        {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form)) return false;
            break;
        }

        case BAD_REQUEST:
        {
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if(!add_content(error_400_form)) return false;
            break;
        }

        case NO_RESOURCE:
        {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form)) return false;
            break;
        }

        case FORBIDDEN_REQUEST:
        {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form)) return false;
            break;
        }

        case FILE_REQUEST:
        {
            add_status_line(200, ok_200_title);
            if(m_file_stat.st_size != 0){
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                return true;
            }else{
                const char *ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string)) return false;
            };
            break;
        }

        default: return false;
    }

    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

/*
 *      处理HTTP请求的入口函数，由工作线程调用
 */
void http_conn::process() {
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST){
        modfd(m_epollfd, m_sockfd, EPOLLIN);    // 下一轮继续读
        return;
    }

    bool write_ret = process_write(read_ret);
    if(!write_ret){
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}


/*
 *      非阻塞读
 *      循环读取客户端数据，直到无数据可读或者对方关闭连接
 */
bool http_conn::read() {
    if(m_read_idx > READ_BUFFER_SIZE){
        return false;
    }

    int bytes_read = 0;
    while(true){
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if(bytes_read == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }
            return false;
        }else if(bytes_read == 0){
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
}

/*
 *      非阻塞写
 *      写HTTP响应
 */
bool http_conn::write() {
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    if(bytes_to_send == 0){
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while(1){
        int temp = writev(m_sockfd, m_iv, m_iv_count);
        if(temp <= -1){
            // 缓冲区没有足够空间则下一轮再写
            if(errno == EAGAIN){
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
//        if(bytes_to_send <= bytes_have_send){     // ?
        if(bytes_have_send >= m_write_idx){
            unmap();
            if(m_linger){
                init();
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            }else{
                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return false;
            }
        }
    }
}

