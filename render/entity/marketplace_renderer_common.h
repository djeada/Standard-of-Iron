#pragma once

#include <QVector3D>

#include <array>
#include <span>
#include <string_view>

#include "ambient_people.h"
#include "building_render_common.h"
#include "building_state.h"
#include "building_torches.h"
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
  std::span<const TorchMount> torches{};
  std::span<const AmbientPerson> people{};
  std::span<const WalkSurface> walk_surfaces{};
};

void register_marketplace_renderer_variant(EntityRendererRegistry& registry,
                                           const MarketplaceRendererConfig& config);

} // namespace Render::GL
