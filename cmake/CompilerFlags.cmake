# ---- Windows-specific definitions ----
if(WIN32)
    add_compile_definitions(NOMINMAX) # Prevent min/max macros from windows.h
endif()

# Optimised builds are stripped so shipped artifacts carry no debug info. A
# sampling profiler needs the symbol table, so let a profiling build opt out.
option(SOI_KEEP_SYMBOLS "Keep the symbol table in optimised builds" OFF)
# Release candidates are built with full debug information, so every build a
# player runs can be symbolised later. The workflows split it off before
# packaging and keep it as a private CI artifact: a PDB, a dSYM, or a .debug
# file. The shipped binary is exactly as lean as it would be without it.
option(
    SOI_RELEASE_DEBUG_INFO
    "Emit debug information in optimised builds for separate symbol files"
    OFF
)

# ---- Compiler Optimization Flags ----
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        # MSVC Debug flags
        set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DDEBUG")
        set(CMAKE_C_FLAGS_DEBUG "/Od /Zi /DDEBUG")
    else()
        # GCC/Clang Debug flags
        if(SOI_DEBUG_INFO STREQUAL "lines")
            # File and line tables only: enough for backtraces and sanitizer
            # reports, a third smaller per object than -g3 (no macro records,
            # no variable locations), which is what CI links and caches.
            set(CMAKE_CXX_FLAGS_DEBUG "-g1 -O0 -DDEBUG")
            set(CMAKE_C_FLAGS_DEBUG "-g1 -O0 -DDEBUG")
        else()
            set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0 -DDEBUG")
            set(CMAKE_C_FLAGS_DEBUG "-g3 -O0 -DDEBUG")
            # Additional GDB-friendly flags
            add_compile_options(-ggdb3) # Maximum debug info for GDB
        endif()
        add_compile_options(-fno-omit-frame-pointer) # Keep frame pointers for better backtraces
        add_compile_options(-fno-inline) # Don't inline functions for easier debugging
        # Disable optimizations that make debugging harder
        add_compile_options(-fno-optimize-sibling-calls)
    endif()
    message(STATUS "Building in DEBUG mode with GDB support")
else()
    # Release/RelWithDebInfo: optimize for performance
    # ---- Floating point ----
    # The codebase guards roughly forty inputs with std::isfinite: volumes read
    # back from the settings file, camera angles, impact geometry. Those guards
    # only exist if the compiler is not allowed to assume operands are finite.
    #
    # Both -ffast-math and MSVC's /fp:fast grant exactly that permission, so an
    # optimised build folds every one of those checks to "true" and the clamp
    # behind it disappears. It is invisible in development because Debug builds
    # do not use these flags, and it diverges per platform, which is worse than
    # being uniformly wrong. Clang says so out loud (-Wnan-infinity-disabled);
    # GCC and MSVC do it silently.
    #
    # GCC and Clang can keep the rest of fast math and give the finiteness
    # assumption back with -fno-finite-math-only. MSVC has no such sub-switch,
    # so Windows uses /fp:precise -- the same reasoning, and the only setting
    # there that keeps the guards alive.
    if(MSVC)
        # MSVC Release flags
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /Oi /Ot /GL /fp:precise")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
    else()
        # GCC/Clang Release flags
        set(CMAKE_CXX_FLAGS
            "${CMAKE_CXX_FLAGS} -O3 -ffast-math -fno-finite-math-only -funroll-loops -ftree-vectorize"
        )
        # Keep release artifacts on the compiler's portable architecture
        # baseline. CI-runner-specific -march flags can make packaged builds
        # crash on otherwise supported user CPUs.
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE) # Enable LTO

    # Give the linker per-symbol granularity so unreferenced code is dropped
    # rather than carried, and leave no debug info in the shipped artifact.
    # Keep an unstripped copy from the build tree if you need to symbolise a
    # crash report; -g is still available through RelWithDebInfo.
    if(SOI_RELEASE_DEBUG_INFO)
        if(MSVC)
            # /Z7 keeps the debug records in each object file, so parallel
            # compiles never contend for one PDB. /OPT:REF and /OPT:ICF restore
            # the folding that /DEBUG would otherwise switch off.
            add_compile_options(/Z7)
            add_link_options(/DEBUG /OPT:REF /OPT:ICF)
        else()
            add_compile_options(-g)
            if(APPLE)
                # With LTO the debug information lives in a temporary object
                # that the linker deletes. Keep it so dsymutil can read it.
                add_link_options("-Wl,-object_path_lto,${CMAKE_BINARY_DIR}/lto.o")
            endif()
        endif()
    endif()
    if(NOT MSVC)
        add_compile_options(-ffunction-sections -fdata-sections)
        if(APPLE)
            add_link_options(-Wl,-dead_strip)
        elseif(SOI_KEEP_SYMBOLS OR SOI_RELEASE_DEBUG_INFO)
            add_link_options(-Wl,--gc-sections)
        else()
            add_link_options(-Wl,--gc-sections -s)
        endif()
    endif()
    message(STATUS "Building in RELEASE mode with optimizations")
endif()
