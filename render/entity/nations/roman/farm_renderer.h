#pragma once

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/registry.h"

namespace Render::GL::Roman {

void register_farm_renderer(EntityRendererRegistry& registry);

auto build_farm_desc(BuildingState state, int stage) -> BuildingArchetypeDesc;

} // namespace Render::GL::Roman
