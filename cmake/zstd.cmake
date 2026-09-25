# zstd, for the baked creature caches (assets/creatures/*.bpat, *.bpsm, *.bprm).
#
# Those caches are ~240 MB raw and ~38 MB at level 19, and they decompress in
# about a tenth of a second, so every package ships them compressed.
#
# third_party/zstd holds the official single-file amalgamation of release
# 1.5.7 (see its README). It is compiled here as one small static target
# rather than through zstd's own CMake project, which would install headers,
# a pkg-config file and libraries into the player's package.

enable_language(C)
find_package(Threads REQUIRED)

add_library(soi_zstd STATIC "${CMAKE_SOURCE_DIR}/third_party/zstd/zstd.c")
target_include_directories(soi_zstd SYSTEM PUBLIC "${CMAKE_SOURCE_DIR}/third_party/zstd")
# The amalgamation enables zstd's worker pool on every platform but Emscripten.
target_link_libraries(soi_zstd PUBLIC Threads::Threads)
# The project's warnings-as-errors set is written for this codebase's C++, and
# third-party C does not follow it. Clear what the directory handed down.
# DISABLE_PRECOMPILE_HEADERS: cmake/PrecompiledHeaders.cmake gives every
# target the project's C++ header set, and a C translation unit cannot include
# <algorithm>. Without it the whole build fails at soi_zstd's first object.
set_target_properties(
    soi_zstd
    PROPERTIES
        COMPILE_OPTIONS ""
        POSITION_INDEPENDENT_CODE ON
        C_STANDARD 99
        DISABLE_PRECOMPILE_HEADERS ON
)
if(MSVC)
    target_compile_options(soi_zstd PRIVATE /W0)
else()
    target_compile_options(soi_zstd PRIVATE -w)
endif()
