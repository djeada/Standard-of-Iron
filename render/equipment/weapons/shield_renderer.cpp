#include "shield_renderer.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../render_archetype.h"
#include "../equipment_submit.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <numbers>
#include <string>

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace {

enum ShieldPaletteSlot : std::uint8_t {
  k_shield_slot = 0U,
  k_back_slot = 1U,
  k_trim_slot = 2U,
  k_inner_ring_slot = 3U,
  k_metal_slot = 4U,
  k_grip_slot = 5U,
};

constexpr float k_scale_factor = 2.5F;
constexpr float k_shield_yaw_degrees = -70.0F;

struct ShieldArchetypeKey {
  int radius_key{0};
  int aspect_key{0};
  bool has_cross_decal{false};
  int material_id{0};
};

auto operator==(const ShieldArchetypeKey &lhs,
                const ShieldArchetypeKey &rhs) -> bool {
  return lhs.radius_key == rhs.radius_key && lhs.aspect_key == rhs.aspect_key &&
         lhs.has_cross_decal == rhs.has_cross_decal &&
         lhs.material_id == rhs.material_id;
}

auto quantize_shield_value(float value) -> int {
  return std::lround(value * 1000.0F);
}

auto shield_archetype(const ShieldRenderConfig &config)
    -> const RenderArchetype & {
  struct CachedArchetype {
    ShieldArchetypeKey key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;

  ShieldArchetypeKey const key{quantize_shield_value(config.shield_radius),
                               quantize_shield_value(config.shield_aspect),
                               config.has_cross_decal, config.material_id};
  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  float const base_extent = config.shield_radius * k_scale_factor;
  float const shield_width = base_extent;
  float const shield_height = base_extent * config.shield_aspect;
  float const min_extent = std::min(shield_width, shield_height);
  float const plate_half = 0.0015F;
  float const plate_full = plate_half * 2.0F;

  QVector3D const shield_center_local{-shield_width * 0.55F, -0.20F, 0.06F};

  RenderArchetypeBuilder builder{
      "shield_" + std::to_string(key.radius_key) + "_" +
      std::to_string(key.aspect_key) + "_" +
      std::to_string(static_cast<int>(key.has_cross_decal)) + "_" +
      std::to_string(key.material_id)};

  QMatrix4x4 front_plate;
  front_plate.translate(shield_center_local +
                        QVector3D(0.0F, 0.0F, plate_half));
  front_plate.scale(shield_width, shield_height, plate_full);
  builder.add_palette_mesh(get_unit_cylinder(), front_plate, k_shield_slot,
                           nullptr, 1.0F, config.material_id);

  QMatrix4x4 back_plate;
  back_plate.translate(shield_center_local - QVector3D(0.0F, 0.0F, plate_half));
  back_plate.scale(shield_width * 0.985F, shield_height * 0.985F, plate_full);
  builder.add_palette_mesh(get_unit_cylinder(), back_plate, k_back_slot,
                           nullptr, 1.0F, config.material_id);

  auto add_ring = [&](float width, float height, float thickness,
                      ShieldPaletteSlot slot) {
    constexpr int k_segments = 18;
    for (int i = 0; i < k_segments; ++i) {
      float const a0 = static_cast<float>(i) / static_cast<float>(k_segments) *
                       2.0F * std::numbers::pi_v<float>;
      float const a1 = static_cast<float>(i + 1) /
                       static_cast<float>(k_segments) * 2.0F *
                       std::numbers::pi_v<float>;
      QVector3D const p0 =
          shield_center_local +
          QVector3D(width * std::cos(a0), height * std::sin(a0), 0.0F);
      QVector3D const p1 =
          shield_center_local +
          QVector3D(width * std::cos(a1), height * std::sin(a1), 0.0F);
      builder.add_palette_mesh(get_unit_cylinder(),
                               cylinder_between(p0, p1, thickness), slot,
                               nullptr, 1.0F, config.material_id);
    }
  };

  add_ring(shield_width, shield_height, min_extent * 0.010F, k_trim_slot);
  add_ring(shield_width * 0.72F, shield_height * 0.72F, min_extent * 0.006F,
           k_inner_ring_slot);

  builder.add_palette_mesh(
      get_unit_sphere(),
      sphere_at(shield_center_local +
                    QVector3D(0.0F, 0.0F, 0.02F * k_scale_factor),
                0.045F * k_scale_factor),
      k_metal_slot, nullptr, 1.0F, config.material_id);

  builder.add_palette_mesh(
      get_unit_cylinder(),
      cylinder_between(shield_center_local + QVector3D(-0.035F, 0.0F, -0.030F),
                       shield_center_local + QVector3D(0.035F, 0.0F, -0.030F),
                       0.010F),
      k_grip_slot, nullptr, 1.0F, config.material_id);

  if (config.has_cross_decal) {
    QVector3D const center_front =
        shield_center_local +
        QVector3D(0.0F, 0.0F, plate_full * 0.5F + 0.0015F);
    float const bar_radius = min_extent * 0.10F;

    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(
            center_front + QVector3D(0.0F, shield_height * 0.90F, 0.0F),
            center_front - QVector3D(0.0F, shield_height * 0.90F, 0.0F),
            bar_radius),
        k_trim_slot, nullptr, 1.0F, config.material_id);
    builder.add_palette_mesh(
        get_unit_cylinder(),
        cylinder_between(
            center_front - QVector3D(shield_width * 0.90F, 0.0F, 0.0F),
            center_front + QVector3D(shield_width * 0.90F, 0.0F, 0.0F),
            bar_radius),
        k_trim_slot, nullptr, 1.0F, config.material_id);
  }

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

auto shield_transform(const QMatrix4x4 &parent,
                      const QVector3D &origin) -> QMatrix4x4 {
  QMatrix4x4 local;
  local.translate(origin);
  local.rotate(k_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);
  return parent * local;
}

} // namespace

ShieldRenderer::ShieldRenderer(ShieldRenderConfig config)
    : m_base(std::move(config)) {}

void ShieldRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                            const HumanoidPalette &palette,
                            const HumanoidAnimationContext &anim,
                            EquipmentBatch &batch) {
  submit(m_base, ctx, frames, palette, anim, batch);
}

void ShieldRenderer::submit(const ShieldRenderConfig &m_config,
                            const DrawContext &ctx, const BodyFrames &frames,
                            const HumanoidPalette &palette,
                            const HumanoidAnimationContext &,
                            EquipmentBatch &batch) {
  std::array<QVector3D, 6> const palette_slots{
      m_config.shield_color,       palette.leather * 0.8F,
      m_config.trim_color * 0.95F, palette.leather * 0.90F,
      m_config.metal_color,        palette.leather};
  append_equipment_archetype(batch, shield_archetype(m_config),
                             shield_transform(ctx.model, frames.hand_l.origin),
                             palette_slots);
}

} // namespace Render::GL
