cmake_minimum_required(VERSION 3.15)
project(Compiler)

add_subdirectory(Flugzeug)

add_executable(Compiler "")
add_subdirectory(src)

target_link_libraries(Compiler PUBLIC Flugzeug)
target_compile_features(Compiler PUBLIC cxx_std_20)
target_include_directories(Compiler PRIVATE src)