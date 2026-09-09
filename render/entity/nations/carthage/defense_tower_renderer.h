#pragma once

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/registry.h"

namespace Render::GL::Carthage {

void register_defense_tower_renderer(EntityRendererRegistry& registry);

auto build_tower_desc(BuildingState state) -> BuildingArchetypeDesc;

} // namespace Render::GL::Carthage
