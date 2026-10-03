# ---- Portability warnings ----
# The game is developed on Linux/GCC and shipped on macOS/AppleClang and
# Windows/MSVC. This option turns on the warnings that catch, on Linux, the
# constructs that behave differently on those other two toolchains. It is OFF
# by default so a normal build stays quiet; CI turns it on (see `make
# portability` and the portability job in quality.yml).
#
# The set is split in two deliberately:
#
#   * ERRORS are the diagnostics that indicate real divergence between
#     compilers -- unspecified evaluation order, dangling references, missing
#     returns, disabled NaN handling, class/struct tags that the Microsoft ABI
#     mangles apart. Every one of these is at zero, so the gate is meaningful:
#     a new hit is a regression, not a backlog item.
#   * The rest stay warnings. There are ~2,700 of them (mostly -Wswitch and
#     -Wmissing-field-initializers) and they are stylistic, not portability
#     signal. Promoting them would drown the ones that matter.
#
# Anything suppressed here is suppressed because it is noise, never because a
# real finding was inconvenient.
option(
    SOI_PROFILE_ALLOCATIONS
    "Replace global operator new/delete to count render-thread allocations"
    OFF
)
if(SOI_PROFILE_ALLOCATIONS)
    add_compile_definitions(SOI_PROFILE_ALLOCATIONS=1)
    message(STATUS "Render-thread allocation counting enabled (SOI_PROFILE_ALLOCATIONS)")
endif()

option(SOI_STRICT_WARNINGS "Compile with the cross-platform portability warning set" OFF)
if(SOI_STRICT_WARNINGS)
    if(MSVC)
        add_compile_options(
            /W4
            /permissive- # Reject the Microsoft language extensions
            /we4715 # Not all control paths return a value
            /we4172 # Returning the address of a local
            /we4239 # Non-standard binding of a temporary to a reference
            /we4099 # Type declared 'struct' in one place and 'class' in another
            /we4062 # Enumerator not handled in a switch with no default
            /we4456 # Declaration shadows a local
            /we4457 # Declaration shadows a function parameter
            /we4458 # Declaration shadows a class member
            /we4700 # Local used without being initialised
            /we4702 # Unreachable code
            /we4189 # Local variable initialised but not referenced
            /we4505 # Unreferenced local function
            /wd4100 # Unreferenced formal parameter
            /wd4244 # Narrowing conversion
            /wd4267 # size_t narrowing
        )
    else()
        add_compile_options(-Wall -Wextra)
        # -Werror=unused-result belongs to this set on merit but cannot live
        # here: moc emits `_t->some_getter();` for every invokable, so a
        # [[nodiscard]] getter on a Q_OBJECT fails the build from generated
        # code we do not own, and 42 headers have that shape. It is enforced in
        # scripts/check-portability.py instead, which only sees our own
        # translation units.
        add_compile_options(
            -Werror=return-type
            -Werror=sign-compare
            -Werror=range-loop-construct
            -Werror=switch
            -Werror=extra-semi
            -Werror=implicit-fallthrough
            -Werror=old-style-cast
            -Werror=unused-local-typedefs
            -Werror=unused-variable
            -Werror=unused-function
            -Wno-missing-field-initializers
            -Wno-unused-parameter
        )
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(
                # The Apple toolchain is Clang, so these are the ones that
                # speak for macOS.
                -Werror=unsequenced
                -Werror=dangling-gsl
                -Werror=inconsistent-missing-override
                # Fires if anyone reinstates -ffast-math without
                # -fno-finite-math-only and silently deletes the isfinite
                # guards again. See the floating point note above.
                -Werror=nan-infinity-disabled
                # class/struct has to agree across declarations: the Microsoft
                # ABI mangles the two differently, so a type forward-declared
                # one way and defined the other is a Windows linker error that
                # nothing on this platform can produce. Clang is the only
                # compiler here that says so.
                -Werror=mismatched-tags
                # A range-for whose loop variable binds to a temporary is
                # reading a per-iteration copy while looking like a reference
                # into the container. Every one of these was `const auto&` over
                # a QJsonArray, whose iterator yields QJsonValue by value.
                -Werror=range-loop-bind-reference
                -Werror=shadow-field
                -Werror=unused-lambda-capture
                -Werror=unused-const-variable
                -Werror=unused-private-field
                -Werror=deprecated-copy-with-dtor
                -Werror=conditional-uninitialized
                -Werror=loop-analysis
                -Werror=self-assign
                -Werror=unreachable-code
                -Werror=header-hygiene
                -Werror=cast-qual
                -Werror=suggest-override
                -Werror=newline-eof
            )
        else()
            add_compile_options(
                -Werror=sequence-point
                -Werror=unused-but-set-variable
                -Werror=unused-const-variable=1
                -Werror=suggest-override
            )
            if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13)
                add_compile_options(-Werror=dangling-reference)
            endif()
        endif()
    endif()
    message(STATUS "Portability warning set enabled (SOI_STRICT_WARNINGS)")
endif()
