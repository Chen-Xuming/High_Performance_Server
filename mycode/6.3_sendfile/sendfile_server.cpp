//
// Created by chen on 2022/9/11.
//

// 服务器端：使用sendfile函数传输文件给客户端

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

int main(){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    const char *ip = "10.0.20.4";
    const int port = 44444;

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);

    int ret = bind(sock, (sockaddr*)&server_addr, sizeof(server_addr));
    assert(ret != -1);

    ret = listen(sock, 5);
    assert(ret != -1);

    sockaddr_in client{};
    socklen_t client_len = sizeof (client);
    int connfd = accept(sock, (sockaddr*)&client, &client_len);

    if(connfd < 0){
        printf("error: %s", gai_strerror(errno));
    }else{
        // 打开文件
        const char *file_name = "../mycode/6.3_sendfile/a_letter.txt";
        int file_fd = open(file_name, O_RDONLY);
        if(file_fd < 0){
            printf("open file %s failed.\n", file_name);
            printf("error: %s\n", gai_strerror(errno));
            fflush(stdout);
            return -1;
        }

        // 通过文件描述符获取文件的属性
        struct stat stat_buf{};
        fstat(file_fd, &stat_buf);

        // 传送文件
        ret = sendfile(connfd, file_fd, NULL, stat_buf.st_size);    // 传输整个文件
        if(ret < 0){
            printf("sendfile failed.\n");
            close(file_fd);
            return -1;
        }else{
            printf("sendfile successfully!\n");
        }
        close(file_fd);
    }

    close(connfd);
    return 0;
}