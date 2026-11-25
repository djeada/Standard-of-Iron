# Every translation unit parses roughly 120k lines of standard library and Qt
# headers before it reaches its own code. Precompiling that shared prefix cuts
# compile time by about a third. For one- and two-source targets, generating the
# PCH costs more than the parsing it saves, so they compile normally. Count C++
# sources, not headers, resources or AUTOMOC's empty generated translation unit.
# The header holds only stable library headers.
# Configure with -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON to build without it.
# OBJECT libraries are skipped: CMake lists their .gch among the objects
# linked into consumers, and the linker rejects it. The header is C++ only, so
# it is scoped to C++ sources; C targets such as soi_zstd would fail on it.
#
# Sharing. One PCH per target meant 40 identical 148 MB precompiled headers,
# about half of the 2 GB compiler cache in CI, and each target's translation
# units waited for their own. With GCC, targets that compile the header the
# same way reuse one of three owners instead:
#
#   soi_pch_plain    no Qt: the standard library prefix only
#   soi_pch_qt       the prefix plus Qt Core/Gui value types
#   soi_pch_qt_test  the same plus <gtest/gtest.h> for the test binaries
#
# GCC accepts a .gch only when the macros the header tested, -fPIC and the
# floating point flags match how it was built. Qt's own usage requirements
# supply -fPIC and QT_NO_DEBUG, which is why the owners link Qt::Core and a
# target that links Qt gets the Qt flavour. A target with flags of its own
# (render_gl's -fno-math-errno) keeps a private PCH, and one source with flags of
# its own (strategic_json.cpp's -fno-fast-math) just parses the header. A
# mismatch is never an error: GCC ignores the .gch and parses the header, so the
# worst case is the old cost. PCH_WARN_INVALID is off for that reason;
# scripts/check-shared-pch.py compiles one unit per target with -Winvalid-pch to
# prove the sharing took.
#
# Clang and MSVC keep one PCH per target: their rules for reuse are stricter and
# the cache pressure is a Linux CI matter.

function(soi_collect_targets directory out_var)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        soi_collect_targets("${child}" child_targets)
        list(APPEND targets ${child_targets})
    endforeach()
    set(${out_var} ${targets} PARENT_SCOPE)
endfunction()

# Sets <out_var> to the names of every target and imported library reachable
# through the link interface of <target>.
function(soi_link_closure target out_var)
    set(pending ${target})
    set(seen "")
    while(pending)
        list(POP_FRONT pending current)
        if(current IN_LIST seen)
            continue()
        endif()
        list(APPEND seen ${current})
        if(NOT TARGET ${current})
            continue()
        endif()
        get_target_property(imported ${current} IMPORTED)
        if(imported)
            continue()
        endif()
        foreach(property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
            get_target_property(libraries ${current} ${property})
            if(NOT libraries)
                continue()
            endif()
            # Generator expressions such as $<LINK_ONLY:Qt6::Core> wrap names.
            string(REGEX MATCHALL "[A-Za-z_][A-Za-z0-9_]*(::[A-Za-z0-9_]+)?" names "${libraries}")
            list(APPEND pending ${names})
        endforeach()
    endwhile()
    set(${out_var} "${seen}" PARENT_SCOPE)
endfunction()

# Creates the target that builds one shared PCH. <header> is the file it
# precompiles; <libraries> supply the definitions and flags consumers must match.
function(soi_add_pch_owner name header)
    set(owner_source "${CMAKE_BINARY_DIR}/pch/${name}.cpp")
    file(GENERATE OUTPUT "${owner_source}" CONTENT "namespace soi_pch {}\n")
    add_library(${name} STATIC "${owner_source}")
    target_precompile_headers(
        ${name}
        PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${header}>"
    )
    if(ARGN)
        target_link_libraries(${name} PRIVATE ${ARGN})
    endif()
    if(TARGET Qt6::Gui AND NOT name STREQUAL "soi_pch_plain")
        # The header names QtGui value types; their include path is all the
        # owner needs, and linking Qt::Gui would add QT_GUI_LIB, a definition
        # that Core-only targets do not have.
        target_include_directories(
            ${name}
            SYSTEM
            PRIVATE $<TARGET_PROPERTY:Qt6::Gui,INTERFACE_INCLUDE_DIRECTORIES>
        )
    endif()
    set_target_properties(${name} PROPERTIES EXCLUDE_FROM_ALL ON FOLDER "pch")
endfunction()

function(soi_precompile_common_headers)
    set(pch_header "${CMAKE_SOURCE_DIR}/cmake/soi_pch.h")
    set(pch_test_header "${CMAKE_SOURCE_DIR}/cmake/soi_pch_test.h")
    set(share_pch OFF)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_DISABLE_PRECOMPILE_HEADERS)
        set(share_pch ON)
    endif()
    get_property(root_options DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY COMPILE_OPTIONS)
    get_property(root_definitions DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY COMPILE_DEFINITIONS)

    soi_collect_targets("${CMAKE_SOURCE_DIR}" targets)
    set(owners_created "")
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        get_target_property(imported ${target} IMPORTED)
        get_target_property(source_dir ${target} SOURCE_DIR)
        if(imported OR NOT type MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY)$")
            continue()
        endif()
        string(FIND "${source_dir}" "${CMAKE_BINARY_DIR}" in_build_tree)
        if(in_build_tree EQUAL 0)
            continue()
        endif()
        if(target MATCHES "^soi_pch_")
            continue()
        endif()
        get_target_property(sources ${target} SOURCES)
        list(FILTER sources INCLUDE REGEX "\\.(cc|cpp|cxx)(>|$)")
        list(LENGTH sources source_count)
        if(source_count LESS 3)
            continue()
        endif()

        set(owner "")
        if(share_pch)
            get_target_property(target_options ${target} COMPILE_OPTIONS)
            get_property(
                target_definitions
                DIRECTORY "${source_dir}"
                PROPERTY COMPILE_DEFINITIONS
            )
            get_target_property(pic ${target} POSITION_INDEPENDENT_CODE)
            if(NOT target_options)
                set(target_options "")
            endif()
            # The owners are created here, so they carry the root directory's
            # definitions (DEFAULT_LANG). GCC refuses a .gch built with a macro
            # the translation unit does not define; a directory added before
            # that definition (animation/) keeps its own PCH.
            set(has_root_definitions ON)
            foreach(definition IN LISTS root_definitions)
                if(NOT definition IN_LIST target_definitions)
                    set(has_root_definitions OFF)
                endif()
            endforeach()
            if(has_root_definitions
               AND target_options STREQUAL "${root_options}"
               AND NOT type STREQUAL "SHARED_LIBRARY")
                soi_link_closure(${target} closure)
                set(qt_libraries ${closure})
                list(FILTER qt_libraries INCLUDE REGEX "^Qt6::")
                if(qt_libraries)
                    if(target MATCHES "_tests$" AND ("GTest::gtest" IN_LIST closure OR "gtest" IN_LIST closure))
                        set(owner soi_pch_qt_test)
                    else()
                        set(owner soi_pch_qt)
                    endif()
                elseif(NOT pic)
                    set(owner soi_pch_plain)
                endif()
            endif()
        endif()

        if(owner STREQUAL "")
            target_precompile_headers(
                ${target}
                PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${pch_header}>"
            )
            continue()
        endif()
        if(NOT owner IN_LIST owners_created)
            if(owner STREQUAL "soi_pch_plain")
                soi_add_pch_owner(${owner} "${pch_header}")
            elseif(owner STREQUAL "soi_pch_qt")
                soi_add_pch_owner(${owner} "${pch_header}" Qt6::Core)
            else()
                soi_add_pch_owner(${owner} "${pch_test_header}" Qt6::Core GTest::gtest)
            endif()
            list(APPEND owners_created ${owner})
        endif()
        target_precompile_headers(${target} REUSE_FROM ${owner})
        set_target_properties(${target} PROPERTIES PCH_WARN_INVALID OFF)
        if(SOI_PCH_VERBOSE)
            message(STATUS "pch: ${target} shares ${owner}")
        endif()
    endforeach()
endfunction()
