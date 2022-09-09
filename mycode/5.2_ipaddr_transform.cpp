//
// Created by chen on 2022/9/7.
//

#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>

int main(){
    auto n_order_addr = inet_addr("192.168.17.128");
    std::cout << n_order_addr << std::endl;

    in_addr my_in_addr{};
    int res = inet_aton("192.168.17.128", &my_in_addr);
    if(res == 0){
        std::cout << "fail.\n";
    }else{
        std::cout << "success: " << my_in_addr.s_addr << std::endl;
    }

    auto ip_addr_1 = inet_ntoa(my_in_addr);
    auto ip_addr_2 = inet_ntoa(in_addr{2148049999});
    std::cout << ip_addr_1 << std::endl;
    std::cout << ip_addr_2 << std::endl;

    return 0;
}
