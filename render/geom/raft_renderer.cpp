#include "raft_renderer.h"

#include <QVector3D>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "render/gl/primitives.h"
#include "render/scene_renderer.h"
#include "transforms.h"

namespace Render::GL {

namespace {

const QVector3D k_log_dark(0.34F, 0.24F, 0.14F);
const QVector3D k_log_light(0.50F, 0.37F, 0.22F);
const QVector3D k_lashing(0.62F, 0.55F, 0.40F);
const QVector3D k_post(0.30F, 0.22F, 0.14F);
const QVector3D k_rope(0.70F, 0.62F, 0.46F);

constexpr int k_log_count = 9;
constexpr float k_rope_height = 1.7F;
constexpr float k_post_height = 2.1F;
constexpr float k_post_sink = 0.8F;
constexpr float k_rope_side_clearance = 0.45F;

[[nodiscard]] auto log_hash(std::uint32_t seed, std::uint32_t salt) -> float {
  std::uint32_t value = seed * 0x9e3779b9U ^ (salt * 0x85ebca6bU);
  value ^= value >> 15U;
  value *= 0x2c1b3c6dU;
  value ^= value >> 12U;
  return static_cast<float>(value & 0xffffU) / 65535.0F;
}

void draw_cylinder(Renderer* renderer,
                   Mesh* cylinder,
                   const QVector3D& a,
                   const QVector3D& b,
                   float radius,
                   const QVector3D& color) {
  renderer->mesh(cylinder, Geom::cylinder_between(a, b, radius), color, nullptr, 1.0F);
}

void draw_raft(Renderer* renderer,
               Mesh* cylinder,
               const RaftDrawable& raft,
               float time) {
  QVector3D const up(0.0F, 1.0F, 0.0F);
  QVector3D const forward = raft.across;
  QVector3D const lateral(-forward.z(), 0.0F, forward.x());
  float const phase = log_hash(raft.seed, 1U) * 6.2831853F;
  float const bob = std::sin(time * 1.6F + phase) * (raft.moving ? 0.035F : 0.02F);
  float const roll = std::sin(time * 1.1F + phase * 0.7F) * 0.025F;

  float const log_radius = raft.half_width / static_cast<float>(k_log_count);
  QVector3D const deck_centre = raft.position + up * bob;
  for (int i = 0; i < k_log_count; ++i) {
    float const across =
        (static_cast<float>(i) + 0.5F) / static_cast<float>(k_log_count) * 2.0F - 1.0F;
    std::uint32_t const salt = static_cast<std::uint32_t>(i) * 7U;
    float const stagger = (log_hash(raft.seed, salt + 2U) - 0.5F) * 0.35F;
    float const thickness =
        log_radius * (0.92F + 0.12F * log_hash(raft.seed, salt + 3U));
    QVector3D const centre = deck_centre + lateral * (across * raft.half_width) +
                             up * (across * roll * raft.half_width) + forward * stagger;
    QVector3D const color =
        k_log_dark + (k_log_light - k_log_dark) * log_hash(raft.seed, salt + 4U);
    draw_cylinder(renderer,
                  cylinder,
                  centre - forward * raft.half_length,
                  centre + forward * raft.half_length,
                  thickness,
                  color);
  }

  float const beam_y = log_radius * 1.05F;
  for (float end : {-0.72F, 0.72F}) {
    QVector3D const mid =
        deck_centre + forward * (end * raft.half_length) + up * beam_y;
    draw_cylinder(
        renderer,
        cylinder,
        mid - lateral * (raft.half_width + 0.08F) - up * (roll * raft.half_width),
        mid + lateral * (raft.half_width + 0.08F) + up * (roll * raft.half_width),
        log_radius * 0.55F,
        k_lashing);
  }

  QVector3D const rope_side = lateral * (raft.half_width + k_rope_side_clearance);
  QVector3D const pole_foot =
      deck_centre + lateral * (raft.half_width - log_radius) + up * beam_y;
  QVector3D const rope_point =
      QVector3D(deck_centre.x(), raft.position.y(), deck_centre.z()) + rope_side +
      up * k_rope_height;
  draw_cylinder(renderer, cylinder, pole_foot, rope_point - up * 0.05F, 0.05F, k_post);
}

void draw_guide_rope(Renderer* renderer, Mesh* cylinder, const RaftDrawable& raft) {
  QVector3D const up(0.0F, 1.0F, 0.0F);
  QVector3D const forward = raft.across;
  QVector3D const lateral(-forward.z(), 0.0F, forward.x());
  QVector3D const rope_side = lateral * (raft.half_width + k_rope_side_clearance);
  float const water_y = raft.position.y();

  std::array<QVector3D, 2> tops;
  for (int side = 0; side < 2; ++side) {
    QVector3D const outward = side == 0 ? -forward : forward;
    QVector3D base = raft.docks[side] + outward * (raft.half_length + 0.5F) + rope_side;
    base.setY(water_y - k_post_sink);
    QVector3D const top = QVector3D(base.x(), water_y + k_post_height, base.z());
    draw_cylinder(renderer, cylinder, base, top, 0.09F, k_post);
    tops[static_cast<std::size_t>(side)] =
        QVector3D(base.x(), water_y + k_rope_height, base.z());
  }

  constexpr int k_rope_links = 6;
  constexpr float k_rope_sag = 0.22F;
  QVector3D previous = tops[0];
  for (int i = 1; i <= k_rope_links; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(k_rope_links);
    QVector3D point = tops[0] + (tops[1] - tops[0]) * t;
    point -= up * (k_rope_sag * 4.0F * t * (1.0F - t));
    draw_cylinder(renderer, cylinder, previous, point, 0.025F, k_rope);
    previous = point;
  }
}

} // namespace

void draw_rafts(Renderer* renderer, std::span<const RaftDrawable> rafts) {
  if (renderer == nullptr || rafts.empty()) {
    return;
  }
  auto* cylinder = get_unit_cylinder();
  if (cylinder == nullptr) {
    return;
  }
  float const time = renderer->get_animation_time();
  for (auto const& raft : rafts) {
    draw_guide_rope(renderer, cylinder, raft);
    draw_raft(renderer, cylinder, raft, time);
  }
}

} // namespace Render::GL
