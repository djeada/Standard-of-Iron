#pragma once

#include <string_view>

#include "../render_archetype.h"
#include "building_render_common.h"
#include "building_state.h"
#include "registry.h"

namespace Render::GL {

using MarketplaceArchetypeResolver = const RenderArchetype& (*)(BuildingState);

struct MarketplaceRendererConfig {
  std::string_view nation_slug;
  MarketplaceArchetypeResolver archetype;
  BuildingHealthBarStyle health_bar;
  BuildingSelectionStyle selection;
};

void register_marketplace_renderer_variant(EntityRendererRegistry& registry,
                                           const MarketplaceRendererConfig& config);

} // namespace Render::GL
