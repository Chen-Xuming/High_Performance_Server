//
// Created by chen on 2022/9/30.
//

#include "cgi_conn.h"
#include "processpool.h"

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    const int port = atoi(argv[2]);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = 0;
    sockaddr_in address{};
    address.sin_family = PF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    ret = bind(listenfd, (sockaddr*)&address, sizeof address);
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    processpool<cgi_conn> *pool = processpool<cgi_conn>::create(listenfd, 8);
    if(pool){
        pool->run();
        delete pool;
    }

    close(listenfd);
    return 0;
}
