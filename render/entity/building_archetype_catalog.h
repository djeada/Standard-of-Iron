#pragma once

#include <functional>
#include <string>
#include <vector>

#include "building_archetype_desc.h"
#include "building_state.h"

namespace Render::GL {

struct BuildingArchetypeCatalogEntry {
  std::string name;
  std::function<BuildingArchetypeDesc(BuildingState)> build;
};

auto building_archetype_catalog() -> const std::vector<BuildingArchetypeCatalogEntry>&;

} // namespace Render::GL
