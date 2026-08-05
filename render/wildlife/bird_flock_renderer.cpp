#include "bird_flock_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "../../game/wildlife/bird_flock.h"
#include "../gl/primitives.h"
#include "../submission_visibility.h"
#include "../submitter.h"
#include "scene/camera.h"

namespace Render::GL::Wildlife {

namespace {

constexpr float k_two_pi = 6.28318530718F;
constexpr float k_degrees_per_radian = 57.2957795F;
constexpr float k_cull_radius = 0.6F;

constexpr float k_far_distance_sq = 100.0F * 100.0F;
constexpr float k_detail_distance_sq = 30.0F * 30.0F;

constexpr float k_bird_scale = 1.65F;

constexpr float k_body_radius_x = 0.024F;
constexpr float k_body_radius_y = 0.027F;
constexpr float k_body_radius_z = 0.082F;

constexpr float k_head_radius = 0.020F;
constexpr float k_head_forward = 0.060F;
constexpr float k_head_rise = 0.011F;

constexpr float k_wing_span = 0.150F;
constexpr float k_wing_chord = 0.042F;
constexpr float k_wing_thickness = 0.005F;
constexpr float k_wing_root_lateral = 0.019F;
constexpr float k_wing_root_rise = 0.011F;
constexpr float k_wing_root_forward = 0.012F;

constexpr float k_tail_length = 0.100F;
constexpr float k_tail_half_width = 0.026F;
constexpr float k_tail_thickness = 0.005F;
constexpr float k_tail_forward = -0.056F;

constexpr float k_flap_amplitude = 44.0F;
constexpr float k_flap_skew = 0.10F;
constexpr float k_cruise_dihedral = 8.0F;
constexpr float k_glide_dihedral = 14.0F;
constexpr float k_glide_residual_beat = 0.08F;
constexpr float k_cruise_sweep = 21.0F;

constexpr float k_perched_span_scale = 0.60F;
constexpr float k_perched_chord = 0.050F;
constexpr float k_perched_sweep = 74.0F;
constexpr float k_perched_dihedral = 1.0F;
constexpr float k_perched_flap = 4.0F;
constexpr float k_perched_pitch = 13.0F;

constexpr float k_wing_shade = 0.86F;
constexpr float k_tail_shade = 0.78F;
constexpr float k_head_shade = 0.90F;

auto smooth_step(float edge0, float edge1, float value) -> float {
  float const t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

auto plumage_for(std::uint16_t flock, std::uint8_t tint) -> QVector3D {
  QVector3D base;
  switch (flock % 3U) {
  case 0U:
    base = QVector3D(0.200F, 0.210F, 0.235F);
    break;
  case 1U:
    base = QVector3D(0.300F, 0.230F, 0.160F);
    break;
  default:
    base = QVector3D(0.430F, 0.415F, 0.395F);
    break;
  }
  float const jitter = 0.92F + (0.08F * static_cast<float>(tint % 3U));
  return base * jitter;
}

void draw_wing(ISubmitter& out,
               const QMatrix4x4& model,
               Mesh* cone,
               float side,
               float flap_degrees,
               float sweep_degrees,
               float span,
               float chord,
               const QVector3D& color) {
  QMatrix4x4 wing = model;
  wing.translate(side * k_wing_root_lateral, k_wing_root_rise, k_wing_root_forward);
  wing.rotate(side * flap_degrees, 0.0F, 0.0F, 1.0F);
  wing.rotate(side * sweep_degrees, 0.0F, 1.0F, 0.0F);
  wing.rotate(side * -90.0F, 0.0F, 0.0F, 1.0F);
  wing.translate(0.0F, span * 0.5F, 0.0F);
  wing.scale(k_wing_thickness, span, chord);
  out.mesh(cone, wing, color);
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

    float const hold = perched ? 0.0F
                               : smooth_step(0.52F, 0.66F, bird.glide) *
                                     (1.0F - smooth_step(0.86F, 0.97F, bird.glide));
    float const beat_gain = 1.0F - ((1.0F - k_glide_residual_beat) * hold);
    float const skewed_phase =
        bird.phase + (k_flap_skew * std::sin(bird.phase * k_two_pi));
    float const beat = std::sin(skewed_phase * k_two_pi);

    float flap_degrees = 0.0F;
    float sweep_degrees = 0.0F;
    float span = k_wing_span;
    float chord = k_wing_chord;
    if (perched) {
      flap_degrees = k_perched_dihedral + (beat * k_perched_flap);
      sweep_degrees = k_perched_sweep;
      span = k_wing_span * k_perched_span_scale;
      chord = k_perched_chord;
    } else {
      float const dihedral =
          k_cruise_dihedral + ((k_glide_dihedral - k_cruise_dihedral) * hold);
      flap_degrees = dihedral + (beat * k_flap_amplitude * beat_gain);
      sweep_degrees = k_cruise_sweep;
    }

    QMatrix4x4 model;
    model.translate(bird.x, bird.y, bird.z);
    model.rotate(bird.yaw * k_degrees_per_radian, 0.0F, 1.0F, 0.0F);
    if (perched) {
      model.rotate(-k_perched_pitch, 1.0F, 0.0F, 0.0F);
    } else if (bird.bank != 0.0F) {
      model.rotate(-bird.bank * k_degrees_per_radian, 0.0F, 0.0F, 1.0F);
    }
    model.scale(k_bird_scale);

    QVector3D const plumage = plumage_for(bird.flock, bird.tint);
    QVector3D const wing_color = plumage * k_wing_shade;

    QMatrix4x4 body = model;
    body.scale(k_body_radius_x, k_body_radius_y, k_body_radius_z);
    out.mesh(sphere, body, plumage);

    draw_wing(
        out, model, cone, 1.0F, flap_degrees, sweep_degrees, span, chord, wing_color);
    draw_wing(
        out, model, cone, -1.0F, flap_degrees, sweep_degrees, span, chord, wing_color);

    stats.submitted += 1U;

    if (distance_sq > k_detail_distance_sq) {
      continue;
    }

    QMatrix4x4 head = model;
    head.translate(0.0F, k_head_rise, k_head_forward);
    head.scale(k_head_radius);
    out.mesh(sphere, head, plumage * k_head_shade);

    QMatrix4x4 tail = model;
    tail.translate(0.0F, 0.003F, k_tail_forward);
    tail.rotate(90.0F, 1.0F, 0.0F, 0.0F);
    tail.translate(0.0F, -k_tail_length * 0.5F, 0.0F);
    tail.scale(k_tail_half_width, k_tail_length, k_tail_thickness);
    out.mesh(cone, tail, plumage * k_tail_shade);
  }

  return stats;
}

} // namespace Render::GL::Wildlife
