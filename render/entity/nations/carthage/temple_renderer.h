#pragma once

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/registry.h"

namespace Render::GL::Carthage {

void register_temple_renderer(EntityRendererRegistry& registry);

auto build_temple_desc(BuildingState state) -> BuildingArchetypeDesc;

} // namespace Render::GL::Carthage
