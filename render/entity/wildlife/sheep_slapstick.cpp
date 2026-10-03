#include "sheep_slapstick.h"

#include <algorithm>
#include <cmath>

#include "render/gl/primitives.h"
#include "render/submitter.h"

namespace Render::GL::Wildlife {

namespace {

constexpr float k_two_pi = 6.28318530718F;

auto smooth(float t) -> float {
  float const x = std::clamp(t, 0.0F, 1.0F);
  return x * x * (3.0F - (2.0F * x));
}

const QVector3D k_star_colour{1.0F, 0.84F, 0.20F};
const QVector3D k_star_core_colour{1.0F, 0.97F, 0.78F};

const QVector3D k_fallen_star_centre{0.0F, 0.34F, 0.42F};
const QVector3D k_wool_burst_origin{0.0F, 0.50F, 0.0F};

void submit_star(ISubmitter& out,
                 Mesh* cube,
                 const QMatrix4x4& model,
                 const QVector3D& centre,
                 float spin_degrees,
                 float size) {
  QMatrix4x4 star = model;
  star.translate(centre);
  star.rotate(spin_degrees, 0.0F, 1.0F, 0.0F);
  star.rotate(spin_degrees * 0.6F, 1.0F, 0.0F, 0.0F);
  float const length = 0.060F * size;
  float const thickness = 0.016F * size;
  for (int axis = 0; axis < 3; ++axis) {
    QMatrix4x4 bar = star;
    if (axis == 1) {
      bar.rotate(90.0F, 0.0F, 0.0F, 1.0F);
    } else if (axis == 2) {
      bar.rotate(90.0F, 0.0F, 1.0F, 0.0F);
    }
    bar.rotate(45.0F, 1.0F, 0.0F, 0.0F);
    bar.scale(length, thickness, thickness);
    out.mesh(cube, bar, k_star_colour);
  }
  QMatrix4x4 core = star;
  core.scale(thickness * 1.6F);
  out.mesh(cube, core, k_star_core_colour);
}

} // namespace

auto plan_sheep_slapstick(const DrawState& state) -> SheepSlapstick {
  SheepSlapstick gag;
  if (state.sink_progress > 0.0F) {
    return gag;
  }

  if (state.death_elapsed >= 0.0F) {
    gag.poof_time =
        state.death_elapsed < k_sheep_wool_poof_seconds ? state.death_elapsed : -1.0F;
    gag.stars = 1.0F - smooth((state.death_elapsed - k_sheep_star_linger_seconds) /
                              k_sheep_star_fade_seconds);
    float const fallen = state.dead ? 1.0F : smooth(state.death_progress / 0.6F);
    gag.star_centre =
        gag.star_centre + ((k_fallen_star_centre - gag.star_centre) * fallen);
    gag.star_radius = 0.17F + (0.05F * fallen);
    return gag;
  }

  if (state.dazed > 0.0F) {
    float const daze = smooth(state.dazed / 0.25F);
    gag.stars = daze;
    gag.sway_roll = std::sin(state.time * 6.8F) * 9.0F * daze;
    gag.sway_pitch = std::sin((state.time * 4.1F) + 1.1F) * 4.0F * daze;
    gag.sway_yaw = std::sin(state.time * 2.7F) * 8.0F * daze;
  }
  return gag;
}

auto sheep_sway_matrix(const SheepSlapstick& gag) -> QMatrix4x4 {
  QMatrix4x4 sway;
  sway.rotate(gag.sway_yaw, 0.0F, 1.0F, 0.0F);
  sway.rotate(gag.sway_roll, 0.0F, 0.0F, 1.0F);
  sway.rotate(gag.sway_pitch, 1.0F, 0.0F, 0.0F);
  return sway;
}

auto sheep_wool_tuft(std::uint32_t seed, int index, float poof_time) -> WoolTuft {
  WoolTuft tuft;
  if (poof_time < 0.0F || poof_time >= k_sheep_wool_poof_seconds) {
    return tuft;
  }
  auto const salt = static_cast<std::uint32_t>(index) * 7U;
  float const angle =
      k_two_pi *
      ((static_cast<float>(index) / static_cast<float>(k_sheep_wool_tuft_count)) +
       (hash_unit_float(seed, 101U + salt) * 0.08F));
  float const climb = 0.45F + (hash_unit_float(seed, 103U + salt) * 0.75F);
  float const speed = 1.4F + (hash_unit_float(seed, 107U + salt) * 1.0F);
  QVector3D const direction =
      QVector3D(std::cos(angle), climb, std::sin(angle)).normalized();

  constexpr float k_drag = 4.0F;
  float const travel = speed * (1.0F - std::exp(-k_drag * poof_time)) / k_drag;
  float const sink = 0.13F * poof_time * poof_time;
  float const rock = std::sin((poof_time * 5.0F) + static_cast<float>(index)) * 0.035F *
                     smooth(poof_time / 0.3F);
  tuft.position =
      k_wool_burst_origin + (direction * travel) + QVector3D(rock, -sink, rock * 0.6F);
  tuft.position.setY(std::max(tuft.position.y(), 0.03F));

  float const base = 0.045F + (hash_unit_float(seed, 109U + salt) * 0.035F);
  float const grow = smooth(poof_time / 0.12F);
  float const shrink = 1.0F - smooth((poof_time - (k_sheep_wool_poof_seconds - 1.0F)));
  tuft.radius = base * grow * shrink * (1.0F + (0.35F * smooth(poof_time / 0.6F)));
  return tuft;
}

void submit_sheep_slapstick(const QMatrix4x4& model,
                            const SheepSlapstick& gag,
                            float time,
                            std::uint32_t seed,
                            const QVector3D& wool,
                            ISubmitter& out) {
  if (gag.stars > 0.01F) {
    Mesh* cube = get_unit_cube();
    float const turn = time * 5.2F;
    for (int i = 0; i < k_sheep_star_count; ++i) {
      float const angle = turn + (k_two_pi * static_cast<float>(i) /
                                  static_cast<float>(k_sheep_star_count));
      QVector3D const centre =
          gag.star_centre + QVector3D(std::cos(angle) * gag.star_radius,
                                      std::sin((angle * 2.0F) + time) * 0.025F,
                                      std::sin(angle) * gag.star_radius);
      float const twinkle =
          0.85F + (0.15F * std::sin((time * 9.0F) + static_cast<float>(i)));
      submit_star(out, cube, model, centre, angle * 140.0F, gag.stars * twinkle);
    }
  }

  if (gag.poof_time >= 0.0F) {
    Mesh* sphere =
        get_unit_sphere(k_coarse_latitude_segments, k_coarse_radial_segments);
    for (int i = 0; i < k_sheep_wool_tuft_count; ++i) {
      WoolTuft const tuft = sheep_wool_tuft(seed, i, gag.poof_time);
      if (tuft.radius <= 0.002F) {
        continue;
      }
      QMatrix4x4 puff = model;
      puff.translate(tuft.position);
      puff.scale(tuft.radius, tuft.radius * 0.82F, tuft.radius);
      out.mesh(sphere, puff, wool);
    }
  }
}

} // namespace Render::GL::Wildlife
