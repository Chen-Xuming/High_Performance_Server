//
// Created by chen on 2022/9/8.
//

// 研究backlog参数对listen系统调用的影响

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool stop = false;

// SIGTERM信号的处理函数，触发时结束主程序中的循环
// SIGTERM => terminate 进程终止
static void handle_term(int sig){
    stop = true;
}

int main(int argc, char *argv[]){
    signal(SIGTERM, handle_term);
    if(argc <= 3){
        printf("usage: %s ip_address port_number backlog\n", basename(argv[0]));
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int backlog = atoi(argv[3]);

    // 创建一个 ipv4 流服务的socket，协议是 TCP
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    // 创建一个 ipv4 socket地址
    sockaddr_in address{};
    bzero(&address, sizeof (address));  // 全部字节置零
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);  // 点分十进制 ---> 网络字节序
    address.sin_port = htons(port);

    // 将socket和socket地址绑定
    int ret = bind(sock, (struct sockaddr*)&address, sizeof (address));
    assert(ret != -1);

    ret = listen(sock, backlog);
    assert(ret != -1);

    // 循环等待连接，直到收到进程终止信号
    while(!stop){
        sleep(1);
    }

    // 关闭socket
    close(sock);

    return 0;
}