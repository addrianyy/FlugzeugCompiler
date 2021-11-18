cmake_minimum_required(VERSION 3.8)
project(Compiler)

set(CMAKE_CXX_STANDARD 20)

add_executable(Compiler src/main.cpp src/FunctionsCreator.cpp src/FunctionsCreator.hpp src/turboc/Lexer.cpp src/turboc/Lexer.hpp src/turboc/PrintToken.cpp src/turboc/Parser.cpp src/turboc/Parser.hpp src/turboc/AST.cpp src/turboc/AST.hpp)

add_subdirectory("Flugzeug")
target_link_libraries(Compiler Flugzeug)

target_include_directories(Compiler PRIVATE src)