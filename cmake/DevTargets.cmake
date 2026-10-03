# ---- clang-format helpers (optional but convenient) ----
# Provides:
#   - clang-format        : formats all C/C++ sources using .clang-format
#   - clang-format-check  : CI-style check (no changes), fails on violations
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    message(STATUS "Found clang-format: ${CLANG_FORMAT_EXE}")

    file(
        GLOB_RECURSE ALL_CXX
        CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/*.c"
        "${CMAKE_SOURCE_DIR}/*.cpp"
        "${CMAKE_SOURCE_DIR}/*.h"
        "${CMAKE_SOURCE_DIR}/*.hpp"
    )

    # Exclude generated, vendored, and local build-tree sources.
    list(FILTER ALL_CXX EXCLUDE REGEX "${CMAKE_SOURCE_DIR}/(build|build-[^/]*|third_party)/.*")
    list(FILTER ALL_CXX EXCLUDE REGEX "${CMAKE_BINARY_DIR}/.*")

    if(ALL_CXX)
        add_custom_target(
            clang-format
            COMMAND "${CLANG_FORMAT_EXE}" -i --style=file ${ALL_CXX}
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Running clang-format with .clang-format (strict)"
        )

        add_custom_target(
            clang-format-check
            COMMAND "${CLANG_FORMAT_EXE}" --dry-run -Werror --style=file ${ALL_CXX}
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Checking clang-format compliance"
        )
    else()
        add_custom_target(
            clang-format
            COMMAND "${CMAKE_COMMAND}" -E echo "No C/C++ files found to format."
        )
        add_custom_target(
            clang-format-check
            COMMAND "${CMAKE_COMMAND}" -E echo "No C/C++ files found to check."
        )
    endif()
endif()

# ---- Content Validation ----
# Add custom target to validate missions and campaigns
add_custom_target(
    validate-content
    COMMAND content_validator "${CMAKE_SOURCE_DIR}/assets"
    DEPENDS content_validator
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Validating mission and campaign content"
)

# Optionally make validation run as part of the build
# Uncomment the following line to make build fail on invalid content:
# add_dependencies(standard_of_iron validate-content)
