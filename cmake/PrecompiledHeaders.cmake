# Every translation unit parses roughly 120k lines of standard library and Qt
# headers before it reaches its own code. Precompiling that shared prefix once
# per larger target cuts compile time by about a third. For one- and two-source
# targets, generating the PCH costs more than the parsing it saves, so they
# compile normally. Count C++ sources, not headers, resources or AUTOMOC's empty
# generated translation unit. The header holds only stable library headers.
# Configure with -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON to build without it.
# OBJECT libraries are skipped: CMake lists their .gch among the objects
# linked into consumers, and the linker rejects it. The header is C++ only, so
# it is scoped to C++ sources; C targets such as soi_zstd would fail on it.

function(soi_collect_targets directory out_var)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        soi_collect_targets("${child}" child_targets)
        list(APPEND targets ${child_targets})
    endforeach()
    set(${out_var} ${targets} PARENT_SCOPE)
endfunction()

function(soi_precompile_common_headers)
    soi_collect_targets("${CMAKE_SOURCE_DIR}" targets)
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
        get_target_property(sources ${target} SOURCES)
        list(FILTER sources INCLUDE REGEX "\\.(cc|cpp|cxx)(>|$)")
        list(LENGTH sources source_count)
        if(source_count LESS 3)
            continue()
        endif()
        target_precompile_headers(
            ${target}
            PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/cmake/soi_pch.h>"
        )
    endforeach()
endfunction()
