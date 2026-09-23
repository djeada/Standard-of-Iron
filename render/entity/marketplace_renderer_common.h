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

struct MarketAwning {
  QVector3D back;
  QVector3D front;
  QVector3D width_axis{0.0F, 0.0F, 1.0F};
  float half_width{0.3F};
  QVector3D stripe_a;
  QVector3D stripe_b;
  int stripes{4};
};

struct MarketHanging {
  QVector3D pivot;
  float length{0.12F};
  float radius{0.03F};
  QVector3D color;
  int beads{3};
};

void submit_market_awnings(const DrawContext& ctx,
                           ISubmitter& out,
                           std::span<const MarketAwning> awnings,
                           std::span<const MarketHanging> hangings);

struct MarketplaceRendererConfig {
  std::string_view nation_slug;
  MarketplaceArchetypeResolver archetype;

  MarketplacePaletteSlotsResolver palette_slots;
  BuildingSelectionStyle selection;
  std::span<const TorchMount> torches{};
  std::span<const AmbientPerson> people{};
  std::span<const WalkSurface> walk_surfaces{};
  std::span<const MarketAwning> awnings{};
  std::span<const MarketHanging> hangings{};
};

void register_marketplace_renderer_variant(EntityRendererRegistry& registry,
                                           const MarketplaceRendererConfig& config);

} // namespace Render::GL
