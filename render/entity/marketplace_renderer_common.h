#pragma once

#include <string_view>

#include "building_render_common.h"
#include "building_state.h"
#include "registry.h"
#include "render/render_archetype.h"

namespace Render::GL {

using MarketplaceArchetypeResolver = const RenderArchetype& (*)(BuildingState);

struct MarketplaceRendererConfig {
  std::string_view nation_slug;
  MarketplaceArchetypeResolver archetype;
  BuildingSelectionStyle selection;
};

void register_marketplace_renderer_variant(EntityRendererRegistry& registry,
                                           const MarketplaceRendererConfig& config);

} // namespace Render::GL
