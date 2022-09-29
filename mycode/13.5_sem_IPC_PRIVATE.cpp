//
// Created by chen on 2022/9/26.
//

// 使用IPC_PRIVATE键值创建信号量，在父子进程中进行PV操作

#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

union semun{
    int val;
    semid_ds *buf;
    unsigned short int *array;
    seminfo *__buf;
};

// op < 0 执行P操作；op > 0 执行V操作
void pv(int sem_id, int op){
    sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_flg = SEM_UNDO;
    sem_b.sem_op = op;
    semop(sem_id, &sem_b, 1);
}

int main(int argc, char *argv[]){
    int sem_id = semget(IPC_PRIVATE, 1, 0666);  // 注：0666, 八进制， rwxrwxrwx = 110110110，表示只有读写权限，没有操作权限
    semun sem_un;
    sem_un.val = 1;
    semctl(sem_id, 0, SETVAL, sem_un);  // 将信号量初始值设为1

    pid_t id = fork();
    if(id < 0) return -1;
    else if(id == 0){
        printf("[child process] try to get semaphore\n");
        pv(sem_id, -1);     // P操作
        printf("[child process] get the semaphore\n");
        sleep(5);
        pv(sem_id, 1);      // V操作
        printf("[child process] release the semaphore\n");
        exit(0);
    }else{
        printf("[parent process] try to get semaphore\n");
        pv(sem_id, -1);
        printf("[parent process] get the semaphore\n");
        sleep(5);
        pv(sem_id, 1);      // V操作
        printf("[parent process] release the semaphore\n");
    }

    waitpid(id, nullptr, 0);
    semctl(sem_id, 0, IPC_RMID, sem_un);    // 删除信号量
    return 0;
}