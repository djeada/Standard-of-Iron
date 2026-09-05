#pragma once

#include <QVector3D>

#include <array>
#include <string_view>

#include "building_render_common.h"
#include "building_state.h"
#include "registry.h"
#include "render/render_archetype.h"

namespace Render::GL {

using MarketplaceArchetypeResolver = const RenderArchetype& (*)(BuildingState);
using MarketplacePaletteSlotsResolver = std::array<QVector3D, 1> (*)(const QVector3D&);

struct MarketplaceRendererConfig {
  std::string_view nation_slug;
  MarketplaceArchetypeResolver archetype;

  MarketplacePaletteSlotsResolver palette_slots;
  BuildingSelectionStyle selection;
};

void register_marketplace_renderer_variant(EntityRendererRegistry& registry,
                                           const MarketplaceRendererConfig& config);

} // namespace Render::GL
