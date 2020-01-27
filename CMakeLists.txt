cmake_minimum_required(VERSION 3.10)
project(efp)
set(CMAKE_CXX_STANDARD 14)

find_package (Threads REQUIRED)
add_library(efp STATIC ElasticFrameProtocol.cpp)

file(GLOB unit_tests "${CMAKE_CURRENT_SOURCE_DIR}/unitTests/*.cpp")
file(GLOB unit_test_template "${CMAKE_CURRENT_SOURCE_DIR}/unitTestTemplate/*.cpp")
add_executable(efptests main.cpp ${unit_test_template} ${unit_tests} ElasticFrameProtocol.cpp)
target_link_libraries(efptests Threads::Threads)

