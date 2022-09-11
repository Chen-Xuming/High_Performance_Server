//
// Created by chen on 2022/9/10.
//

// 父进程、子进程之间利用管道通信

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

int main(){
    int ret = -1;
    int fd[2];
    pid_t pid;
    char buf[512] = {0};
    char *msg = "hello world";

    ret = pipe(fd);
    if(ret == -1){
        printf("create pipe failed.\n");
        return -1;
    }

    pid = fork();

    if(pid == 0){
        printf("[kid] write message to fd[1].\n");
        ret = write(fd[1], msg, strlen(msg));
    }else{
        ret = read(fd[0], buf, sizeof(buf));
        printf("[parent] read message from fd[0]: %s\n", buf);
        wait(NULL);
    }

    return 0;
}