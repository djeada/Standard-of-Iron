add_library(engine_core STATIC
    core/entity.cpp
    core/component.cpp
    core/system.cpp
    core/world.cpp
    core/event_manager.cpp
    core/serialization.cpp
)

target_include_directories(engine_core PUBLIC .)
target_link_libraries(engine_core PUBLIC Qt6::Core)