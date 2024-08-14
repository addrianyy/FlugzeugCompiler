cmake_minimum_required(VERSION 3.15)

add_subdirectory(deps/fmt EXCLUDE_FROM_ALL)

add_library(Flugzeug "")
add_subdirectory(src)

target_link_libraries(Flugzeug PUBLIC fmt::fmt)
target_compile_features(Flugzeug PUBLIC cxx_std_20)
target_include_directories(Flugzeug PUBLIC src)