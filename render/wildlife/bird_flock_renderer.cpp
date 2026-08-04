#include "bird_flock_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "../../game/wildlife/bird_flock.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../submission_visibility.h"
#include "../submitter.h"
#include "scene/camera.h"

namespace Render::GL::Wildlife {

namespace {

constexpr float k_two_pi = 6.28318530718F;
constexpr float k_degrees_per_radian = 57.2957795F;
constexpr float k_cull_radius = 0.6F;
constexpr float k_far_distance_sq = 140.0F * 140.0F;
constexpr float k_detail_distance_sq = 42.0F * 42.0F;

constexpr float k_body_length = 0.085F;
constexpr float k_wing_span = 0.115F;
constexpr float k_flap_amplitude = 46.0F;
constexpr float k_perched_flap = 6.0F;

auto plumage_for(std::uint8_t tint) -> QVector3D {
  switch (tint % 3U) {
  case 0U:
    return {0.24F, 0.23F, 0.26F};
  case 1U:
    return {0.33F, 0.29F, 0.25F};
  default:
    return {0.19F, 0.20F, 0.24F};
  }
}

auto bird_model(const Game::Wildlife::Bird& bird) -> QMatrix4x4 {
  QMatrix4x4 model;
  model.translate(bird.x, bird.y, bird.z);
  model.rotate(bird.yaw * k_degrees_per_radian, 0.0F, 1.0F, 0.0F);
  return model;
}

void draw_wing(ISubmitter& out,
               const QMatrix4x4& model,
               Mesh* sphere,
               float side,
               float flap_degrees,
               const QVector3D& color) {
  QMatrix4x4 wing = model;
  wing.translate(side * 0.032F, 0.014F, -0.006F);
  wing.rotate(side * flap_degrees, 0.0F, 0.0F, 1.0F);
  wing.rotate(side * -14.0F, 0.0F, 1.0F, 0.0F);
  wing.translate(side * (k_wing_span * 0.5F), 0.0F, 0.0F);
  wing.scale(k_wing_span * 0.5F, 0.009F, 0.050F);
  out.mesh(sphere, wing, color);
}

} // namespace

auto submit_bird_flocks(ISubmitter& out,
                        const SubmissionVisibilityPolicy* visibility,
                        const Camera* camera) -> BirdFlockSubmitStats {
  BirdFlockSubmitStats stats;

  const auto frame = Game::Wildlife::BirdFlockManager::instance().frame();
  if (frame == nullptr || frame->birds.empty()) {
    return stats;
  }

  Mesh* const sphere = get_unit_sphere();
  Mesh* const cone = get_unit_cone();
  if (sphere == nullptr || cone == nullptr) {
    return stats;
  }

  QVector3D const camera_position =
      camera != nullptr ? camera->get_position() : QVector3D();

  for (const auto& bird : frame->birds) {
    stats.considered += 1U;

    QVector3D const position(bird.x, bird.y, bird.z);
    float distance_sq = 0.0F;
    if (camera != nullptr) {
      float const dx = position.x() - camera_position.x();
      float const dy = position.y() - camera_position.y();
      float const dz = position.z() - camera_position.z();
      distance_sq = (dx * dx) + (dy * dy) + (dz * dz);
      if (distance_sq > k_far_distance_sq) {
        continue;
      }
    }

    if (visibility != nullptr &&
        !visibility->accepts_sphere(
            position, k_cull_radius, SubmissionFogMode::VisibleOnly)) {
      continue;
    }

    bool const perched = bird.behavior == Game::Wildlife::Behavior::Graze;
    float const flap_rate = perched ? 1.0F : 2.0F;
    float const flap =
        perched ? (std::sin(bird.phase * k_two_pi * flap_rate) * k_perched_flap)
                : (std::sin(bird.phase * k_two_pi * flap_rate) * k_flap_amplitude);

    QMatrix4x4 const model = bird_model(bird);
    QVector3D const plumage = plumage_for(bird.tint);
    QVector3D const accent = plumage * 1.35F;

    QMatrix4x4 body = model;
    body.translate(0.0F, 0.0F, 0.0F);
    body.scale(0.045F, 0.040F, k_body_length);
    out.mesh(sphere, body, plumage);

    draw_wing(out, model, sphere, 1.0F, flap, plumage);
    draw_wing(out, model, sphere, -1.0F, flap, plumage);

    stats.submitted += 1U;

    if (distance_sq > k_detail_distance_sq) {
      continue;
    }

    QMatrix4x4 head = model;
    head.translate(0.0F, 0.022F, 0.070F);
    head.scale(0.030F, 0.030F, 0.032F);
    out.mesh(sphere, head, plumage);

    out.mesh(cone,
             Render::Geom::cone_from_to(model,
                                        QVector3D(0.0F, 0.020F, 0.092F),
                                        QVector3D(0.0F, 0.016F, 0.128F),
                                        0.013F),
             accent);

    out.mesh(cone,
             Render::Geom::cone_from_to(model,
                                        QVector3D(0.0F, 0.004F, -0.070F),
                                        QVector3D(0.0F, 0.002F, -0.135F),
                                        0.028F),
             plumage);
  }

  return stats;
}

} // namespace Render::GL::Wildlife
