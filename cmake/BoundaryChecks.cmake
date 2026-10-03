# engine core moved under game; no separate engine subdir target
# Layering guard.  render/ reads game state to draw it; the simulation must not
# depend on the renderer.  game_systems does not link render_gl, so symbol-level
# violations already fail to link -- this catches header-only ones at build time
# and names the offending file instead of producing a confusing link error.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(
        check_layering
        ALL
        COMMAND
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-layering.py ${CMAKE_SOURCE_DIR}
        COMMENT "Checking layer dependencies"
        VERBATIM
    )
    # The boundaries *inside* game/. Every module in scripts/module_rules.json
    # is its own CMake target (or shares one with a neighbour it is allowed to
    # use), so the linker enforces the split at symbol level; this is the
    # header-level counterpart, and the only thing that separates the modules
    # that still share an archive. It also fails if game/CMakeLists.txt puts a
    # source in a target its module does not name.
    add_custom_target(
        check_modules
        ALL
        COMMAND
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-modules.py ${CMAKE_SOURCE_DIR}
        COMMENT "Checking module boundaries inside game/"
        VERBATIM
    )
    # The renderer draws the match it was handed and does not resolve the
    # ambient session for itself. That is the precondition for a client
    # rendering state it received rather than state it simulated, so it is a
    # build failure rather than a review comment. See render/world_view.h.
    add_custom_target(
        check_render_boundary
        ALL
        COMMAND
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-render-boundary.py
            ${CMAKE_SOURCE_DIR}
        COMMENT "Checking the render/simulation boundary"
        VERBATIM
    )
    # The client (app/, ui/) submits orders as Game::Command payloads and never
    # applies them itself. That is the precondition for the same client driving
    # a local match, a remote one and a replay, so it is a build failure too.
    # See scripts/check-command-boundary.py.
    add_custom_target(
        check_command_boundary
        ALL
        COMMAND
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-command-boundary.py
            ${CMAKE_SOURCE_DIR}
        COMMENT "Checking the client/command boundary"
        VERBATIM
    )
    # Ambient `X::instance()` lookups are the last thing standing between one
    # match per process and several; their number may only go down. See
    # scripts/check-ambient-instances.py and scripts/ambient_instance_budget.json.
    add_custom_target(
        check_ambient_instances
        ALL
        COMMAND
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-ambient-instances.py
            ${CMAKE_SOURCE_DIR}
        COMMENT "Checking the ambient-lookup budget"
        VERBATIM
    )
    # A radius question answered by a full entity scan costs O(units) per asker,
    # so a system that asks one per unit costs O(units^2) per tick. Local queries
    # go through `world.spatial_index()`; the number of full scans may only go
    # down. See scripts/check-world-scans.py and scripts/world_scan_budget.json.
    add_custom_target(
        check_world_scans
        ALL
        COMMAND
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-world-scans.py
            ${CMAKE_SOURCE_DIR}
        COMMENT "Checking the full-world-scan budget"
        VERBATIM
    )
    # Entity-object component access (`entity->get_component<T>()`) is the shape
    # the ECS is migrating away from; the registry answers the same question
    # from the component side. Their number may only go down. See
    # scripts/check-entity-access.py and scripts/entity_access_budget.json.
    add_custom_target(
        check_entity_access
        ALL
        COMMAND
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check-entity-access.py
            ${CMAKE_SOURCE_DIR}
        COMMENT "Checking the entity-centric component access budget"
        VERBATIM
    )
else()
    message(STATUS "Python3 not found; skipping the layering and module checks")
endif()
