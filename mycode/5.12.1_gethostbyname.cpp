//
// Created by chen on 2022/9/9.
//

// 通过主机名获得主机信息

#include <stdio.h>
#include <netdb.h>

int main(int argc, char *argv[]){
    if(argc < 2){
        printf("too few arguments.\n");
    }

    const char *name = argv[1];

    hostent *host_info = gethostbyname(name);

    if(host_info){
        printf("----------------- host info ------------------\n");
        printf("[host name] %s\n", host_info->h_name);
        printf("[aliases] ");
        for(char *alias = *host_info->h_aliases; alias; alias = *++(host_info->h_aliases)){
            printf("%s  ", alias);
        }
        printf("\n[addr_type] %d\n", host_info->h_addrtype);
        printf("[addr_length] %d\n", host_info->h_length);
    }

    return 0;
}