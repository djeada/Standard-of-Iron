if(TARGET WrapOpenGL::WrapOpenGL)
    set(WrapOpenGL_FOUND ON)
    return()
endif()

set(WrapOpenGL_FOUND OFF)

find_package(OpenGL ${WrapOpenGL_FIND_VERSION})

if(OpenGL_FOUND)
    set(WrapOpenGL_FOUND ON)
    add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)

    if(APPLE)
        get_target_property(soi_opengl_library OpenGL::GL IMPORTED_LOCATION)
        if(soi_opengl_library AND NOT soi_opengl_library MATCHES "/([^/]+)\\.framework$")
            get_filename_component(soi_opengl_framework_path "${soi_opengl_library}" DIRECTORY)
        endif()
        if(NOT soi_opengl_framework_path)
            set(soi_opengl_framework_path "-framework OpenGL")
        endif()
        target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE ${soi_opengl_framework_path})
    else()
        target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE OpenGL::GL)
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WrapOpenGL DEFAULT_MSG WrapOpenGL_FOUND)
