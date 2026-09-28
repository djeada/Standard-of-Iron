# Knobs that make a from-scratch build cheaper. Each defaults to what a
# developer at a debugger wants; CI turns them on, because a CI build is thrown
# away and only its test results and backtraces are kept.
#
# SOI_DEBUG_INFO=lines (CMakeLists.txt) builds Debug with -g1 instead of
# -g3 -ggdb3. Objects lose macro records and variable locations and keep the
# line tables that backtraces and sanitizer reports print: world.cpp's object
# goes from 22.9 MB to 14.7 MB, which is what the linker reads and ccache
# stores.
#
# SOI_LINKER=mold links through -fuse-ld. GNU ld is single threaded and the 52
# test and tool binaries link at the tail of the build, when little is left to
# overlap with them. Naming a linker that is not installed is a configure error
# rather than a silent fall back to the slow one.
#
# Unity (jumbo) builds were tried on 28 Sep 2026 and are not here on purpose.
# At -O0 over half of a typical file's compile is code generation for templates
# that a batch would generate once, but 1111 of 1379 sources keep helpers in
# anonymous namespaces and the same names recur (smoothstep, clamp01,
# k_two_pi, scale, add): 129 of 234 batches of eight failed across nearly every
# target. Batches are assigned by source order, so even after renaming them all
# a new file would reshuffle the batches and break a build it never touched.

function(soi_select_linker)
    if(SOI_LINKER STREQUAL "")
        return()
    endif()
    if(MSVC)
        message(FATAL_ERROR "SOI_LINKER is for GCC and Clang; MSVC uses link.exe")
    endif()
    include(CheckLinkerFlag)
    check_linker_flag(CXX "-fuse-ld=${SOI_LINKER}" SOI_HAS_LINKER_${SOI_LINKER})
    if(NOT SOI_HAS_LINKER_${SOI_LINKER})
        message(FATAL_ERROR "SOI_LINKER=${SOI_LINKER} but -fuse-ld=${SOI_LINKER} does not link")
    endif()
    message(STATUS "Linking with ${SOI_LINKER}")
    add_link_options("-fuse-ld=${SOI_LINKER}")
endfunction()
