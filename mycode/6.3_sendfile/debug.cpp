//
// Created by chen on 2022/9/11.
//

#include <stdio.h>
#include <fcntl.h>

int main(){
    int file_fd = open("../mycode/6.3_sendfile/a_letter.txt", O_RDONLY);
    if(file_fd == -1){
        printf("failed.\n");
    }else{
        printf("success.\n");
    }

    return 0;
}