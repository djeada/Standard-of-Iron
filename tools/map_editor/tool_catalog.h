#pragma once

#include <QString>
#include <QVector>

#include <span>

#include "tool_type.h"

namespace MapEditor {

enum class ToolSection {
  Selection,
  Terrain,
  WorldProps,
  TroopsInfantry,
  TroopsMounted,
  TroopsCommand,
  Sepulcher,
  Wildlife,
  Paths,
  Fortifications,
  Structures,
};

enum class ToolPlacement {
  None,
  Erase,
  Terrain,
  WorldProp,
  Structure,
  Gate,
  Troop,
  Forest,
  Wildlife,
  UndeadZone,
  Linear,
};

struct ToolSpec {
  ToolType tool;
  ToolSection section;
  ToolPlacement placement;
  const char* name;
  const char* card_label;
  const char* glyph;
  const char* element_type;
  const char* description;
  const char* usage_hint;
};

[[nodiscard]] auto tool_catalog() -> std::span<const ToolSpec>;
[[nodiscard]] auto tool_spec(ToolType tool) -> const ToolSpec&;
[[nodiscard]] auto tools_in_section(ToolSection section) -> QVector<const ToolSpec*>;
[[nodiscard]] auto tool_sections() -> std::span<const ToolSection>;
[[nodiscard]] auto section_title(ToolSection section) -> QString;
[[nodiscard]] auto section_columns(ToolSection section) -> int;

[[nodiscard]] auto tool_name(ToolType tool) -> QString;
[[nodiscard]] auto tool_card_label(ToolType tool) -> QString;
[[nodiscard]] auto tool_description(ToolType tool) -> QString;
[[nodiscard]] auto tool_status_label(ToolType tool) -> QString;
[[nodiscard]] auto tool_placement(ToolType tool) -> ToolPlacement;
[[nodiscard]] auto element_type_for_tool(ToolType tool) -> QString;
[[nodiscard]] auto tool_matches_filter(const ToolSpec& spec,
                                       const QString& filter) -> bool;

[[nodiscard]] inline auto is_troop_tool(ToolType tool) -> bool {
  return tool_placement(tool) == ToolPlacement::Troop;
}

[[nodiscard]] inline auto is_linear_tool(ToolType tool) -> bool {
  return tool_placement(tool) == ToolPlacement::Linear;
}

} // namespace MapEditor
