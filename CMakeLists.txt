cmake_minimum_required(VERSION 3.19)
project(IRDecompiler)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory("deps/fmt")

add_executable(${PROJECT_NAME} src/main.cpp src/IR/Value.cpp src/IR/Value.hpp src/IR/User.cpp src/IR/User.hpp src/IR/Instruction.cpp src/IR/Instruction.hpp src/IR/Type.cpp src/IR/Type.hpp src/Core/ClassTraits.cpp src/Core/ClassTraits.hpp src/Core/Error.cpp src/Core/Error.hpp src/Core/Log.cpp src/Core/Log.hpp src/Core/Platform.cpp src/Core/Platform.hpp src/Core/Casting.cpp src/Core/Casting.hpp src/IR/Function.cpp src/IR/Function.hpp src/IR/Block.cpp src/IR/Block.hpp src/Core/IntrusiveLinkedList.cpp src/Core/IntrusiveLinkedList.hpp src/IR/Instructions.cpp src/IR/Instructions.hpp src/IR/IRPrinter.cpp src/IR/IRPrinter.hpp src/IR/PrintInstruction.cpp src/IR/Context.cpp src/IR/Context.hpp src/IR/InstructionInserter.cpp src/IR/InstructionInserter.hpp src/IR/ConsolePrinter.cpp src/IR/ConsolePrinter.hpp src/Core/ConsoleColors.cpp src/Core/ConsoleColors.hpp src/Passes/MemoryToSSA.cpp src/Passes/MemoryToSSA.hpp src/Passes/DeadCodeElimination.cpp src/Passes/DeadCodeElimination.hpp src/FunctionsCreator.cpp src/FunctionsCreator.hpp src/Core/Iterator.cpp src/Core/Iterator.hpp)

target_link_libraries(${PROJECT_NAME} fmt::fmt)
target_include_directories(${PROJECT_NAME} PRIVATE src/)