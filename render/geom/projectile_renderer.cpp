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
namespace {

constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;

[[nodiscard]] auto scaled_color(const QVector3D& color, float scale) -> QVector3D {
  return {std::clamp(color.x() * scale, 0.0F, 1.0F),
          std::clamp(color.y() * scale, 0.0F, 1.0F),
          std::clamp(color.z() * scale, 0.0F, 1.0F)};
}

[[nodiscard]] auto projectile_model_at(const QVector3D& position,
                                       const QVector3D& direction) -> QMatrix4x4 {
  QVector3D dir = direction;
  if (dir.lengthSquared() <= 1.0e-6F) {
    dir = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    dir.normalize();
  }
  QMatrix4x4 model;
  model.translate(position);
  model.rotate(std::atan2(dir.x(), dir.z()) * k_rad_to_deg, QVector3D(0, 1, 0));
  float const horizontal = std::hypot(dir.x(), dir.z());
  model.rotate(-std::atan2(dir.y(), horizontal) * k_rad_to_deg, QVector3D(1, 0, 0));
  return model;
}

void render_projectile_impact(Renderer* renderer,
                              const Game::Systems::ProjectileImpactEvent& impact) {
  float const progress =
      std::clamp(impact.age / std::max(impact.lifetime, 1.0e-4F), 0.0F, 1.0F);
  float const fade = 1.0F - progress;
  float const flash = std::clamp(1.0F - progress * 4.5F, 0.0F, 1.0F);

  if (impact.kind == Game::Systems::ProjectileKind::Stone) {
    renderer->stone_impact(impact.position + QVector3D(0.0F, 0.08F, 0.0F),
                           QVector3D(0.72F, 0.60F, 0.42F),
                           0.82F * impact.scale,
                           1.65F,
                           impact.age);
    return;
  }

  if (impact.kind == Game::Systems::ProjectileKind::Fireball) {
    float const expansion =
        0.18F + 0.92F * std::sin(progress * std::numbers::pi_v<float>);
    renderer->fireball(impact.position,
                       QVector3D(1.0F, 0.24F, 0.025F),
                       expansion,
                       1.65F * fade,
                       renderer->get_animation_time() + impact.age * 2.0F);
    renderer->fireball(impact.position + QVector3D(0.0F, 0.10F, 0.0F),
                       QVector3D(1.0F, 0.74F, 0.20F),
                       expansion * 0.48F,
                       1.90F * fade,
                       renderer->get_animation_time() * 1.37F);
    renderer->stone_impact(
        impact.position, QVector3D(0.95F, 0.28F, 0.035F), 0.95F, 1.35F, impact.age);
    return;
  }

  auto* shaft = Geom::Arrow::get_shaft();
  auto* tip = Geom::Arrow::get_tip();
  auto* fletching = Geom::Arrow::get_fletching();
  if (shaft == nullptr || tip == nullptr || fletching == nullptr) {
    return;
  }

  bool const cursed = impact.kind == Game::Systems::ProjectileKind::CursedArrow;
  QVector3D const flash_color =
      cursed ? QVector3D(0.76F, 0.30F, 1.0F)
             : (impact.ballista_bolt ? QVector3D(1.0F, 0.76F, 0.28F)
                                     : QVector3D(1.0F, 0.88F, 0.52F));
  float const impact_radius = impact.ballista_bolt ? 0.32F : 0.14F;
  float const impact_intensity = impact.ballista_bolt ? 1.35F : 0.72F;
  renderer->metal_spark(
      impact.position, flash_color, impact_radius, impact_intensity, impact.age);

  if (!impact.ballista_bolt) {
    return;
  }

  if (auto* sphere = get_unit_sphere(); sphere != nullptr && flash > 0.0F) {
    QMatrix4x4 flash_model;
    flash_model.translate(impact.position);
    float const radius =
        (impact.ballista_bolt ? 0.20F : 0.075F) * (0.55F + flash * 1.30F);
    flash_model.scale(radius, radius, radius);
    renderer->mesh(sphere, flash_model, flash_color, nullptr, flash);
  }

  QMatrix4x4 model =
      projectile_model_at(impact.position - impact.incoming_direction *
                                                (impact.ballista_bolt ? 0.24F : 0.10F),
                          impact.incoming_direction);
  if (impact.ballista_bolt) {
    model.translate(0.0F, 0.0F, -0.62F);
    model.scale(0.72F, 0.72F, 1.25F);
  } else {
    model.translate(0.0F, 0.0F, -Geom::Arrow::k_arrow_z_scale * 0.48F);
    model.scale(Geom::Arrow::k_arrow_xy_scale * impact.scale,
                Geom::Arrow::k_arrow_xy_scale * impact.scale,
                Geom::Arrow::k_arrow_z_scale * impact.scale);
  }
  QVector3D const shaft_color =
      cursed ? QVector3D(0.42F, 0.18F, 0.58F) : Geom::Arrow::shaft_color(impact.color);
  QVector3D const tip_color =
      cursed ? QVector3D(0.75F, 0.36F, 0.96F) : Geom::Arrow::tip_color();
  renderer->mesh(shaft, model, shaft_color, nullptr, fade);
  renderer->mesh(tip, model, tip_color, nullptr, fade);
  renderer->mesh(fletching, model, impact.color, nullptr, fade * 0.82F);
}

} // namespace

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
        pos, QVector3D(0.92F, 0.20F, 0.025F), 0.135F * pulse, 0.82F, spell_phase);
    renderer->fireball(pos,
                       QVector3D(1.0F, 0.68F, 0.18F),
                       0.076F * pulse,
                       1.32F,
                       spell_phase * 1.37F + 1.9F);

    constexpr int k_trail_segments = 5;
    for (int trail_idx = 1; trail_idx <= k_trail_segments; ++trail_idx) {
      float const trail_t =
          arrow.get_progress() - static_cast<float>(trail_idx) * 0.040F;
      if (trail_t < 0.0F) {
        continue;
      }

      QVector3D trail_pos = arrow.get_start() + delta * trail_t;
      float const trail_h = arrow.get_arc_height() * 4.0F * trail_t * (1.0F - trail_t);
      trail_pos.setY(trail_pos.y() + trail_h);

      float const trail_falloff =
          1.0F - static_cast<float>(trail_idx) / static_cast<float>(k_trail_segments);
      QVector3D const trail_color =
          QVector3D(0.88F, 0.18F, 0.025F) * (0.36F + 0.64F * trail_falloff) +
          QVector3D(0.045F, 0.025F, 0.018F) * (1.0F - trail_falloff);
      renderer->fireball(trail_pos,
                         trail_color,
                         std::max(0.018F, 0.073F * trail_falloff),
                         std::max(0.08F, 0.52F * trail_falloff * trail_falloff),
                         spell_phase - static_cast<float>(trail_idx) * 0.13F);
    }

    QMatrix4x4 core_model = model;
    core_model.scale(0.040F * pulse, 0.040F * pulse, 0.040F * pulse);
    renderer->mesh(
        fireball_mesh, core_model, QVector3D(1.0F, 0.94F, 0.64F), nullptr, 1.0F);

    QVector3D const orbit_offset(std::sin(spell_phase * 7.3F) * 0.031F,
                                 std::cos(spell_phase * 5.2F) * 0.017F,
                                 std::cos(spell_phase * 6.4F) * 0.031F);
    QMatrix4x4 spark_model = model;
    spark_model.translate(orbit_offset);
    spark_model.scale(0.009F, 0.009F, 0.009F);
    renderer->mesh(
        fireball_mesh, spark_model, QVector3D(1.0F, 0.86F, 0.40F), nullptr, 1.0F);
    return;
  }

  if (arrow.is_ballista_bolt()) {

    float const spin_speed = 1440.0F;
    float const spin_angle = arrow.get_progress() * spin_speed;
    model.rotate(spin_angle, QVector3D(0, 0, 1));

    constexpr float bolt_z_scale = 1.25F;
    constexpr float bolt_xy_scale = 0.72F;
    constexpr float bolt_z_translate_factor = 0.5F;

    QMatrix4x4 bolt_model = model;
    bolt_model.translate(0.0F, 0.0F, -bolt_z_scale * bolt_z_translate_factor);
    bolt_model.scale(bolt_xy_scale, bolt_xy_scale, bolt_z_scale);

    QVector3D const base_color = arrow.get_color();

    QVector3D const wood_color =
        QVector3D(std::clamp(base_color.x() * 0.45F + 0.54F, 0.0F, 1.0F),
                  std::clamp(base_color.y() * 0.40F + 0.43F, 0.0F, 1.0F),
                  std::clamp(base_color.z() * 0.30F + 0.22F, 0.0F, 1.0F));

    renderer->mesh(arrow_shaft_mesh, bolt_model, wood_color, nullptr, 1.0F);

    QMatrix4x4 glow_model = bolt_model;
    glow_model.scale(1.55F, 1.55F, 1.04F);
    renderer->mesh(
        arrow_shaft_mesh, glow_model, QVector3D(1.0F, 0.72F, 0.30F), nullptr, 0.28F);

    QMatrix4x4 tip_model = bolt_model;

    tip_model.translate(0.0F, 0.0F, bolt_z_scale * 0.3F);
    tip_model.scale(0.85F, 0.85F, 0.2F);

    QVector3D const iron_color = QVector3D(0.70F, 0.68F, 0.62F);
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

      for (int trail_idx = 1; trail_idx <= 3; trail_idx++) {
        float const trail_t = arrow.get_progress() - (trail_idx * 0.065F);
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

          float const trail_scale_factor = 0.72F - (trail_idx * 0.13F);
          trail_model.translate(0.0F, 0.0F, -bolt_z_scale * bolt_z_translate_factor);
          trail_model.scale(bolt_xy_scale * trail_scale_factor,
                            bolt_xy_scale * trail_scale_factor,
                            bolt_z_scale * trail_scale_factor);

          QVector3D const trail_color =
              scaled_color(wood_color, 1.0F - trail_opacity * 0.25F);
          renderer->mesh(arrow_shaft_mesh,
                         trail_model,
                         trail_color,
                         nullptr,
                         1.0F - (trail_opacity * 0.7F));
        }
      }
    }
  } else {
    if (arrow.trail_alpha() > 0.001F && arrow.trail_length() > 0.0F) {
      for (int segment = 1; segment <= 2; ++segment) {
        float const trail_t =
            arrow.get_progress() -
            arrow.trail_length() * (0.55F + 0.38F * static_cast<float>(segment));
        if (trail_t <= 0.0F) {
          continue;
        }
        QVector3D trail_pos = arrow.get_start() + delta * trail_t;
        trail_pos.setY(trail_pos.y() +
                       arrow.get_arc_height() * 4.0F * trail_t * (1.0F - trail_t));
        QVector3D tangent = delta;
        tangent.setY(tangent.y() +
                     arrow.get_arc_height() * 4.0F * (1.0F - 2.0F * trail_t));
        QMatrix4x4 trail_model = projectile_model_at(trail_pos, tangent);
        trail_model.rotate(arrow.roll_deg() + trail_t * arrow.spin_rate_deg(),
                           QVector3D(0, 0, 1));
        trail_model.translate(0.0F, 0.0F, -Geom::Arrow::k_arrow_z_scale * 0.5F);
        float const trail_scale =
            arrow.get_scale() * (0.82F - 0.14F * static_cast<float>(segment));
        trail_model.scale(Geom::Arrow::k_arrow_xy_scale * trail_scale,
                          Geom::Arrow::k_arrow_xy_scale * trail_scale,
                          Geom::Arrow::k_arrow_z_scale * trail_scale);
        float const alpha =
            arrow.trail_alpha() * (0.95F - 0.24F * static_cast<float>(segment));
        renderer->mesh(arrow_shaft_mesh,
                       trail_model,
                       scaled_color(Geom::Arrow::shaft_color(arrow.get_color()), 0.82F),
                       nullptr,
                       alpha);
        renderer->mesh(arrow_tip_mesh,
                       trail_model,
                       scaled_color(Geom::Arrow::tip_color(), 0.88F),
                       nullptr,
                       alpha * 0.78F);
      }
    }
    model.rotate(arrow.roll_deg() + arrow.get_progress() * arrow.spin_rate_deg(),
                 QVector3D(0, 0, 1));
    constexpr float arrow_z_scale = Geom::Arrow::k_arrow_z_scale;
    constexpr float arrow_xy_scale = Geom::Arrow::k_arrow_xy_scale;
    constexpr float arrow_z_translate_factor = Geom::Arrow::k_arrow_z_translate_factor;
    model.translate(0.0F, 0.0F, -arrow_z_scale * arrow_z_translate_factor);
    model.scale(arrow_xy_scale * arrow.get_scale(),
                arrow_xy_scale * arrow.get_scale(),
                arrow_z_scale * arrow.get_scale());

    QVector3D const team_color = arrow.get_color();
    QVector3D shaft_color =
        scaled_color(Geom::Arrow::shaft_color(team_color), arrow.brightness());
    QVector3D tip_color = scaled_color(Geom::Arrow::tip_color(), arrow.brightness());
    QVector3D fletch_color =
        scaled_color(Geom::Arrow::fletch_color(team_color), arrow.brightness());
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

  for (const auto& projectile : projectiles) {
    if (!projectile->is_active() || projectile->get_progress() < 0.0F) {
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

  for (auto const& impact : projectile_system.impacts()) {
    render_projectile_impact(renderer, impact);
  }
}

} // namespace Render::GL
