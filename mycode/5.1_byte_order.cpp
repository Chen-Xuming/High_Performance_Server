//
// Created by chen on 2022/9/7.
//

#include <iostream>
#include "netinet/in.h"

void byteorder(){
    union {
        short value;
        char union_bytes[sizeof (short)];
    } test;

    test.value = 0x0102;

    if(test.union_bytes[0] == 1 && test.union_bytes[1] == 2){
        std::cout << "big endian.\n";
    }
    else if(test.union_bytes[0] == 2 && test.union_bytes[1] == 1){
        std::cout << "little endian.\n";
    }else{
        std::cout << "unknown.\n";
    }
}

int main(){
    byteorder();

    unsigned long int a = 1;
    auto b = htonl(a);
    std::cout << a << " " << b << std::endl;
    return 0;
}