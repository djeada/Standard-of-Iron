#include "marketplace_renderer_common.h"

#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "../entity_appearance.h"
#include "civilian_actor.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"

namespace Render::GL {
namespace {

constexpr float k_cloth_far_metres = 95.0F;
constexpr int k_cloth_material = 3;
constexpr float k_tau = 2.0F * std::numbers::pi_v<float>;

auto unit_hash(std::uint32_t value) -> float {
  return static_cast<float>(ambient_hash(value)) * (1.0F / 4294967296.0F);
}

} // namespace

void submit_market_awnings(const DrawContext& ctx,
                           ISubmitter& out,
                           std::span<const MarketAwning> awnings,
                           std::span<const MarketHanging> hangings) {
  if ((awnings.empty() && hangings.empty()) || ctx.template_prewarm) {
    return;
  }
  if (ctx.distance_sq > k_cloth_far_metres * k_cloth_far_metres) {
    return;
  }
  const BuildingState state = resolve_building_state(ctx);
  if (state == BuildingState::Destroyed) {
    return;
  }
  const bool tattered = state == BuildingState::Damaged;
  const float time = ctx.animation_time;
  const auto owner =
      ctx.entity != nullptr ? static_cast<std::uint32_t>(ctx.entity->get_id()) : 0U;
  const float gust_phase = unit_hash(owner * 131U + 7U) * k_tau;
  const float gust = 0.55F + 0.45F * std::sin(time * 0.37F + gust_phase) *
                                 std::sin(time * 0.11F + gust_phase * 0.5F);
  Mesh* const cube = get_unit_cube();
  constexpr int k_segments = 6;

  std::uint32_t index = 0;
  for (const MarketAwning& awning : awnings) {
    ++index;
    const float phase = unit_hash(owner * 977U + index) * k_tau;
    const QVector3D run = awning.front - awning.back;
    const float length = run.length();
    if (length < 1.0e-4F) {
      continue;
    }
    const QVector3D along = run / length;
    const QVector3D across = awning.width_axis.normalized();
    QVector3D normal = QVector3D::crossProduct(across, along);
    if (normal.y() < 0.0F) {
      normal = -normal;
    }
    const int stripes = std::max(1, awning.stripes);
    const float stripe_w = awning.half_width * 2.0F / static_cast<float>(stripes);
    const float seg_len = length / static_cast<float>(k_segments);
    for (int s = 0; s < stripes; ++s) {
      if (tattered && (s + static_cast<int>(index)) % 3 == 0) {
        continue;
      }
      const float offset =
          -awning.half_width + stripe_w * (static_cast<float>(s) + 0.5F);
      const QVector3D& color = (s % 2 == 0) ? awning.stripe_a : awning.stripe_b;
      QVector3D previous = awning.back + across * offset;
      for (int k = 1; k <= k_segments; ++k) {
        const float t = static_cast<float>(k) / static_cast<float>(k_segments);
        const float billow =
            std::sin(t * std::numbers::pi_v<float>) * 0.035F * length * (0.6F + gust);
        const float ripple = std::sin(time * 2.3F - t * 4.2F + phase + offset * 3.0F) *
                             0.012F * t * (0.4F + gust);
        const QVector3D point = awning.back + across * offset + along * (t * length) -
                                normal * (billow - ripple);
        const QVector3D mid = (previous + point) * 0.5F;
        const QVector3D dir = point - previous;
        QMatrix4x4 local;
        local.translate(mid);
        QVector3D x_axis = across;
        QVector3D z_axis = dir.normalized();
        QVector3D y_axis = QVector3D::crossProduct(z_axis, x_axis).normalized();
        QMatrix4x4 basis(x_axis.x(),
                         y_axis.x(),
                         z_axis.x(),
                         0.0F,
                         x_axis.y(),
                         y_axis.y(),
                         z_axis.y(),
                         0.0F,
                         x_axis.z(),
                         y_axis.z(),
                         z_axis.z(),
                         0.0F,
                         0.0F,
                         0.0F,
                         0.0F,
                         1.0F);
        local *= basis;
        local.scale(stripe_w * 0.5F, 0.006F, std::max(dir.length(), seg_len) * 0.52F);
        out.mesh(cube, ctx.model * local, color, nullptr, 1.0F, k_cloth_material);
        previous = point;
      }
      const float flap = std::sin(time * 3.1F + phase + static_cast<float>(s) * 1.3F) *
                         (0.20F + 0.25F * gust);
      const QVector3D hang_dir =
          (QVector3D(0.0F, -1.0F, 0.0F) + along * flap).normalized();
      const QVector3D tip = previous + hang_dir * 0.055F;
      QMatrix4x4 drop;
      drop.translate((previous + tip) * 0.5F);
      const QVector3D z_axis = QVector3D::crossProduct(across, hang_dir).normalized();
      QMatrix4x4 drop_basis(across.x(),
                            hang_dir.x(),
                            z_axis.x(),
                            0.0F,
                            across.y(),
                            hang_dir.y(),
                            z_axis.y(),
                            0.0F,
                            across.z(),
                            hang_dir.z(),
                            z_axis.z(),
                            0.0F,
                            0.0F,
                            0.0F,
                            0.0F,
                            1.0F);
      drop *= drop_basis;
      drop.scale(stripe_w * 0.46F, 0.0275F, 0.005F);
      out.mesh(cube,
               ctx.model * drop,
               (s % 2 == 0) ? awning.stripe_b : awning.stripe_a,
               nullptr,
               1.0F,
               k_cloth_material);
    }
  }

  Mesh* const cylinder = get_unit_cylinder(6);
  Mesh* const sphere = get_unit_sphere(6, 8);
  for (const MarketHanging& hanging : hangings) {
    ++index;
    const float phase = unit_hash(owner * 613U + index) * k_tau;
    const float swing = std::sin(time * 1.7F + phase) * 0.16F * (0.35F + gust);
    const float twist = std::cos(time * 1.3F + phase * 1.7F) * 0.08F * (0.35F + gust);
    const QVector3D dir = QVector3D(swing, -1.0F, twist).normalized();
    const QVector3D end = hanging.pivot + dir * hanging.length;
    out.mesh(cylinder,
             ctx.model * Render::Geom::cylinder_between(hanging.pivot, end, 0.004F),
             QVector3D(0.42F, 0.34F, 0.22F),
             nullptr,
             1.0F,
             k_cloth_material);
    for (int b = 0; b < std::max(1, hanging.beads); ++b) {
      const float t = 1.0F - static_cast<float>(b) * 0.28F;
      QMatrix4x4 bead;
      bead.translate(hanging.pivot + dir * (hanging.length * t));
      bead.scale(hanging.radius * (1.0F - 0.12F * static_cast<float>(b)));
      out.mesh(sphere,
               ctx.model * bead,
               hanging.color * (1.0F - 0.06F * static_cast<float>(b)),
               nullptr,
               1.0F,
               0);
    }
  }
}

void register_marketplace_renderer_variant(EntityRendererRegistry& registry,
                                           const MarketplaceRendererConfig& config) {
  register_building_renderer(
      registry,
      config.nation_slug,
      "marketplace",
      [config](const DrawContext& ctx, ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }
        const BuildingState state = resolve_building_state(ctx);
        if (config.palette_slots != nullptr) {
          const QVector3D team = Render::entity_color(*ctx.entity);
          const auto palette_slots = config.palette_slots(team);
          submit_building_instance(out, ctx, config.archetype(state), palette_slots);
        } else {
          submit_building_instance(out, ctx, config.archetype(state));
        }
        submit_building_torches(ctx, out, config.torches);
        submit_market_awnings(ctx, out, config.awnings, config.hangings);
        submit_ambient_people(ctx,
                              out,
                              config.nation_slug == "carthage",
                              config.people,
                              config.walk_surfaces);
        draw_building_selection_overlay(out, ctx, config.selection);
      });
}

} // namespace Render::GL
