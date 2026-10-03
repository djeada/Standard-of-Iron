# ---- Translation support ----
# Define default language (can be overridden with -DDEFAULT_LANG=de)
if(NOT DEFINED DEFAULT_LANG)
    set(DEFAULT_LANG "en")
endif()
add_compile_definitions(DEFAULT_LANG="${DEFAULT_LANG}")

# Strings the player reads that live in assets/ instead of source. lupdate only
# parses code, so scripts/extract-asset-strings.py mirrors them into a generated
# stub it can read; the stub is compiled here so a malformed extraction fails at
# build time instead of silently producing an empty catalogue.
#
# The `translations` / `translations-check` Makefile targets drive the
# extraction and the lupdate run. See game/util/asset_text.h.
#
# This one stays tracked on purpose, unlike the .qm below: it is the only place
# a reviewer can see that a change to a mission or map file altered player-facing
# text, and `translations-check` diffs it to catch asset text nobody extracted.
set(SOI_ASSET_STRINGS_CPP "${CMAKE_CURRENT_SOURCE_DIR}/translations/asset_strings_generated.cpp")

# .qm catalogues are compiled from the tracked .ts at build time rather than
# committed. They are derived binaries: unreviewable in a diff, a recurring
# merge conflict, and a standing invitation to ship a .qm that no longer matches
# its .ts. lrelease is already a declared dependency (scripts/setup-deps.sh
# installs qt6-tools), so requiring it costs nothing.
set(SOI_TS_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_en.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_de.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_es.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_pt_br.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_ar.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_tr.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_pl.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/app_ru.ts"
)

# Some distributions ship the lrelease binary without the LinguistTools CMake
# package, so fall back to locating it directly before giving up.
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS LinguistTools)
if(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
    set(SOI_LRELEASE Qt${QT_VERSION_MAJOR}::lrelease)
else()
    get_target_property(_qt_core_location Qt${QT_VERSION_MAJOR}::Core LOCATION)
    get_filename_component(_qt_lib_dir "${_qt_core_location}" DIRECTORY)
    find_program(
        SOI_LRELEASE
        NAMES lrelease-qt${QT_VERSION_MAJOR} lrelease
        HINTS "${_qt_lib_dir}/../bin" "${_qt_lib_dir}/qt${QT_VERSION_MAJOR}/bin"
        PATHS /usr/lib/qt${QT_VERSION_MAJOR}/bin /usr/lib/qt${QT_VERSION_MAJOR}/libexec
    )
    if(NOT SOI_LRELEASE)
        message(
            FATAL_ERROR
            "lrelease not found. It compiles translations/*.ts into the .qm "
            "catalogues embedded in the binary, which are no longer tracked in "
            "git. Install the Qt Linguist tools (Debian/Ubuntu: "
            "qt6-tools-dev-tools, Arch: qt6-tools, Fedora: qt6-qttools-devel) "
            "or run scripts/setup-deps.sh, then re-run CMake."
        )
    endif()
endif()

set(SOI_QM_DIR "${CMAKE_CURRENT_BINARY_DIR}/translations")
file(MAKE_DIRECTORY "${SOI_QM_DIR}")

set(SOI_QM_FILES)
set(_soi_qm_qrc_entries "")
foreach(_ts IN LISTS SOI_TS_FILES)
    get_filename_component(_ts_name "${_ts}" NAME_WE)
    set(_qm "${SOI_QM_DIR}/${_ts_name}.qm")
    add_custom_command(
        OUTPUT "${_qm}"
        COMMAND ${SOI_LRELEASE} -silent "${_ts}" -qm "${_qm}"
        DEPENDS "${_ts}"
        COMMENT "Compiling ${_ts_name}.qm"
        VERBATIM
    )
    list(APPEND SOI_QM_FILES "${_qm}")
    # Absolute path plus an alias keeps the runtime lookup at
    # ":/translations/app_xx.qm" even though the .qm lives in the build tree.
    string(APPEND _soi_qm_qrc_entries "        <file alias=\"${_ts_name}.qm\">${_qm}</file>\n")
endforeach()

set(SOI_TRANSLATIONS_QRC "${CMAKE_CURRENT_BINARY_DIR}/translations_generated.qrc")
file(
    WRITE "${SOI_TRANSLATIONS_QRC}"
    "<RCC>\n    <qresource prefix=\"/translations\">\n${_soi_qm_qrc_entries}    </qresource>\n</RCC>\n"
)
# AUTORCC reads the .qrc at build time but does not know the listed files are
# generated, so the dependency has to be spelled out or a clean build races.
set_source_files_properties("${SOI_TRANSLATIONS_QRC}" PROPERTIES OBJECT_DEPENDS "${SOI_QM_FILES}")
add_custom_target(translations_qm DEPENDS ${SOI_QM_FILES})
