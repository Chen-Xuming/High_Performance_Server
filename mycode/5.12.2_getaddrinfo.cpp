//
// Created by chen on 2022/9/10.
//

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

int main(){
    struct addrinfo* res = NULL;
    getaddrinfo("www.szu.edu.cn", NULL, NULL, &res);

    struct addrinfo* i;
    for(i=res; i!=NULL; i=i->ai_next)
    {
        char str[INET6_ADDRSTRLEN];
        if (i->ai_addr->sa_family == AF_INET) {
            struct sockaddr_in *p = (struct sockaddr_in *)i->ai_addr;
            printf("%s\n", inet_ntop(AF_INET, &p->sin_addr, str, sizeof(str)));
        } else if (i->ai_addr->sa_family == AF_INET6) {
            struct sockaddr_in6 *p = (struct sockaddr_in6 *)i->ai_addr;
            printf("%s\n", inet_ntop(AF_INET6, &p->sin6_addr, str, sizeof(str)));
        }
    }

    sockaddr_in sa;
    char host_name[500];
    char service_name[500];
    sa.sin_family = AF_INET;
    sa.sin_port = htons(80);
    sa.sin_addr.s_addr = inet_addr("183.3.226.35");

    int ret = getnameinfo((struct sockaddr*)&sa, sizeof sa, host_name, sizeof host_name, service_name, sizeof service_name, 0);
    printf("hostname: %s\n", host_name);
    printf("servicename: %s\n", service_name);
    printf("%d", ret);

    return 0;
}