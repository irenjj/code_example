cmake_minimum_required(VERSION 3.24)
project(code_example)

set(CMAKE_CXX_STANDARD 11)

set_target_properties(${TARGET} PROPERTIES
    COMPILE_FLAGS "-pthread"
    LINK_FLAGS "-pthread")

find_package(Threads)

add_executable(code_example main.cc)
target_link_libraries(code_example ${CMAKE_THREAD_LIBS_INIT})
