//
// Created by chen on 2022/9/12.
//

// 回射服务器：将客户端发来的数据返回给客户端

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int main(){
    const char *ip = "10.0.20.4";
    const int port = 44444;

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(port);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    int ret = bind(sock, (sockaddr*)&server_addr, sizeof server_addr);
    assert(ret != -1);

    ret = listen(sock, 5);
    assert(ret != -1);

    sockaddr_in client{};
    socklen_t client_length = sizeof client;
    int connfd = accept(sock, (sockaddr*)&client, &client_length);
    if(connfd < 0){
        printf("connection failed. error: %s", gai_strerror(errno));
    }else{
        int pipefd[2];
        ret = pipe(pipefd);

        // 将connfd上的数据（客户端发来的数据）定向到管道写端
        ret = splice(connfd, NULL, pipefd[1], NULL, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
        assert(ret != -1);
        printf("receive %d bytes\n", ret);

        // 将管道的数据又重新定向到connfd上
        ret = splice(pipefd[0], NULL, connfd, NULL, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
        assert(ret != -1);
        printf("re-send %d bytes\n", ret);
        close(connfd);
    }

    close(sock);
    return 0;
}