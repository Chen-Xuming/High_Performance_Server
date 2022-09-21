cmake_minimum_required(VERSION 3.16)
project(linux_high_performance_server)

set(CMAKE_CXX_STANDARD 14)

add_executable(linux_high_performance_server mycode/11.2_linklist_timer/nonactive_connection.cpp)
