add_executable(content_validator
    main.cpp
    ../../game/map/mission_loader.cpp
    ../../game/map/campaign_loader.cpp
)

target_link_libraries(content_validator
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
)

target_include_directories(content_validator
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

set_target_properties(content_validator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
