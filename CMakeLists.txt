cmake_minimum_required(VERSION 3.16)
project(Xrpc)

set(CMAKE_CXX_STANDARD 11)

include_directories(.)

add_executable(server
        server.cpp)

add_executable(client
        client.cpp)