if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_executable(map_editor
        main.cpp
        editor_window.cpp
        map_data.cpp
        map_canvas.cpp
        tool_panel.cpp
        json_edit_dialog.cpp
        resize_dialog.cpp
    )
else()
    add_executable(map_editor
        main.cpp
        editor_window.cpp
        map_data.cpp
        map_canvas.cpp
        tool_panel.cpp
        json_edit_dialog.cpp
        resize_dialog.cpp
    )
endif()

target_link_libraries(map_editor
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

# Keep tool binaries alongside the main game binary for consistent dev runs
# (Makefile expects executables under build/bin)
set_target_properties(map_editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)