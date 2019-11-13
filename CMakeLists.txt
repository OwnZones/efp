cmake_minimum_required(VERSION 3.15)
project(efp)

set(CMAKE_CXX_STANDARD 14)

add_executable(efp main.cpp EdgewareFrameProtocol.cpp EdgewareFrameProtocol.h)