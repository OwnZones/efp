cmake_minimum_required(VERSION 3.10)
project(efp)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

find_package (Threads REQUIRED)
add_library(efp STATIC ElasticFrameProtocol.cpp)
add_library(efp_shared SHARED ElasticFrameProtocol.cpp)

file(GLOB unit_tests "${CMAKE_CURRENT_SOURCE_DIR}/unitTests/*.cpp")
file(GLOB unit_test_template "${CMAKE_CURRENT_SOURCE_DIR}/unitTestTemplate/*.cpp")
add_executable(efptests main.cpp ${unit_test_template} ${unit_tests} ElasticFrameProtocol.cpp)
target_link_libraries(efptests Threads::Threads)

set(CMAKE_C_STANDARD 99)
add_executable(test_efp_c_api ${CMAKE_CURRENT_SOURCE_DIR}/efp_c_api/main.c)
target_link_libraries(test_efp_c_api efp Threads::Threads)


