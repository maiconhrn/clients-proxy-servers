cmake_minimum_required(VERSION 3.17)
project(client_proxy_server C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS -pthread)

add_executable(client src/client.c)
add_executable(proxy src/proxy.c)
add_executable(server src/server.c)
