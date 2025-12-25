#include "projectile_renderer.h"
#include "../game/systems/arrow_projectile.h"
#include "../game/systems/projectile_system.h"
#include "../game/systems/stone_projectile.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "arrow.h"
#include "stone.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL {

void render_arrow_projectile(Renderer *renderer, ResourceManager *resources,
                             const Game::Systems::ArrowProjectile &arrow,
                             const QVector3D &pos,
                             const QMatrix4x4 &base_model) {
  if (!renderer || !resources) {
    return;
  }

  auto *arrow_shaft_mesh = Geom::Arrow::get_shaft();
  auto *arrow_tip_mesh = Geom::Arrow::get_tip();
  if (!arrow_shaft_mesh || !arrow_tip_mesh) {
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

    QVector3D base_color = arrow.get_color();

    QVector3D wood_color =
        QVector3D(std::clamp(base_color.x() * 0.6F + 0.35F, 0.0F, 1.0F),
                  std::clamp(base_color.y() * 0.55F + 0.30F, 0.0F, 1.0F),
                  std::clamp(base_color.z() * 0.5F + 0.15F, 0.0F, 1.0F));

    renderer->mesh(arrow_shaft_mesh, bolt_model, wood_color, nullptr, 1.0F);

    QMatrix4x4 tip_model = bolt_model;

    tip_model.translate(0.0F, 0.0F, bolt_z_scale * 0.3F);
    tip_model.scale(0.85F, 0.85F, 0.2F);

    QVector3D iron_color = QVector3D(0.25F, 0.24F, 0.22F);
    renderer->mesh(arrow_tip_mesh, tip_model, iron_color, nullptr, 1.0F);

    QMatrix4x4 fletch_model = bolt_model;
    fletch_model.translate(0.0F, 0.0F, -bolt_z_scale * 0.2F);
    fletch_model.scale(0.75F, 0.75F, 0.15F);

    QVector3D fletch_color =
        QVector3D(std::clamp(wood_color.x() * 1.15F, 0.0F, 1.0F),
                  std::clamp(wood_color.y() * 1.10F, 0.0F, 1.0F),
                  std::clamp(wood_color.z() * 0.95F, 0.0F, 1.0F));
    renderer->mesh(arrow_shaft_mesh, fletch_model, fletch_color, nullptr, 0.7F);

    if (arrow.get_progress() > 0.15F) {
      float trail_opacity =
          std::clamp((arrow.get_progress() - 0.15F) / 0.85F, 0.0F, 0.3F);

      for (int trail_idx = 1; trail_idx <= 2; trail_idx++) {
        float trail_t = arrow.get_progress() - (trail_idx * 0.08F);
        if (trail_t >= 0.0F) {
          QVector3D trail_pos = arrow.get_start() + delta * trail_t;
          float trail_h =
              arrow.get_arc_height() * 4.0F * trail_t * (1.0F - trail_t);
          trail_pos.setY(trail_pos.y() + trail_h);

          QMatrix4x4 trail_model;
          trail_model.translate(trail_pos.x(), trail_pos.y(), trail_pos.z());

          constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;
          QVector3D const dir = delta.normalized();
          float const yaw_deg = std::atan2(dir.x(), dir.z()) * k_rad_to_deg;
          trail_model.rotate(yaw_deg, QVector3D(0, 1, 0));
          trail_model.rotate(pitch_deg, QVector3D(1, 0, 0));

          float trail_spin_angle = trail_t * spin_speed;
          trail_model.rotate(trail_spin_angle, QVector3D(0, 0, 1));

          float trail_scale_factor = 0.6F - (trail_idx * 0.15F);
          trail_model.translate(0.0F, 0.0F,
                                -bolt_z_scale * bolt_z_translate_factor);
          trail_model.scale(bolt_xy_scale * trail_scale_factor,
                            bolt_xy_scale * trail_scale_factor,
                            bolt_z_scale * trail_scale_factor);

          QVector3D trail_color = wood_color * (1.0F - trail_opacity * 0.4F);
          renderer->mesh(arrow_shaft_mesh, trail_model, trail_color, nullptr,
                         1.0F - (trail_opacity * 0.7F));
        }
      }
    }
  } else {

    constexpr float arrow_z_scale = 0.40F;
    constexpr float arrow_xy_scale = 0.26F;
    constexpr float arrow_z_translate_factor = 0.5F;
    model.translate(0.0F, 0.0F, -arrow_z_scale * arrow_z_translate_factor);
    model.scale(arrow_xy_scale, arrow_xy_scale, arrow_z_scale);
    QVector3D wood_color =
        QVector3D(std::clamp(arrow.get_color().x() * 0.6F + 0.35F, 0.0F, 1.0F),
                  std::clamp(arrow.get_color().y() * 0.55F + 0.30F, 0.0F, 1.0F),
                  std::clamp(arrow.get_color().z() * 0.5F + 0.15F, 0.0F, 1.0F));
    renderer->mesh(arrow_shaft_mesh, model, wood_color, nullptr, 1.0F);

    QVector3D tip_color(0.70F, 0.72F, 0.75F);
    renderer->mesh(arrow_tip_mesh, model, tip_color, nullptr, 1.0F);
  }
}

void render_stone_projectile(Renderer *renderer, ResourceManager *resources,
                             const Game::Systems::StoneProjectile &stone,
                             const QVector3D &pos,
                             const QMatrix4x4 &base_model) {
  if (!renderer || !resources) {
    return;
  }

  auto *stone_mesh = Geom::Stone::get();
  if (!stone_mesh) {
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

void render_projectiles(
    Renderer *renderer, ResourceManager *resources,
    const Game::Systems::ProjectileSystem &projectile_system) {
  if (!renderer || !resources) {
    return;
  }

  const auto &projectiles = projectile_system.projectiles();

  constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;

  for (const auto &projectile : projectiles) {
    if (!projectile->is_active()) {
      continue;
    }

    const QVector3D delta = projectile->get_end() - projectile->get_start();
    const float dist = std::max(0.001F, delta.length());
    QVector3D pos =
        projectile->get_start() + delta * projectile->get_progress();

    float const h = projectile->get_arc_height() * 4.0F *
                    projectile->get_progress() *
                    (1.0F - projectile->get_progress());
    pos.setY(pos.y() + h);

    QMatrix4x4 model;
    model.translate(pos.x(), pos.y(), pos.z());

    QVector3D const dir = delta.normalized();
    float const yaw_deg = std::atan2(dir.x(), dir.z()) * k_rad_to_deg;
    model.rotate(yaw_deg, QVector3D(0, 1, 0));

    if (auto *arrow = dynamic_cast<const Game::Systems::ArrowProjectile *>(
            projectile.get())) {
      render_arrow_projectile(renderer, resources, *arrow, pos, model);
    } else if (auto *stone =
                   dynamic_cast<const Game::Systems::StoneProjectile *>(
                       projectile.get())) {
      render_stone_projectile(renderer, resources, *stone, pos, model);
    }
  }
}

} // namespace Render::GL
