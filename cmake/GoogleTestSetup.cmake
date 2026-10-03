# Named GoogleTestSetup, not GoogleTest: cmake/ is on CMAKE_MODULE_PATH, so a
# file called GoogleTest.cmake would shadow the CMake module that provides
# gtest_discover_tests().
# ---- Google Test Setup ----
option(BUILD_TESTING "Build and register the test suite" ON)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    # GoogleTest installs its headers and static libraries by default, which would
    # otherwise land in the shipped package: a player build has no use for
    # include/gtest or libgtest.a.
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()
