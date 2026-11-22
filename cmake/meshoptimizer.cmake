# meshoptimizer, for simplifying creature meshes into their small-on-screen copy.
#
# third_party/meshoptimizer holds the needed sources of release 0.22, unmodified
# (see its README). It is compiled here as one small static target rather than
# through meshoptimizer's own CMake project, which would install headers and
# libraries into the player's package.

add_library(
    soi_meshoptimizer
    STATIC
    "${CMAKE_SOURCE_DIR}/third_party/meshoptimizer/allocator.cpp"
    "${CMAKE_SOURCE_DIR}/third_party/meshoptimizer/simplifier.cpp"
    "${CMAKE_SOURCE_DIR}/third_party/meshoptimizer/vcacheoptimizer.cpp"
)
target_include_directories(
    soi_meshoptimizer
    SYSTEM
    PUBLIC "${CMAKE_SOURCE_DIR}/third_party/meshoptimizer"
)
# The project's warnings-as-errors set is written for this codebase, and
# third-party code does not follow it. Clear what the directory handed down,
# and keep the project's precompiled header set out of these sources.
set_target_properties(
    soi_meshoptimizer
    PROPERTIES COMPILE_OPTIONS "" POSITION_INDEPENDENT_CODE ON DISABLE_PRECOMPILE_HEADERS ON
)
if(MSVC)
    target_compile_options(soi_meshoptimizer PRIVATE /W0)
else()
    target_compile_options(soi_meshoptimizer PRIVATE -w)
endif()
