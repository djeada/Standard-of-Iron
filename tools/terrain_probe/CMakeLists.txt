# The heightfield an authored map actually produces, dumped for the placement
# audit in scripts/fix-map-prop-overlaps.py.
#
# The engine's hills are not the ellipses they are authored as: the boundary is
# warped by fbm, roughened by up to +-34%, unioned with an off-centre lobe,
# widened and hash-rotated at campaign scale, and then cut open by ramp
# corridors that reach beyond the footprint entirely. None of that is derivable
# from the map JSON, so the audit asks the engine instead of modelling it twice.
add_executable(terrain_probe main.cpp)

target_link_libraries(terrain_probe PRIVATE Qt${QT_VERSION_MAJOR}::Core soi_missions)

target_include_directories(terrain_probe PRIVATE ${CMAKE_SOURCE_DIR})

set_target_properties(terrain_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
