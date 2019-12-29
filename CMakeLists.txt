cmake_minimum_required(VERSION 3.10)
project(efp)

set(CMAKE_CXX_STANDARD 14)

find_package (Threads REQUIRED)
add_library(efp STATIC ElasticFrameProtocol.cpp)

#remove_definitions(-DDEBUG)
#add_definitions(-DDEBUG)

#if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
#    add_link_options(-fprofile-instr-generate)
#    #Uncomment in case of linker errors
#    #link_libraries(clang_rt.profile-x86_64)
#elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
#    add_compile_options(--coverage)
#    #Uncomment in case of linker errors
#    #link_libraries(gcov)
#endif ()

file(GLOB unit_tests "${CMAKE_CURRENT_SOURCE_DIR}/unitTests/*.cpp")
file(GLOB unit_test_template "${CMAKE_CURRENT_SOURCE_DIR}/unitTestTemplate/*.cpp")
add_executable(efptests main.cpp ${unit_test_template} ${unit_tests} ElasticFrameProtocol.cpp)
target_link_libraries(efptests Threads::Threads)

