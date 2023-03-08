cmake_minimum_required(VERSION 3.10)
project(efp)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

#Code coverage Clang
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
#Code coverage GCC
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")

find_package(Threads REQUIRED)
find_package(GTest REQUIRED)

if(WIN32)
    set(CMAKE_CXX_FLAGS_RELEASE "/MT")
    set(CMAKE_CXX_FLAGS_DEBUG "/MTd")
endif()

add_library(efp STATIC ElasticFrameProtocol.cpp)
add_library(efp_shared SHARED ElasticFrameProtocol.cpp)

add_executable(runUnitTestsEFP
        ${CMAKE_CURRENT_SOURCE_DIR}/ElasticFrameProtocol.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest1.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest2.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest3.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest4.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest5.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest6.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest7.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest8.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest9.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest10.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest11.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest12.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest13.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest14.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest15.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest16.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest17.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest18.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest19.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest20.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest21.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest22.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTest23.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/UnitTestHelpers.cpp
        )

target_include_directories(runUnitTestsEFP
        PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/unitTests_gtest
        SYSTEM
        ${GTEST_INCLUDE_DIRS})

target_link_libraries(runUnitTestsEFP
        Threads::Threads
        GTest::gtest_main)

add_executable(performanceLab
        ${CMAKE_CURRENT_SOURCE_DIR}/ElasticFrameProtocol.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/unitTests/PerformanceLab.cpp
        )

target_include_directories(performanceLab
        PRIVATE
        ${CMAKE_SOURCE_DIR}
        SYSTEM
        ${GTEST_INCLUDE_DIRS})

target_link_libraries(performanceLab
        Threads::Threads)


set(CMAKE_C_STANDARD 99)
add_executable(test_efp_c_api ${CMAKE_CURRENT_SOURCE_DIR}/efp_c_api/main.c)
target_link_libraries(test_efp_c_api efp Threads::Threads)

