cmake_minimum_required(VERSION 3.8)
project(Compiler)

set(CMAKE_CXX_STANDARD 20)

add_executable(Compiler src/main.cpp src/turboc/Lexer.cpp src/turboc/Lexer.hpp src/turboc/PrintToken.cpp src/turboc/Parser.cpp src/turboc/Parser.hpp src/turboc/AST.cpp src/turboc/AST.hpp src/turboc/Type.cpp src/turboc/Type.hpp src/turboc/Function.cpp src/turboc/Function.hpp src/turboc/Conversion.cpp src/turboc/Conversion.hpp src/turboc/ASTPrinter.cpp src/turboc/ASTPrinter.hpp src/turboc/IRGenerator.cpp src/turboc/IRGenerator.hpp src/turboc/ASTVisitor.cpp src/turboc/ASTVisitor.hpp src/bf/Compiler.cpp src/bf/Compiler.hpp src/turboc/Compiler.cpp src/turboc/Compiler.hpp src/bf/BrainfuckLoopOptimization.cpp src/bf/BrainfuckLoopOptimization.hpp src/bf/BrainfuckBufferSplitting.cpp src/bf/BrainfuckBufferSplitting.hpp)

add_subdirectory("Flugzeug")
target_link_libraries(Compiler Flugzeug)

target_include_directories(Compiler PRIVATE src)