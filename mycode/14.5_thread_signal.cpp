//
// Created by chen on 2022/9/28.
//

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

void handle_error_en(int en, const char *msg) {
    do {
        errno = en;
        perror(msg);
        exit(EXIT_FAILURE);
    } while(0);
}

void *sig_thread(void *arg){
    sigset_t *set = (sigset_t *)arg;
    int s, sig;
    for(;;){
        // 步骤二：调用sigwait等待信号
        s = sigwait(set, &sig);
        if(s != 0){
            handle_error_en(s, "sigwait");
        }
        printf("Signal handling thread got signal %d\n", sig);
    }
}

int main(){
    printf("hello\n");
    pthread_t thread;
    sigset_t set;
    int s;

    // 步骤一：在主线程设置信号掩码
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTTIN);
    s = pthread_sigmask(SIG_BLOCK, &set, NULL);     // 新线程的掩码是当前值与set的并集
    if(s != 0){
        handle_error_en(s, "pthread_sigmask");
    }

    s = pthread_create(&thread, NULL, &sig_thread, (void *)&set);
    if(s != 0){
        handle_error_en(s, "pthread_create");
    }

    pthread_join(thread, nullptr);
    return 0;
}