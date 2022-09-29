//
// Created by chen on 2022/9/13.
//

#include <iostream>
#include <unistd.h>
#include <cstring>
#include <wait.h>

using namespace std;

string str = "hello";

int main(){
    cout << "parent process: pid = " << getpid() << endl;
    for(int i = 0; i < 2; i++){
        int ret = fork();
        if(ret == 0){
            cout << "child process: pid = " << getpid() << "  ppid = " << getppid() << "   i = " << i << endl;
        }else{
            waitpid(ret, nullptr, 0);
        }
    }
    return 0;
}


