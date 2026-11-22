#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "render/entity/barracks_flag_renderer.h"
#include "render/frame/draw_context.h"
#include "render/frame/draw_resources.h"
#include "render/frame/entity_appearance.h"
#include "render/frame/frame_submitter.h"
#include "render/frame/submission_visibility.h"
#include "render/geom/arrow.h"
#include "render/geom/primitives.h"
#include "render/geom/stone.h"
#include "render/geom/transforms.h"
#include "render/overlay/projectile_renderer.h"
#include "render/overlay/projectile_renderer_internal.h"
#include "render/overlay/raft_renderer.h"

namespace Render::GL {

using namespace projectile_renderer_detail;

void render_stone_projectile(FrameSubmitter* renderer,
                             const DrawResources* resources,
                             const Game::Systems::ProjectileView& projectile,
                             const QVector3D& position,
                             const QMatrix4x4& base_model,
                             bool reduced_effects) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }

  auto* stone_mesh = Geom::Stone::get();
  if (stone_mesh == nullptr) {
    return;
  }

  QMatrix4x4 model = base_model;

  float const tumble_speed = 720.0F;
  float const tumble_angle = projectile.progress * tumble_speed;
  model.rotate(tumble_angle, QVector3D(1, 0.5F, 0.3F).normalized());

  float const stone_scale = projectile.scale;
  float const mesh_scale = stone_scale * Geom::Stone::k_projectile_radius;
  model.scale(mesh_scale, mesh_scale, mesh_scale);

  bool const flaming = projectile.kind == Game::Systems::ProjectileKind::FlamingStone;
  QVector3D const stone_color =
      flaming ? QVector3D(0.32F, 0.24F, 0.20F) : QVector3D(0.45F, 0.42F, 0.38F);
  renderer->mesh(stone_mesh, model, stone_color, nullptr, 1.0F);

  if (!flaming) {
    if (float const night = projectile_night_amount(renderer); night > 0.02F) {
      QVector3D const tangent = flight_tangent(projectile);
      QVector3D const trail_delta = projectile.end - projectile.start;
      draw_night_glint(renderer,
                       position,
                       tangent,
                       k_night_glint_steel,
                       0.30F * stone_scale,
                       1.7F * night,
                       projectile.progress * 0.3F);
      for (int streak = 1; streak <= (reduced_effects ? 1 : 4); ++streak) {
        float const trail_t = projectile.progress - static_cast<float>(streak) * 0.04F;
        if (trail_t < 0.0F) {
          continue;
        }
        QVector3D trail_pos = projectile.start + trail_delta * trail_t;
        trail_pos.setY(trail_pos.y() +
                       projectile.arc_height * 4.0F * trail_t * (1.0F - trail_t));
        float const fade = 1.0F - static_cast<float>(streak) / 5.0F;
        draw_night_glint(renderer,
                         trail_pos,
                         tangent,
                         k_night_glint_steel * fade,
                         0.22F * stone_scale * fade,
                         1.1F * night * fade,
                         0.06F * static_cast<float>(streak));
      }
      add_night_projectile_light(
          renderer, position, QVector3D(0.55F, 0.78F, 1.0F), 4.0F, 0.5F * night);
    }
    return;
  }

  float const animation_time = renderer->get_animation_time();
  float const pulse =
      0.92F + 0.10F * std::sin(animation_time * 11.0F + projectile.progress * 27.0F);
  float const core_radius = 0.20F * stone_scale * pulse;
  renderer->fireball(position,
                     QVector3D(0.95F, 0.26F, 0.04F),
                     core_radius,
                     0.92F,
                     animation_time + projectile.progress * 3.1F);
  renderer->fireball(position,
                     QVector3D(1.0F, 0.66F, 0.18F),
                     core_radius * 0.55F,
                     1.28F,
                     animation_time * 1.29F + 1.4F);

  const QVector3D delta = projectile.end - projectile.start;
  constexpr int k_trail_segments = 4;
  for (int trail_idx = 1; trail_idx <= k_trail_segments; ++trail_idx) {
    float const trail_t = projectile.progress - static_cast<float>(trail_idx) * 0.045F;
    if (trail_t < 0.0F) {
      continue;
    }

    QVector3D trail_pos = projectile.start + delta * trail_t;
    trail_pos.setY(trail_pos.y() +
                   projectile.arc_height * 4.0F * trail_t * (1.0F - trail_t));

    float const falloff =
        1.0F - static_cast<float>(trail_idx) / static_cast<float>(k_trail_segments);
    renderer->fireball(trail_pos,
                       QVector3D(0.86F, 0.20F, 0.03F) * (0.35F + 0.65F * falloff),
                       core_radius * (0.30F + 0.42F * falloff),
                       0.72F * falloff,
                       animation_time * 1.13F + static_cast<float>(trail_idx));
  }
}

} // namespace Render::GL
