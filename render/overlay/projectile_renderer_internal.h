#pragma once

// Internal to the projectile renderer: the night-glint helpers that both
// projectile_renderer.cpp and projectile_stone.cpp draw with.

#include <QVector3D>

#include "game/systems/render_effects_frame.h"
#include "render/frame/frame_submitter.h"
#include "render/frame/local_lighting.h"
#include "render/frame/submission_visibility.h"

namespace Render::GL::projectile_renderer_detail {

inline const QVector3D k_night_glint_steel(0.55F, 0.80F, 1.0F);
inline const QVector3D k_night_glint_cursed(0.42F, 0.12F, 0.80F);

[[nodiscard]] inline auto
projectile_night_amount(const FrameSubmitter* renderer) -> float {
  return renderer->environment_lighting().darkness_amount();
}

[[nodiscard]] inline auto
flight_tangent(const Game::Systems::ProjectileView& projectile) -> QVector3D {
  QVector3D tangent = projectile.end - projectile.start;
  tangent.setY(tangent.y() +
               projectile.arc_height * 4.0F * (1.0F - 2.0F * projectile.progress));
  if (tangent.lengthSquared() <= 1.0e-6F) {
    return {0.0F, 0.0F, 1.0F};
  }
  return tangent.normalized();
}

inline void draw_night_glint(FrameSubmitter* renderer,
                             const QVector3D& position,
                             const QVector3D& direction,
                             const QVector3D& tint,
                             float radius,
                             float intensity,
                             float phase) {
  renderer->metal_spark(position, tint, radius, intensity, phase, direction);
}

inline void add_night_projectile_light(FrameSubmitter* renderer,
                                       const QVector3D& position,
                                       const QVector3D& tint,
                                       float radius,
                                       float intensity) {

  if (!renderer->submission_visibility().accepts_sphere(
          position, 0.25F, SubmissionFogMode::VisibleOnly, FogExtent::Anchor)) {
    return;
  }
  Render::LocalLight light;
  light.position = position;
  light.color = tint;
  light.radius = radius;
  light.intensity = intensity;
  renderer->local_light(light);
}

} // namespace Render::GL::projectile_renderer_detail
