# ---- Icon resources ----
# The whole tracked icon directory is embedded rather than a hand-kept list: a
# missing entry only shows up as a blank image at runtime, which is exactly how
# marketplace.png went unnoticed. These files are tracked in git, so the glob is
# reproducible on CI.
file(
    GLOB SOI_UNIT_ICON_ABS
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/visuals/icons/*.png"
)
if(NOT SOI_UNIT_ICON_ABS)
    message(FATAL_ERROR "No icons found under assets/visuals/icons")
endif()
set(SOI_UNIT_ICON_RESOURCES)
foreach(_icon IN LISTS SOI_UNIT_ICON_ABS)
    file(RELATIVE_PATH _icon_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_icon}")
    list(APPEND SOI_UNIT_ICON_RESOURCES "${_icon_rel}")
endforeach()
list(SORT SOI_UNIT_ICON_RESOURCES)
