#include "render/entity/marketplace_renderer_common.h"

#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "render/entity/civilian_actor.h"
#include "render/frame/entity_appearance.h"
#include "render/geom/primitives.h"
#include "render/geom/transforms.h"

namespace Render::GL {
namespace {

constexpr float k_cloth_far_metres = 95.0F;
constexpr int k_cloth_material = 3;
constexpr int k_produce_material = 4;
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
    normal.normalize();
    const int stripes = std::max(1, awning.stripes);
    const float stripe_w = awning.half_width * 2.0F / static_cast<float>(stripes);
    const float seg_len = length / static_cast<float>(k_segments);
    const float width_sag = 0.065F * awning.half_width;
    for (int s = 0; s < stripes; ++s) {
      if (tattered && (s + static_cast<int>(index)) % 3 == 0) {
        continue;
      }
      const float offset =
          -awning.half_width + stripe_w * (static_cast<float>(s) + 0.5F);
      const float across_t = offset / std::max(awning.half_width, 1.0e-4F);
      const float sag = width_sag * (1.0F - across_t * across_t);
      const float sag_slope = -2.0F * width_sag * offset /
                              std::max(awning.half_width * awning.half_width, 1.0e-6F);
      const QVector3D x_axis = (across - normal * sag_slope).normalized();
      const QVector3D& color = (s % 2 == 0) ? awning.stripe_a : awning.stripe_b;
      const float weather = 0.93F + 0.07F * unit_hash(owner * 389U + index * 31U +
                                                      static_cast<std::uint32_t>(s));
      QVector3D previous = awning.back + across * offset - normal * sag;
      for (int k = 1; k <= k_segments; ++k) {
        const float t = static_cast<float>(k) / static_cast<float>(k_segments);
        const float billow =
            std::sin(t * std::numbers::pi_v<float>) * length * (0.065F - 0.022F * gust);
        const float ripple_phase = time * 1.65F - t * 4.2F + phase + offset * 1.2F;

        const float envelope = 4.0F * t * (1.0F - t);
        const float ripple = std::sin(ripple_phase) * 0.008F * envelope * gust;
        const QVector3D point = awning.back + across * offset + along * (t * length) -
                                normal * (billow - ripple + sag * (1.0F - 0.35F * t));
        const QVector3D mid = (previous + point) * 0.5F;
        const QVector3D dir = point - previous;
        QMatrix4x4 local;
        local.translate(mid);
        QVector3D z_axis = dir.normalized();
        QVector3D y_axis = QVector3D::crossProduct(z_axis, x_axis).normalized();
        const QVector3D cloth_across = QVector3D::crossProduct(y_axis, z_axis);
        QMatrix4x4 basis(cloth_across.x(),
                         y_axis.x(),
                         z_axis.x(),
                         0.0F,
                         cloth_across.y(),
                         y_axis.y(),
                         z_axis.y(),
                         0.0F,
                         cloth_across.z(),
                         y_axis.z(),
                         z_axis.z(),
                         0.0F,
                         0.0F,
                         0.0F,
                         0.0F,
                         1.0F);
        local *= basis;
        local.scale(stripe_w * 0.51F, 0.002F, std::max(dir.length(), seg_len) * 0.52F);
        const float fold = 0.98F - 0.05F * (billow / length);
        out.mesh(cube,
                 ctx.model * local,
                 color * (fold * weather),
                 nullptr,
                 1.0F,
                 k_cloth_material);
        previous = point;
      }
      const float flap =
          std::sin(time * 2.1F + phase + offset * 1.4F) * (0.08F + 0.18F * gust);
      const QVector3D hang_dir =
          (QVector3D(0.0F, -1.0F, 0.0F) + along * flap).normalized();
      const QVector3D z_axis = QVector3D::crossProduct(across, hang_dir).normalized();
      QMatrix4x4 const drop_basis(across.x(),
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
      const QVector3D& valance = (s % 2 == 0) ? awning.stripe_b : awning.stripe_a;
      const QVector3D tip = previous + hang_dir * 0.050F;
      QMatrix4x4 drop;
      drop.translate((previous + tip) * 0.5F);
      drop *= drop_basis;
      drop.scale(stripe_w * 0.49F, 0.025F, 0.002F);
      out.mesh(
          cube, ctx.model * drop, valance * weather, nullptr, 1.0F, k_cloth_material);

      const QVector3D point_tip = tip + hang_dir * 0.022F;
      QMatrix4x4 scallop;
      scallop.translate((tip + point_tip) * 0.5F);
      scallop *= drop_basis;
      scallop.rotate(45.0F, 0.0F, 0.0F, 1.0F);
      scallop.scale(stripe_w * 0.26F, stripe_w * 0.26F, 0.002F);
      out.mesh(cube,
               ctx.model * scallop,
               valance * (weather * 0.96F),
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
    const float frequency =
        std::clamp(std::sqrt(0.75F / std::max(hanging.length, 0.04F)), 1.2F, 3.0F);
    const float swing = std::sin(time * frequency + phase) * 0.11F * (0.35F + gust);
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
               k_produce_material);
    }
  }
}

void register_marketplace_renderer_variant(EntityRendererRegistry& registry,
                                           const MarketplaceRendererConfig& config) {
  register_building_renderer(
      registry,
      config.nation_slug,
      "marketplace",
      [config](const DrawContext& ctx,
               const EntityPresentation& presentation,
               ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }
        const BuildingState state = presented_building_state(presentation, ctx);
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
