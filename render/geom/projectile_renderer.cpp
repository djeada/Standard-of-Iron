#include "projectile_renderer.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../game/systems/arrow_projectile.h"
#include "../game/systems/projectile_system.h"
#include "../game/systems/stone_projectile.h"
#include "../gl/primitives.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "arrow.h"
#include "stone.h"

namespace Render::GL {

void render_arrow_projectile(Renderer* renderer,
                             ResourceManager* resources,
                             const Game::Systems::ArrowProjectile& arrow,
                             const QVector3D& pos,
                             const QMatrix4x4& base_model) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }

  auto* arrow_shaft_mesh = Geom::Arrow::get_shaft();
  auto* arrow_tip_mesh = Geom::Arrow::get_tip();
  auto* arrow_fletching_mesh = Geom::Arrow::get_fletching();
  if ((arrow_shaft_mesh == nullptr) || (arrow_tip_mesh == nullptr) ||
      (arrow_fletching_mesh == nullptr)) {
    return;
  }

  const QVector3D delta = arrow.get_end() - arrow.get_start();
  const float dist = std::max(0.001F, delta.length());

  QMatrix4x4 model = base_model;

  constexpr float k_arc_height_multiplier = 8.0F;
  constexpr float k_arc_center_offset = 0.5F;
  float const vy = (arrow.get_end().y() - arrow.get_start().y()) / dist;
  float const pitch_deg =
      -std::atan2(vy - (k_arc_height_multiplier * arrow.get_arc_height() *
                        (arrow.get_progress() - k_arc_center_offset) / dist),
                  1.0F) *
      (180.0F / std::numbers::pi_v<float>);
  model.rotate(pitch_deg, QVector3D(1, 0, 0));

  if (arrow.get_kind() == Game::Systems::ProjectileKind::Fireball) {
    auto* fireball_mesh = get_unit_sphere();
    if (fireball_mesh == nullptr) {
      return;
    }

    float const animation_time = renderer->get_animation_time();
    float const pulse =
        0.92F + 0.10F * std::sin(animation_time * 12.0F + arrow.get_progress() * 32.0F);
    float const spell_phase = animation_time + arrow.get_progress() * 3.7F;

    renderer->fireball(
        pos, QVector3D(1.0F, 0.44F, 0.10F), 0.21F * pulse, 1.35F, spell_phase);

    for (int trail_idx = 1; trail_idx <= 3; ++trail_idx) {
      float const trail_t =
          arrow.get_progress() - static_cast<float>(trail_idx) * 0.06F;
      if (trail_t < 0.0F) {
        continue;
      }

      QVector3D trail_pos = arrow.get_start() + delta * trail_t;
      float const trail_h = arrow.get_arc_height() * 4.0F * trail_t * (1.0F - trail_t);
      trail_pos.setY(trail_pos.y() + trail_h);

      float const trail_falloff = 1.0F - static_cast<float>(trail_idx) * 0.18F;
      renderer->fireball(trail_pos,
                         QVector3D(1.0F, 0.30F, 0.06F),
                         std::max(0.075F, 0.13F * trail_falloff),
                         std::max(0.18F, 0.72F * trail_falloff),
                         spell_phase - static_cast<float>(trail_idx) * 0.11F);
    }

    QMatrix4x4 core_model = model;
    core_model.scale(0.092F * pulse, 0.092F * pulse, 0.092F * pulse);
    renderer->mesh(
        fireball_mesh, core_model, QVector3D(1.0F, 0.84F, 0.22F), nullptr, 1.0F);

    QMatrix4x4 ember_model = model;
    ember_model.scale(0.044F, 0.044F, 0.044F);
    renderer->mesh(
        fireball_mesh, ember_model, QVector3D(1.0F, 0.97F, 0.62F), nullptr, 1.0F);

    QVector3D const orbit_offset(std::sin(spell_phase * 7.3F) * 0.048F,
                                 std::cos(spell_phase * 5.2F) * 0.026F,
                                 std::cos(spell_phase * 6.4F) * 0.048F);
    QMatrix4x4 spark_model = model;
    spark_model.translate(orbit_offset);
    spark_model.scale(0.021F, 0.021F, 0.021F);
    renderer->mesh(
        fireball_mesh, spark_model, QVector3D(1.0F, 0.94F, 0.48F), nullptr, 1.0F);
    return;
  }

  if (arrow.is_ballista_bolt()) {

    float const spin_speed = 1440.0F;
    float const spin_angle = arrow.get_progress() * spin_speed;
    model.rotate(spin_angle, QVector3D(0, 0, 1));

    constexpr float bolt_z_scale = 0.85F;
    constexpr float bolt_xy_scale = 0.48F;
    constexpr float bolt_z_translate_factor = 0.5F;

    QMatrix4x4 bolt_model = model;
    bolt_model.translate(0.0F, 0.0F, -bolt_z_scale * bolt_z_translate_factor);
    bolt_model.scale(bolt_xy_scale, bolt_xy_scale, bolt_z_scale);

    QVector3D const base_color = arrow.get_color();

    QVector3D const wood_color =
        QVector3D(std::clamp(base_color.x() * 0.6F + 0.35F, 0.0F, 1.0F),
                  std::clamp(base_color.y() * 0.55F + 0.30F, 0.0F, 1.0F),
                  std::clamp(base_color.z() * 0.5F + 0.15F, 0.0F, 1.0F));

    renderer->mesh(arrow_shaft_mesh, bolt_model, wood_color, nullptr, 1.0F);

    QMatrix4x4 tip_model = bolt_model;

    tip_model.translate(0.0F, 0.0F, bolt_z_scale * 0.3F);
    tip_model.scale(0.85F, 0.85F, 0.2F);

    QVector3D const iron_color = QVector3D(0.25F, 0.24F, 0.22F);
    renderer->mesh(arrow_tip_mesh, tip_model, iron_color, nullptr, 1.0F);

    QMatrix4x4 fletch_model = bolt_model;
    fletch_model.translate(0.0F, 0.0F, -bolt_z_scale * 0.2F);
    fletch_model.scale(0.75F, 0.75F, 0.15F);

    QVector3D const fletch_color =
        QVector3D(std::clamp(wood_color.x() * 1.15F, 0.0F, 1.0F),
                  std::clamp(wood_color.y() * 1.10F, 0.0F, 1.0F),
                  std::clamp(wood_color.z() * 0.95F, 0.0F, 1.0F));
    renderer->mesh(arrow_fletching_mesh, fletch_model, fletch_color, nullptr, 0.85F);

    if (arrow.get_progress() > 0.15F) {
      float const trail_opacity =
          std::clamp((arrow.get_progress() - 0.15F) / 0.85F, 0.0F, 0.3F);

      for (int trail_idx = 1; trail_idx <= 2; trail_idx++) {
        float const trail_t = arrow.get_progress() - (trail_idx * 0.08F);
        if (trail_t >= 0.0F) {
          QVector3D trail_pos = arrow.get_start() + delta * trail_t;
          float const trail_h =
              arrow.get_arc_height() * 4.0F * trail_t * (1.0F - trail_t);
          trail_pos.setY(trail_pos.y() + trail_h);

          QMatrix4x4 trail_model;
          trail_model.translate(trail_pos.x(), trail_pos.y(), trail_pos.z());

          constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;
          QVector3D const dir = delta.normalized();
          float const yaw_deg = std::atan2(dir.x(), dir.z()) * k_rad_to_deg;
          trail_model.rotate(yaw_deg, QVector3D(0, 1, 0));
          trail_model.rotate(pitch_deg, QVector3D(1, 0, 0));

          float const trail_spin_angle = trail_t * spin_speed;
          trail_model.rotate(trail_spin_angle, QVector3D(0, 0, 1));

          float const trail_scale_factor = 0.6F - (trail_idx * 0.15F);
          trail_model.translate(0.0F, 0.0F, -bolt_z_scale * bolt_z_translate_factor);
          trail_model.scale(bolt_xy_scale * trail_scale_factor,
                            bolt_xy_scale * trail_scale_factor,
                            bolt_z_scale * trail_scale_factor);

          QVector3D const trail_color = wood_color * (1.0F - trail_opacity * 0.4F);
          renderer->mesh(arrow_shaft_mesh,
                         trail_model,
                         trail_color,
                         nullptr,
                         1.0F - (trail_opacity * 0.7F));
        }
      }
    }
  } else {

    constexpr float arrow_z_scale = Geom::Arrow::k_arrow_z_scale;
    constexpr float arrow_xy_scale = Geom::Arrow::k_arrow_xy_scale;
    constexpr float arrow_z_translate_factor = Geom::Arrow::k_arrow_z_translate_factor;
    model.translate(0.0F, 0.0F, -arrow_z_scale * arrow_z_translate_factor);
    model.scale(arrow_xy_scale, arrow_xy_scale, arrow_z_scale);

    QVector3D const team_color = arrow.get_color();
    QVector3D shaft_color = Geom::Arrow::shaft_color(team_color);
    QVector3D tip_color = Geom::Arrow::tip_color();
    QVector3D fletch_color = Geom::Arrow::fletch_color(team_color);
    if (arrow.get_kind() == Game::Systems::ProjectileKind::CursedArrow) {
      shaft_color = QVector3D(0.42F, 0.18F, 0.58F);
      tip_color = QVector3D(0.72F, 0.32F, 0.92F);
      fletch_color = QVector3D(0.58F, 0.22F, 0.82F);
    }
    renderer->mesh(arrow_shaft_mesh, model, shaft_color, nullptr, 1.0F);
    renderer->mesh(arrow_tip_mesh, model, tip_color, nullptr, 1.0F);

    QMatrix4x4 glow_model = model;
    glow_model.scale(2.45F, 2.45F, 1.08F);
    QVector3D const glow_color =
        arrow.get_kind() == Game::Systems::ProjectileKind::CursedArrow
            ? QVector3D(0.78F, 0.35F, 1.0F)
            : Geom::Arrow::fletch_color(team_color);
    renderer->mesh(arrow_shaft_mesh, glow_model, glow_color, nullptr, 0.30F);

    QMatrix4x4 fletch_model = model;
    fletch_model.translate(
        0.0F, 0.0F, -arrow_z_scale * Geom::Arrow::k_fletch_z_offset_factor);
    fletch_model.scale(Geom::Arrow::k_fletch_xy_scale,
                       Geom::Arrow::k_fletch_xy_scale,
                       Geom::Arrow::k_fletch_z_scale);
    renderer->mesh(arrow_fletching_mesh, fletch_model, fletch_color, nullptr, 0.85F);
  }
}

void render_stone_projectile(Renderer* renderer,
                             ResourceManager* resources,
                             const Game::Systems::StoneProjectile& stone,
                             const QVector3D&,
                             const QMatrix4x4& base_model) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }

  auto* stone_mesh = Geom::Stone::get();
  if (stone_mesh == nullptr) {
    return;
  }

  QMatrix4x4 model = base_model;

  float const tumble_speed = 720.0F;
  float const tumble_angle = stone.get_progress() * tumble_speed;
  model.rotate(tumble_angle, QVector3D(1, 0.5F, 0.3F).normalized());

  float const stone_scale = stone.get_scale();
  model.scale(stone_scale, stone_scale, stone_scale);

  QVector3D const stone_color(0.45F, 0.42F, 0.38F);
  renderer->mesh(stone_mesh, model, stone_color, nullptr, 1.0F);
}

void render_projectiles(Renderer* renderer,
                        ResourceManager* resources,
                        const Game::Systems::ProjectileSystem& projectile_system) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }

  const auto& projectiles = projectile_system.projectiles();

  constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;

  for (const auto& projectile : projectiles) {
    if (!projectile->is_active()) {
      continue;
    }

    const QVector3D delta = projectile->get_end() - projectile->get_start();
    const float dist = std::max(0.001F, delta.length());
    QVector3D pos = projectile->get_start() + delta * projectile->get_progress();

    float const h = projectile->get_arc_height() * 4.0F * projectile->get_progress() *
                    (1.0F - projectile->get_progress());
    pos.setY(pos.y() + h);

    QMatrix4x4 model;
    model.translate(pos.x(), pos.y(), pos.z());

    QVector3D const dir = delta.normalized();
    float const yaw_deg = std::atan2(dir.x(), dir.z()) * k_rad_to_deg;
    model.rotate(yaw_deg, QVector3D(0, 1, 0));

    if (const auto* arrow =
            dynamic_cast<const Game::Systems::ArrowProjectile*>(projectile.get())) {
      render_arrow_projectile(renderer, resources, *arrow, pos, model);
    } else if (const auto* stone = dynamic_cast<const Game::Systems::StoneProjectile*>(
                   projectile.get())) {
      render_stone_projectile(renderer, resources, *stone, pos, model);
    }
  }
}

} // namespace Render::GL
