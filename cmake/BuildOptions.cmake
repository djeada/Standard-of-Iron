# ---- Build speed (CI sets these; the defaults are what a developer wants) ----
# See cmake/BuildSpeed.cmake for what each costs and saves.
set(SOI_DEBUG_INFO
    "full"
    CACHE STRING
    "Debug information in Debug builds: full (-g3, macros for GDB) or lines (-g1)"
)
set_property(CACHE SOI_DEBUG_INFO PROPERTY STRINGS full lines)
if(SOI_DEBUG_INFO STREQUAL "lines")
    set(SOI_DEBUG_FLAG -g1)
elseif(SOI_DEBUG_INFO STREQUAL "full")
    set(SOI_DEBUG_FLAG -g)
else()
    message(FATAL_ERROR "SOI_DEBUG_INFO must be full or lines, not '${SOI_DEBUG_INFO}'")
endif()
set(SOI_LINKER "" CACHE STRING "Linker for -fuse-ld (e.g. mold, lld); empty keeps the default")
include(cmake/BuildSpeed.cmake)
soi_select_linker()

# ---- Sanitizer and coverage lanes (used by the weekly CI workflow) ----
# ENABLE_SANITIZER accepts a comma-separated -fsanitize list, e.g.
#   cmake -DENABLE_SANITIZER=address,undefined ..
set(ENABLE_SANITIZER
    ""
    CACHE STRING
    "Comma-separated sanitizer list (address, undefined, thread, ...)"
)
option(ENABLE_COVERAGE "Instrument the build for coverage reporting" OFF)

if(ENABLE_SANITIZER)
    if(MSVC)
        message(WARNING "ENABLE_SANITIZER is not supported with MSVC; ignoring")
    else()
        message(STATUS "Sanitizers enabled: ${ENABLE_SANITIZER}")
        add_compile_options(
            -fsanitize=${ENABLE_SANITIZER}
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
            ${SOI_DEBUG_FLAG}
        )
        add_link_options(-fsanitize=${ENABLE_SANITIZER})
        set(SOI_INSTRUMENTED_BUILD ON)
    endif()
endif()

if(ENABLE_COVERAGE)
    if(MSVC)
        message(WARNING "ENABLE_COVERAGE is not supported with MSVC; ignoring")
    else()
        message(STATUS "Coverage instrumentation enabled")
        add_compile_options(--coverage ${SOI_DEBUG_FLAG} -O0 -fprofile-update=atomic)
        add_link_options(--coverage)
        set(SOI_INSTRUMENTED_BUILD ON)
    endif()
endif()

# A sanitized or coverage-instrumented build runs several times slower than the
# one players get. Assertions about wall-clock behaviour measure the
# instrumentation in that configuration rather than the game, so the few places
# that make them ask for this first. It is not a feature toggle: nothing
# changes about what the code does, only about which timing claims are
# meaningful to check.
if(SOI_INSTRUMENTED_BUILD)
    add_compile_definitions(SOI_INSTRUMENTED_BUILD=1)
endif()
