# ---- Clang-Tidy integration ----
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
option(ENABLE_CLANG_TIDY "Enable clang-tidy analysis" OFF)

if(ENABLE_CLANG_TIDY AND CLANG_TIDY_EXE)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
    # Prefer using .clang-tidy config, but define fallback checks for safety
    set(CLANG_TIDY_ARGS "-header-filter=^(?!.*third_party/).*/Standard-of-Iron/.*")
    set(CLANG_TIDY_FILTER "${CMAKE_SOURCE_DIR}/tools/clang_tidy_filter.sh")
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.clang-tidy")
        list(
            APPEND CLANG_TIDY_ARGS
            "-checks=bugprone-*,performance-*,readability-*,modernize-*,cppcoreguidelines-*,clang-analyzer-*"
        )
    endif()
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_FILTER} ${CLANG_TIDY_EXE} ${CLANG_TIDY_ARGS})
elseif(ENABLE_CLANG_TIDY)
    message(WARNING "clang-tidy requested but not found! Static analysis skipped.")
else()
    message(STATUS "clang-tidy disabled (ENABLE_CLANG_TIDY=OFF)")
endif()
