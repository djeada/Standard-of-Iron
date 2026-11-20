# Copyright 2026 dywoq - MIT License 2.0
# https://github.com/djeada/Standard-of-Iron

cmake_minimum_required(VERSION 3.21.0)
project(ninja_reader LANGUAGES CXX)

add_executable(ninja_reader
    main.cpp
)
target_compile_options(ninja_reader
    PUBLIC -std=c++20
)

