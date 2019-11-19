cmake_minimum_required(VERSION 3.15)
project(efp)

set(CMAKE_CXX_STANDARD 14)

add_library(efp STATIC EdgewareFrameProtocol.cpp)

#remove_definitions(-DDEBUG)
#add_definitions(-DDEBUG)
add_executable(efptests main.cpp EdgewareFrameProtocol.cpp)

