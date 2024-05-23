cmake_minimum_required(VERSION 3.22)

project(ex2)

set(CMAKE_CXX_STANDARD 11)

add_executable(ex2 scheduler.h scheduler.cpp thread.h thread.cpp main.cpp uthreads.h uthreads.cpp )