#include "building_torches.h"

#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "building_render_common.h"
#include "building_state.h"
#include "home_activity.h"
#include "registry.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/local_lighting.h"
#include "render/submitter.h"

namespace Render::GL {
namespace {

constexpr float k_tau = 6.2831853F;
constexpr float k_bracket_reach = 0.09F;
constexpr float k_shaft_length = 0.30F;
constexpr float k_shaft_radius = 0.016F;
constexpr float k_head_length = 0.055F;
constexpr float k_head_radius = 0.026F;
constexpr float k_lean = 0.30F;
constexpr float k_light_reach = 5.0F;
constexpr float k_light_intensity = 1.05F;
constexpr float k_lit_from_night = 0.08F;
constexpr float k_far_metres = 150.0F;

const QVector3D k_iron{0.20F, 0.19F, 0.18F};
const QVector3D k_shaft{0.42F, 0.29F, 0.17F};
const QVector3D k_pitch{0.12F, 0.09F, 0.07F};
const QVector3D k_flame_outer{1.00F, 0.46F, 0.12F};
const QVector3D k_flame_inner{1.00F, 0.86F, 0.42F};
const QVector3D k_firelight{1.00F, 0.58F, 0.26F};

auto hash01(std::uint32_t value) -> float {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return static_cast<float>(value & 0xffffU) / 65536.0F;
}

auto smoothstep(float edge0, float edge1, float x) -> float {
  const float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5F), 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

auto cylinder(const QVector3D& a, const QVector3D& b, float radius) -> QMatrix4x4 {
  return Render::Geom::cylinder_between(a, b, radius);
}

const QVector3D k_bronze{0.46F, 0.31F, 0.16F};
const QVector3D k_coals{0.55F, 0.12F, 0.03F};

void submit_brazier(ISubmitter& out,
                    const QVector3D& ground,
                    float night,
                    float time,
                    std::uint32_t seed) {
  constexpr float k_bowl_height = 0.42F;
  constexpr float k_bowl_radius = 0.15F;
  Mesh* const unit_cylinder = get_unit_cylinder();
  Mesh* const unit_cone = get_unit_cone();
  const QVector3D up(0.0F, 1.0F, 0.0F);
  const QVector3D bowl = ground + up * k_bowl_height;
  for (int leg = 0; leg < 3; ++leg) {
    const float angle = (static_cast<float>(leg) / 3.0F) * k_tau + 0.4F;
    const QVector3D foot = ground + QVector3D(std::cos(angle), 0.0F, std::sin(angle)) *
                                        (k_bowl_radius * 0.95F);
    out.mesh(unit_cylinder,
             cylinder(foot, bowl - up * 0.03F, 0.012F),
             k_bronze * 0.8F,
             nullptr,
             1.0F);
  }
  out.mesh(
      unit_cone,
      Render::Geom::cone_from_to(bowl + up * 0.05F, bowl - up * 0.07F, k_bowl_radius),
      k_bronze,
      nullptr,
      1.0F);
  out.mesh(unit_cylinder,
           cylinder(bowl + up * 0.035F, bowl + up * 0.05F, k_bowl_radius * 0.86F),
           k_coals,
           nullptr,
           1.0F);

  const float burn = 0.55F + 0.45F * night;
  const float phase = hash01(seed) * k_tau;
  for (int tongue = 0; tongue < 3; ++tongue) {
    const float offset_angle = phase + static_cast<float>(tongue) * 2.1F;
    const float flick = 0.78F + 0.22F * std::sin(time * (7.3F + tongue) + offset_angle);
    const QVector3D base =
        bowl + up * 0.05F +
        QVector3D(std::cos(offset_angle), 0.0F, std::sin(offset_angle)) * 0.04F;
    const QVector3D tip =
        base + up * (0.30F * flick * burn) +
        QVector3D(std::sin(time * 2.9F + offset_angle), 0.0F, 0.0F) * 0.03F;
    out.mesh(unit_cone,
             Render::Geom::cone_from_to(base, tip, 0.075F),
             tongue == 0 ? k_flame_inner : k_flame_outer,
             nullptr,
             1.0F);
  }
  Render::LocalLight light;
  light.position = bowl + up * 0.35F;
  light.color = k_firelight;
  light.radius = 7.0F;
  light.intensity =
      (0.35F + 1.05F * night) * (0.9F + 0.1F * std::sin(time * 8.3F + phase));
  out.local_light(light);
}

} // namespace

void submit_temple_incense(const DrawContext& ctx,
                           ISubmitter& out,
                           std::span<const QVector3D> vents) {
  if (vents.empty() || ctx.template_prewarm || ctx.entity == nullptr ||
      ctx.distance_sq > k_far_metres * k_far_metres) {
    return;
  }
  if (resolve_building_state(ctx) == BuildingState::Destroyed) {
    return;
  }
  const auto owner = static_cast<std::uint32_t>(ctx.entity->get_id());
  std::uint32_t index = 0;
  for (const QVector3D& vent : vents) {
    ++index;
    out.hearth_smoke(ctx.model.map(vent),
                     QVector3D(0.74F, 0.73F, 0.78F),
                     0.30F,
                     0.34F,
                     ctx.animation_time * 0.55F + hash01(owner + index) * 50.0F);
  }
}

void submit_building_torches(const DrawContext& ctx,
                             ISubmitter& out,
                             std::span<const TorchMount> mounts) {
  if (mounts.empty() || ctx.template_prewarm) {
    return;
  }
  if (ctx.entity != nullptr &&
      resolve_building_state(ctx) == BuildingState::Destroyed) {
    return;
  }
  if (ctx.distance_sq > k_far_metres * k_far_metres) {
    return;
  }

  const float night = ctx.home_activity != nullptr ? ctx.home_activity->night() : 0.0F;
  const float lit = smoothstep(k_lit_from_night, k_lit_from_night + 0.25F, night);
  const float time = ctx.animation_time;
  const auto owner =
      ctx.entity != nullptr ? static_cast<std::uint32_t>(ctx.entity->get_id()) : 0U;
  Mesh* const unit_cylinder = get_unit_cylinder();
  Mesh* const unit_cone = get_unit_cone();

  std::uint32_t index = 0;
  for (const TorchMount& mount : mounts) {
    ++index;
    if (mount.brazier) {
      submit_brazier(out, ctx.model.map(mount.at), lit, time, owner * 97U + index);
      continue;
    }
    const QVector3D wall = ctx.model.map(mount.at);
    QVector3D outward = ctx.model.mapVector(mount.outward);
    outward.setY(0.0F);
    if (outward.lengthSquared() < 1e-6F) {
      outward = QVector3D(0.0F, 0.0F, 1.0F);
    }
    outward.normalize();
    const QVector3D up(0.0F, 1.0F, 0.0F);

    const QVector3D elbow = wall + outward * k_bracket_reach;
    const QVector3D socket = elbow + up * 0.045F;
    out.mesh(unit_cylinder, cylinder(wall, elbow, 0.008F), k_iron, nullptr, 1.0F);
    out.mesh(unit_cylinder,
             cylinder(elbow - up * 0.035F, socket, 0.020F),
             k_iron,
             nullptr,
             1.0F);

    const QVector3D axis = (up + outward * k_lean).normalized();
    const QVector3D butt = socket - axis * (k_shaft_length * 0.45F);
    const QVector3D neck = socket + axis * (k_shaft_length * 0.55F);
    const QVector3D crown = neck + axis * k_head_length;
    out.mesh(
        unit_cylinder, cylinder(butt, neck, k_shaft_radius), k_shaft, nullptr, 1.0F);
    out.mesh(
        unit_cylinder, cylinder(neck, crown, k_head_radius), k_pitch, nullptr, 1.0F);

    if (lit <= 0.0F) {
      continue;
    }
    const std::uint32_t seed = owner * 2654435761U + index * 40503U;
    const float phase = hash01(seed) * k_tau;
    const float flicker = 0.80F + (0.12F * std::sin((time * 9.1F) + phase)) +
                          (0.08F * std::sin((time * 23.7F) + (phase * 1.7F)));
    const float sway = 0.018F * std::sin((time * 3.3F) + phase);
    const QVector3D side = QVector3D::crossProduct(up, outward).normalized();
    const QVector3D tip_drift = side * sway + outward * 0.012F;

    const float outer_height = 0.15F * flicker * lit;
    const float inner_height = 0.09F * (0.9F + 0.2F * flicker) * lit;
    out.mesh(unit_cone,
             Render::Geom::cone_from_to(
                 crown, crown + up * outer_height + tip_drift, k_head_radius * 1.35F),
             k_flame_outer,
             nullptr,
             1.0F);
    out.mesh(unit_cone,
             Render::Geom::cone_from_to(crown + up * 0.01F,
                                        crown + up * inner_height + tip_drift * 0.6F,
                                        k_head_radius * 0.80F),
             k_flame_inner,
             nullptr,
             1.0F);

    Render::LocalLight light;
    light.position = crown + up * 0.12F + outward * 0.05F;
    light.color = k_firelight;
    light.radius = k_light_reach;
    light.intensity = k_light_intensity * lit * (0.86F + 0.14F * flicker);
    out.local_light(light);
  }
}

} // namespace Render::GL
