# Copyright 2026 dywoq - MIT License
# https://github.com/djeada/Standard-of-Iron

cmake_minimum_required(VERSION 3.21.0)
project(ninja_reader LANGUAGES CXX)

add_executable(ninja_reader
    main.cpp
    commandline_argument.cpp
    routines_ninjalog_parser.cpp
)
target_compile_options(ninja_reader PUBLIC -std=c++20)
target_include_directories(ninja_reader PUBLIC ${CMAKE_SOURCE_DIR})
