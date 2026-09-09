#pragma once

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/registry.h"

namespace Render::GL::Roman {

void register_marketplace_renderer(EntityRendererRegistry& registry);

auto build_marketplace_desc(BuildingState state) -> BuildingArchetypeDesc;

} // namespace Render::GL::Roman
