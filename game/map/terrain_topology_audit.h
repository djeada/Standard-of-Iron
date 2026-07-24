#pragma once

#include <QStringList>

#include "map_definition.h"

namespace Game::Map {

struct TerrainTopologyAudit {
  int road_components = 0;
  int river_components = 0;
  int invalid_river_endpoints = 0;
  int hills_without_two_approaches = 0;
  int tactically_unanchored_lakes = 0;
  QStringList issues;

  [[nodiscard]] auto passed() const -> bool { return issues.isEmpty(); }
};

[[nodiscard]] auto
audit_terrain_topology(const MapDefinition& map) -> TerrainTopologyAudit;

} // namespace Game::Map
