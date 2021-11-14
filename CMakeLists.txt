cmake_minimum_required(VERSION 3.8)
project(Compiler)

set(CMAKE_CXX_STANDARD 20)

add_executable(Compiler src/main.cpp src/FunctionsCreator.cpp src/FunctionsCreator.hpp)

add_subdirectory("Flugzeug")
target_link_libraries(Compiler Flugzeug)

target_include_directories(Compiler PRIVATE src)