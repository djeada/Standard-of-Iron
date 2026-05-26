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

namespace Render::GL::Roman {
namespace {

struct RomanMarketPalette {
  QVector3D limestone{0.96F, 0.94F, 0.88F};
  QVector3D cedar{0.52F, 0.38F, 0.26F};
  QVector3D cedar_dark{0.40F, 0.28F, 0.18F};
  QVector3D cloth_red{0.72F, 0.18F, 0.14F};
  QVector3D cloth_gold{0.85F, 0.72F, 0.28F};
  QVector3D stone_base{0.80F, 0.76F, 0.70F};
};

constexpr std::uint8_t k_marketplace_team_slot = 1;

auto build_marketplace_archetype(BuildingState state) -> RenderArchetype {
  RomanMarketPalette const c;
  float height_multiplier = 1.0F;
  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc("roman_marketplace");

  desc.add_box(
      QVector3D(0.0F, 0.06F, 0.0F), QVector3D(1.2F, 0.06F, 1.2F), c.stone_base);

  float const counter_h = 0.4F * height_multiplier;
  desc.add_box(
      QVector3D(-0.4F, counter_h, 0.0F), QVector3D(0.6F, 0.05F, 0.8F), c.cedar);

  float const post_h = 0.8F * height_multiplier;
  desc.add_box(QVector3D(-0.85F, post_h * 0.5F, -0.7F),
               QVector3D(0.04F, post_h * 0.5F, 0.04F),
               c.cedar_dark);
  desc.add_box(QVector3D(-0.85F, post_h * 0.5F, 0.7F),
               QVector3D(0.04F, post_h * 0.5F, 0.04F),
               c.cedar_dark);
  desc.add_box(QVector3D(0.05F, post_h * 0.5F, -0.7F),
               QVector3D(0.04F, post_h * 0.5F, 0.04F),
               c.cedar_dark);
  desc.add_box(QVector3D(0.05F, post_h * 0.5F, 0.7F),
               QVector3D(0.04F, post_h * 0.5F, 0.04F),
               c.cedar_dark);

  float const awning_y = post_h + 0.04F;
  desc.add_box(QVector3D(-0.4F, awning_y, 0.0F),
               QVector3D(0.55F, 0.02F, 0.8F),
               c.cloth_red,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);

  desc.add_box(QVector3D(-0.4F, awning_y - 0.03F, 0.78F),
               QVector3D(0.55F, 0.02F, 0.04F),
               c.cloth_gold,
               BuildingStateMask::Normal | BuildingStateMask::Damaged);

  float const wall_h = 0.6F * height_multiplier;
  desc.add_box(QVector3D(0.8F, wall_h * 0.5F + 0.12F, 0.0F),
               QVector3D(0.1F, wall_h * 0.5F, 0.9F),
               c.limestone);

  desc.add_palette_box(QVector3D(0.92F, 0.5F * height_multiplier, 0.0F),
                       QVector3D(0.02F, 0.2F, 0.12F),
                       k_marketplace_team_slot,
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
  draw_building_health_bar(out, ctx, {1.0F, 0.08F, 1.2F});
  draw_building_selection_overlay(out, ctx, {1.5F, 1.5F});
}

} // namespace

void register_marketplace_renderer(EntityRendererRegistry& registry) {
  register_building_renderer(registry, "roman", "marketplace", draw_marketplace);
}

} // namespace Render::GL::Roman
