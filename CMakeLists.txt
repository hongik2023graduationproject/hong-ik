cmake_minimum_required(VERSION 3.30)
project(HongIk)

set(CMAKE_CXX_STANDARD 26)

add_executable(HongIk main.cpp
        repl/repl.cpp
        repl/repl.h
)
