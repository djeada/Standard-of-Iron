#pragma once

#include "map_data.h"
#include "tool_type.h"

namespace MapEditor::CanvasDetail {

inline auto default_terrain_element(ToolType tool) -> TerrainElement {
  TerrainElement elem;
  elem.type =
      tool == ToolType::Mountain ? QStringLiteral("mountain") : QStringLiteral("hill");
  elem.height = tool == ToolType::Mountain ? 8.0F : 3.0F;
  elem.radius = 10.0F;

  switch (tool) {
  case ToolType::HillRidge:
    elem.shape = QStringLiteral("corridor");
    elem.radius = 0.0F;
    elem.width = 36.0F;
    elem.depth = 10.0F;
    break;
  case ToolType::HillArc:
    elem.shape = QStringLiteral("arc");
    elem.radius = 0.0F;
    elem.width = 40.0F;
    elem.depth = 40.0F;
    elem.thickness = 10.0F;
    elem.has_arc = true;
    elem.arc = 120.0F;
    elem.taper = 0.35F;
    break;
  case ToolType::HillElbow:
    elem.shape = QStringLiteral("elbow");
    elem.radius = 0.0F;
    elem.width = 34.0F;
    elem.depth = 34.0F;
    elem.thickness = 10.0F;
    break;
  case ToolType::HillRing:
    elem.shape = QStringLiteral("ring");
    elem.radius = 0.0F;
    elem.width = 40.0F;
    elem.depth = 40.0F;
    elem.thickness = 10.0F;
    break;
  default:
    break;
  }
  return elem;
}

inline auto default_wildlife_radius(const QString& species) -> float {
  if (species == QLatin1String("wolves")) {
    return 22.0F;
  }
  if (species == QLatin1String("birds")) {
    return 26.0F;
  }
  return 14.0F;
}

} // namespace MapEditor::CanvasDetail
