# Copyright 2026 dywoq - MIT License
# https://github.com/djeada/Standard-of-Iron

cmake_minimum_required(VERSION 3.21.0)
project(ninja_reader LANGUAGES CXX)

add_executable(ninja_reader
    main.cpp
    commandline_argument.cpp
    routines_ninjalog_parser.cpp
    routines_sources.cpp
)
target_compile_features(ninja_reader PUBLIC cxx_std_20)
target_include_directories(ninja_reader PUBLIC ${CMAKE_SOURCE_DIR})