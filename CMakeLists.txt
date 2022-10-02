cmake_minimum_required(VERSION 3.16)
project(linux_high_performance_server)

set(CMAKE_CXX_STANDARD 14)

link_libraries(pthread)

add_executable(linux_high_performance_server mycode/threadpool/main.cpp
                                             mycode/threadpool/http_conn.cpp
                                             mycode/threadpool/http_conn.h
                                             mycode/threadpool/threadpool.h)
#add_executable(linux_high_performance_server codes_in_book/13/13-4shm_talk_server.cpp)
