cmake_minimum_required(VERSION 3.13)
project(Flugzeug)

set(CMAKE_CXX_STANDARD 20)

add_library(Flugzeug "")
add_subdirectory(src)

add_subdirectory(deps/fmt EXCLUDE_FROM_ALL)
target_link_libraries(Flugzeug fmt::fmt)

target_include_directories(Flugzeug PUBLIC src)