#include "home_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../geom/math_utils.h"
#include "../../../submitter.h"
#include "../../building_archetype_desc.h"
#include "../../building_render_common.h"
#include "../../building_state.h"
#include "../../registry.h"

#include <QVector3D>
#include <algorithm>

namespace Render::GL::Carthage {
namespace {

using Render::Geom::clamp_vec_01;

struct CarthagePalette {
  QVector3D stone_light{0.62F, 0.60F, 0.58F};
  QVector3D stone_dark{0.50F, 0.48F, 0.46F};
  QVector3D stone_base{0.55F, 0.53F, 0.51F};
  QVector3D brick{0.75F, 0.52F, 0.42F};
  QVector3D brick_dark{0.62F, 0.42F, 0.32F};
  QVector3D tile_red{0.72F, 0.40F, 0.30F};
  QVector3D tile_dark{0.58F, 0.30F, 0.22F};
  QVector3D wood{0.42F, 0.28F, 0.16F};
  QVector3D wood_dark{0.32F, 0.20F, 0.10F};
  QVector3D team{0.8F, 0.9F, 1.0F};
  QVector3D team_trim{0.48F, 0.54F, 0.60F};
};

inline auto make_palette(const QVector3D &team) -> CarthagePalette {
  CarthagePalette p;
  p.team = clamp_vec_01(team);
  p.team_trim = clamp_vec_01(
      QVector3D(team.x() * 0.6F, team.y() * 0.6F, team.z() * 0.6F));
  return p;
}

auto build_home_archetype(BuildingState state) -> RenderArchetype {
  CarthagePalette const c = make_palette(QVector3D(1.0F, 1.0F, 1.0F));
  float const wall_height = 0.8F;
  float height_multiplier = 1.0F;

  if (state == BuildingState::Damaged) {
    height_multiplier = 0.7F;
  } else if (state == BuildingState::Destroyed) {
    height_multiplier = 0.4F;
  }

  BuildingArchetypeDesc desc("carthage_home");

  desc.add_box(QVector3D(0.0F, 0.1F, 0.0F), QVector3D(1.0F, 0.1F, 1.0F),
               c.stone_base);
  desc.add_box(
      QVector3D(0.0F, wall_height * 0.5F * height_multiplier + 0.2F, -0.9F),
      QVector3D(0.85F, wall_height * 0.5F * height_multiplier, 0.08F), c.brick);
  desc.add_box(
      QVector3D(0.0F, wall_height * 0.5F * height_multiplier + 0.2F, 0.9F),
      QVector3D(0.85F, wall_height * 0.5F * height_multiplier, 0.08F), c.brick);
  desc.add_box(
      QVector3D(-0.9F, wall_height * 0.5F * height_multiplier + 0.2F, 0.0F),
      QVector3D(0.08F, wall_height * 0.5F * height_multiplier, 0.8F), c.brick);
  desc.add_box(
      QVector3D(0.9F, wall_height * 0.5F * height_multiplier + 0.2F, 0.0F),
      QVector3D(0.08F, wall_height * 0.5F * height_multiplier, 0.8F), c.brick);

  desc.add_box(QVector3D(0.0F, 1.15F, 0.0F), QVector3D(1.0F, 0.05F, 1.0F),
               c.tile_red, kBuildingStateMaskIntact);
  for (float z = -0.8F; z <= 0.8F; z += 0.3F) {
    desc.add_box(QVector3D(0.0F, 1.18F, z), QVector3D(0.95F, 0.02F, 0.06F),
                 c.tile_dark, kBuildingStateMaskIntact);
  }

  desc.add_box(QVector3D(0.0F, 0.4F, 0.95F), QVector3D(0.3F, 0.4F, 0.05F),
               c.wood_dark);
  return build_building_archetype(desc, state);
}

auto home_archetype(BuildingState state) -> const RenderArchetype & {
  static const BuildingArchetypeSet k_set =
      build_stateful_building_archetype_set(build_home_archetype);
  return k_set.for_state(state);
}

void draw_home(const DrawContext &p, ISubmitter &out) {
  if (p.entity == nullptr) {
    return;
  }

  auto *r = p.entity->get_component<Engine::Core::RenderableComponent>();
  if (r == nullptr) {
    return;
  }

  submit_building_instance(out, p, home_archetype(resolve_building_state(p)));
  draw_building_health_bar(out, p, BuildingHealthBarStyle{1.0F, 0.08F, 1.5F});
  draw_building_selection_overlay(out, p, BuildingSelectionStyle{1.4F, 1.4F});
}

} // namespace

void register_home_renderer(Render::GL::EntityRendererRegistry &registry) {
  register_building_renderer(registry, "carthage", "home", draw_home);
}

} // namespace Render::GL::Carthage
