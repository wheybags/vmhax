cmake_minimum_required(VERSION 3.16)
project(test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

add_executable(test_pinned test_pinned.cpp test_pinned.c test.h ../pinned.c ../pinned.h)
add_executable(bench_pinned bench_pinned.cpp ../pinned.c ../pinned.h)

add_executable(test_cow test_cow.cpp ../recursive_cow.cpp ../recursive_cow.hpp)