cmake_minimum_required(VERSION 3.16)
project(test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

add_executable(test main.cpp ../pinned.c ../pinned.h)
add_executable(bench bench.cpp ../pinned.c ../pinned.h)