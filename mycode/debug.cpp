//
// Created by chen on 2022/9/13.
//

#include <iostream>
#include <string.h>

int main(){
    char buf[100];
    memset(buf, '\0', 100);
    std::cin.getline(buf, sizeof buf);
    printf("%s", buf);

    return 0;
}

