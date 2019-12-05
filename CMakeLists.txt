cmake_minimum_required(VERSION 3.15)
project(efp)

set(CMAKE_CXX_STANDARD 14)

add_library(efp STATIC EdgewareFrameProtocol.cpp)

#remove_definitions(-DDEBUG)
#add_definitions(-DDEBUG)
file(GLOB unit_tests "${CMAKE_CURRENT_SOURCE_DIR}/unitTests/*.cpp")
file(GLOB unit_test_template "${CMAKE_CURRENT_SOURCE_DIR}/unitTestTemplate/*.cpp")
add_executable(efptests main.cpp ${unit_test_template} ${unit_tests} EdgewareFrameProtocol.cpp)

