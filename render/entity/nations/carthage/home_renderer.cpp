#include "home_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../geom/math_utils.h"
#include "../../../gl/primitives.h"
#include "../../../gl/resources.h"
#include "../../../render_archetype.h"
#include "../../../submitter.h"
#include "../../building_state.h"
#include "../../registry.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>

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

void submit_box(ISubmitter &out, Texture *white, const QMatrix4x4 &model,
                const QVector3D &pos, const QVector3D &size,
                const QVector3D &color) {
  QMatrix4x4 m = model;
  m.translate(pos);
  m.scale(size);
  out.mesh(get_unit_cube(), m, color, white, 1.0F);
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

  RenderArchetypeBuilder builder("carthage_home");
  builder.set_max_distance(std::numeric_limits<float>::infinity());

  builder.add_box(QVector3D(0.0F, 0.1F, 0.0F), QVector3D(1.0F, 0.1F, 1.0F),
                  c.stone_base);
  builder.add_box(
      QVector3D(0.0F, wall_height * 0.5F * height_multiplier + 0.2F, -0.9F),
      QVector3D(0.85F, wall_height * 0.5F * height_multiplier, 0.08F), c.brick);
  builder.add_box(
      QVector3D(0.0F, wall_height * 0.5F * height_multiplier + 0.2F, 0.9F),
      QVector3D(0.85F, wall_height * 0.5F * height_multiplier, 0.08F), c.brick);
  builder.add_box(
      QVector3D(-0.9F, wall_height * 0.5F * height_multiplier + 0.2F, 0.0F),
      QVector3D(0.08F, wall_height * 0.5F * height_multiplier, 0.8F), c.brick);
  builder.add_box(
      QVector3D(0.9F, wall_height * 0.5F * height_multiplier + 0.2F, 0.0F),
      QVector3D(0.08F, wall_height * 0.5F * height_multiplier, 0.8F), c.brick);

  if (state != BuildingState::Destroyed) {
    builder.add_box(QVector3D(0.0F, 1.15F, 0.0F), QVector3D(1.0F, 0.05F, 1.0F),
                    c.tile_red);
    for (float z = -0.8F; z <= 0.8F; z += 0.3F) {
      builder.add_box(QVector3D(0.0F, 1.18F, z), QVector3D(0.95F, 0.02F, 0.06F),
                      c.tile_dark);
    }
  }

  builder.add_box(QVector3D(0.0F, 0.4F, 0.95F), QVector3D(0.3F, 0.4F, 0.05F),
                  c.wood_dark);
  return std::move(builder).build();
}

auto home_archetype(BuildingState state) -> const RenderArchetype & {
  static const RenderArchetype k_normal =
      build_home_archetype(BuildingState::Normal);
  static const RenderArchetype k_damaged =
      build_home_archetype(BuildingState::Damaged);
  static const RenderArchetype k_destroyed =
      build_home_archetype(BuildingState::Destroyed);

  switch (state) {
  case BuildingState::Normal:
    return k_normal;
  case BuildingState::Damaged:
    return k_damaged;
  case BuildingState::Destroyed:
    return k_destroyed;
  }
  return k_normal;
}

void draw_health_bar(const DrawContext &p, ISubmitter &out, Texture *white) {
  if (p.entity == nullptr) {
    return;
  }
  auto *u = p.entity->get_component<Engine::Core::UnitComponent>();
  if (u == nullptr) {
    return;
  }

  float const ratio =
      std::clamp(u->health / float(std::max(1, u->max_health)), 0.0F, 1.0F);
  if (ratio <= 0.0F) {
    return;
  }

  auto *capture = p.entity->get_component<Engine::Core::CaptureComponent>();
  bool under_attack = (capture != nullptr && capture->is_being_captured);

  if (!under_attack && u->health >= u->max_health) {
    return;
  }

  float const bar_width = 1.0F;
  float const bar_height = 0.08F;
  float const bar_y = 1.5F;
  float const border_thickness = 0.012F;

  if (under_attack) {
    float pulse = HEALTHBAR_PULSE_MIN +
                  HEALTHBAR_PULSE_AMPLITUDE *
                      sinf(p.animation_time * HEALTHBAR_PULSE_SPEED);
    submit_box(out, white, p.model, QVector3D(0.0F, bar_y, 0.0F),
               QVector3D(bar_width * 0.5F + border_thickness * 3.0F,
                         bar_height * 0.5F + border_thickness * 3.0F, 0.095F),
               HealthBarColors::GLOW_ATTACK * pulse * 0.6F);
  }

  submit_box(out, white, p.model, QVector3D(0.0F, bar_y, 0.0F),
             QVector3D(bar_width * 0.5F + border_thickness,
                       bar_height * 0.5F + border_thickness, 0.09F),
             HealthBarColors::BORDER);

  submit_box(out, white, p.model, QVector3D(0.0F, bar_y, 0.0F),
             QVector3D(bar_width * 0.5F + border_thickness * 0.5F,
                       bar_height * 0.5F + border_thickness * 0.5F, 0.088F),
             HealthBarColors::INNER_BORDER);

  submit_box(out, white, p.model, QVector3D(0.0F, bar_y + 0.003F, 0.0F),
             QVector3D(bar_width * 0.5F, bar_height * 0.5F, 0.085F),
             HealthBarColors::BACKGROUND);

  QVector3D fg_color;
  QVector3D fg_dark;

  if (ratio >= HEALTH_THRESHOLD_NORMAL) {
    fg_color = HealthBarColors::NORMAL_BRIGHT;
    fg_dark = HealthBarColors::NORMAL_DARK;
  } else if (ratio >= HEALTH_THRESHOLD_DAMAGED) {
    float t = (ratio - HEALTH_THRESHOLD_DAMAGED) /
              (HEALTH_THRESHOLD_NORMAL - HEALTH_THRESHOLD_DAMAGED);
    fg_color = HealthBarColors::NORMAL_BRIGHT * t +
               HealthBarColors::DAMAGED_BRIGHT * (1.0F - t);
    fg_dark = HealthBarColors::NORMAL_DARK * t +
              HealthBarColors::DAMAGED_DARK * (1.0F - t);
  } else {
    float t = ratio / HEALTH_THRESHOLD_DAMAGED;
    fg_color = HealthBarColors::DAMAGED_BRIGHT * t +
               HealthBarColors::CRITICAL_BRIGHT * (1.0F - t);
    fg_dark = HealthBarColors::DAMAGED_DARK * t +
              HealthBarColors::CRITICAL_DARK * (1.0F - t);
  }

  submit_box(
      out, white, p.model,
      QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F, bar_y + 0.005F, 0.0F),
      QVector3D(bar_width * ratio * 0.5F, bar_height * 0.48F, 0.08F), fg_dark);

  submit_box(
      out, white, p.model,
      QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F, bar_y + 0.008F, 0.0F),
      QVector3D(bar_width * ratio * 0.5F, bar_height * 0.40F, 0.078F),
      fg_color);

  QVector3D const highlight = fg_color * 1.6F;
  submit_box(out, white, p.model,
             QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F,
                       bar_y + bar_height * 0.35F, 0.0F),
             QVector3D(bar_width * ratio * 0.5F, bar_height * 0.20F, 0.075F),
             clamp_vec_01(highlight));

  submit_box(out, white, p.model,
             QVector3D(-(bar_width * (1.0F - ratio)) * 0.5F,
                       bar_y + bar_height * 0.48F, 0.0F),
             QVector3D(bar_width * ratio * 0.5F, bar_height * 0.08F, 0.073F),
             HealthBarColors::SHINE * 0.8F);

  float marker_70_x = bar_width * 0.5F * (HEALTH_THRESHOLD_NORMAL - 0.5F);
  submit_box(out, white, p.model, QVector3D(marker_70_x, bar_y, 0.0F),
             QVector3D(0.015F, bar_height * 0.55F, 0.09F),
             HealthBarColors::SEGMENT);

  float marker_30_x = bar_width * 0.5F * (HEALTH_THRESHOLD_DAMAGED - 0.5F);
  submit_box(out, white, p.model, QVector3D(marker_30_x, bar_y, 0.0F),
             QVector3D(0.015F, bar_height * 0.55F, 0.09F),
             HealthBarColors::SEGMENT);
}

void draw_selection(const DrawContext &p, ISubmitter &out) {
  QMatrix4x4 m;
  QVector3D const pos = p.model.column(3).toVector3D();
  m.translate(pos.x(), 0.0F, pos.z());
  m.scale(1.4F, 1.0F, 1.4F);
  if (p.selected) {
    out.selection_smoke(m, QVector3D(0.2F, 0.85F, 0.2F), 0.35F);
  } else if (p.hovered) {
    out.selection_smoke(m, QVector3D(0.95F, 0.92F, 0.25F), 0.22F);
  }
}

void draw_home(const DrawContext &p, ISubmitter &out) {
  if (p.entity == nullptr) {
    return;
  }

  auto *r = p.entity->get_component<Engine::Core::RenderableComponent>();
  auto *u = p.entity->get_component<Engine::Core::UnitComponent>();
  if (r == nullptr) {
    return;
  }

  BuildingState state = BuildingState::Normal;
  if (u != nullptr) {
    float const health_ratio =
        std::clamp(u->health / float(std::max(1, u->max_health)), 0.0F, 1.0F);
    state = get_building_state(health_ratio);
  }

  Texture *white = (p.resources != nullptr) ? p.resources->white() : nullptr;

  const RenderArchetype &archetype = home_archetype(state);
  RenderInstance instance;
  instance.archetype = &archetype;
  instance.world = p.model;
  instance.default_texture = white;
  instance.lod = RenderArchetypeLod::Full;
  submit_render_instance(out, instance);

  draw_health_bar(p, out, white);
  draw_selection(p, out);
}

} // namespace

void register_home_renderer(Render::GL::EntityRendererRegistry &registry) {
  registry.register_renderer("troops/carthage/home", draw_home);
}

} // namespace Render::GL::Carthage
