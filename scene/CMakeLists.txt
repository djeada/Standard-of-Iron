# Shared scene description: types both the simulation and the renderer need.
#
# This library exists to break an inverted dependency.  The camera and the
# environment lighting state are plain view/data types, but they used to live
# under render/, which forced game code that legitimately needs a view matrix or
# a lighting description to include renderer headers.
#
# It must stay a leaf: Qt only, no engine_core, no game_systems, no render_gl.
add_library(scene_core STATIC camera.cpp)

target_include_directories(scene_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(
    scene_core
    PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
)
