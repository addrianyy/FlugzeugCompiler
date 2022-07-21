cmake_minimum_required(VERSION 3.13)
project(Compiler)

set(CMAKE_CXX_STANDARD 20)

add_executable(Compiler "")
add_subdirectory(src)

add_subdirectory(Flugzeug)
target_link_libraries(Compiler Flugzeug)

target_include_directories(Compiler PRIVATE src)