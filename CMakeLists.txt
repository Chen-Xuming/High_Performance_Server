cmake_minimum_required(VERSION 3.16)
project(linux_high_performance_server)

set(CMAKE_CXX_STANDARD 14)

add_executable(linux_high_performance_server mycode/6.3_sendfile/sendfile_server.cpp)
#add_executable(linux_high_performance_server codes_in_book/5/5-7oobrecv.cpp)
