#pragma once

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/registry.h"

namespace Render::GL::Roman {

void register_home_renderer(EntityRendererRegistry& registry);

auto build_home_desc(BuildingState state) -> BuildingArchetypeDesc;

} // namespace Render::GL::Roman
