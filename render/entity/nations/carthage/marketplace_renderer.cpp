#include "marketplace_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

#include "../../../../game/core/component.h"
#include "../../../../game/visuals/team_colors.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../render_archetype.h"
#include "../../../submitter.h"
#include "../../../template_cache.h"
#include "../../building_archetype_desc.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../registry.h"

namespace Render::GL::Carthage {
namespace {

struct CarthageMarketPalette {
  QVector3D sandstone{0.88F, 0.82F, 0.68F};
  QVector3D wood_dark{0.36F, 0.24F, 0.14F};
  QVector3D wood_medium{0.48F, 0.34F, 0.22F};
  QVector3D cloth_purple{0.45F, 0.15F, 0.42F};
  QVector3D cloth_gold{0.82F, 0.68F, 0.22F};
  QVector3D brick{0.72F, 0.52F, 0.38F};
};

constexpr std::uint8_t k_marketplace_team_slot = 1;

auto build_marketplace_archetype(BuildingState state) -> RenderArchetype {
  CarthageMarketPalette const c;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc("carthage_marketplace");

  desc.add_box(QVector3D(0.0F, 0.06F, 0.0F), QVector3D(1.2F, 0.06F, 1.2F), c.brick);

  float const counter_h = 0.38F * height_multiplier;
  desc.add_box(QVector3D(-0.35F, counter_h, 0.0F), QVector3D(0.55F, 0.05F, 0.75F), c.wood_medium);

  float const post_h = 0.78F * height_multiplier;
  desc.add_box(QVector3D(-0.8F, post_h * 0.5F, -0.65F), QVector3D(0.05F, post_h * 0.5F, 0.05F), c.wood_dark);
  desc.add_box(QVector3D(-0.8F, post_h * 0.5F, 0.65F), QVector3D(0.05F, post_h * 0.5F, 0.05F), c.wood_dark);
  desc.add_box(QVector3D(0.1F, post_h * 0.5F, -0.65F), QVector3D(0.05F, post_h * 0.5F, 0.05F), c.wood_dark);
  desc.add_box(QVector3D(0.1F, post_h * 0.5F, 0.65F), QVector3D(0.05F, post_h * 0.5F, 0.05F), c.wood_dark);

  float const awning_y = post_h + 0.04F;
  desc.add_box(QVector3D(-0.35F, awning_y, 0.0F), QVector3D(0.52F, 0.02F, 0.75F), c.cloth_purple,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);

  desc.add_box(QVector3D(-0.35F, awning_y - 0.03F, 0.73F), QVector3D(0.52F, 0.02F, 0.04F), c.cloth_gold,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);

  float const wall_h = 0.58F * height_multiplier;
  desc.add_box(QVector3D(0.78F, wall_h * 0.5F + 0.12F, 0.0F), QVector3D(0.12F, wall_h * 0.5F, 0.85F), c.sandstone);

  desc.add_palette_box(QVector3D(0.92F, 0.48F * height_multiplier, 0.0F),
                       QVector3D(0.02F, 0.18F, 0.12F), k_marketplace_team_slot,
                       BuildingStateMask::Normal | BuildingStateMask::Damaged);

  return build_building_archetype(desc, state);
}

void draw_marketplace(const DrawContext& ctx, ISubmitter& out) {
  static auto normal = build_marketplace_archetype(BuildingState::Normal);
  static auto damaged = build_marketplace_archetype(BuildingState::Damaged);
  static auto destroyed = build_marketplace_archetype(BuildingState::Destroyed);

  BuildingState const state = resolve_building_state(ctx);
  const RenderArchetype& archetype = (state == BuildingState::Destroyed) ? destroyed
                                   : (state == BuildingState::Damaged) ? damaged
                                   : normal;
  submit_building_instance(out, ctx, archetype);
  draw_building_health_bar(out, ctx, {1.0F, 0.08F, 1.1F});
  draw_building_selection_overlay(out, ctx, {1.5F, 1.5F});
}

} // namespace

void register_marketplace_renderer(EntityRendererRegistry& registry) {
  register_building_renderer(registry, "carthage", "marketplace", draw_marketplace);
}

} // namespace Render::GL::Carthage
