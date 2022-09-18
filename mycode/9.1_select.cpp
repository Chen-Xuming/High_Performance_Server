//
// Created by chen on 2022/9/15.
//

#include <iostream>
#include <sys/select.h>
#include <unistd.h>

int main()
{
    std::string message;
    while(1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);     // 标准输入fd
        int ret = select(1, &fds, NULL, NULL, NULL);
        if (ret == -1) break;
        getline(std::cin, message);
        if (message == "exit") {
            break;
        }
        std::cout << "read: " << message << std::endl;
    }
    return 0;
}