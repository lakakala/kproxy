cmake_minimum_required(VERSION 3.20)

project(kproxy C)


find_package(Libevent CONFIG REQUIRED)

set(src_list main.c server.c server.h kmalloc.h kmalloc.c sock5_parser.c)

add_executable(kproxy ${src_list})

target_link_libraries(kproxy PRIVATE libevent::core libevent::extra libevent::pthreads)