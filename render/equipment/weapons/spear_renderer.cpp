#include "spear_renderer.h"
#include "../../entity/renderer_constants.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/spear_pose_utils.h"
#include "../equipment_submit.h"
#include "../generated_equipment.h"
#include "../oriented_archetype_utils.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>

namespace Render::GL {

namespace {

enum SpearPaletteSlot : std::uint8_t {
  k_shaft_slot = 0U,
  k_shaft_dark_slot = 1U,
  k_head_slot = 2U,
  k_grip_slot = 3U,
};

struct SpearArchetypeKey {
  int shaft_radius_key{0};
  int material_id{0};
};

auto operator==(const SpearArchetypeKey &lhs,
                const SpearArchetypeKey &rhs) -> bool {
  return lhs.shaft_radius_key == rhs.shaft_radius_key &&
         lhs.material_id == rhs.material_id;
}

auto quantize_spear_value(float value) -> int {
  return std::lround(value * 1000.0F);
}

auto spear_lower_shaft_archetype(const SpearRenderConfig &config)
    -> const RenderArchetype & {
  struct CachedArchetype {
    SpearArchetypeKey key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  SpearArchetypeKey const key{quantize_spear_value(config.shaft_radius),
                              config.material_id};
  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }
  std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
      generated_cylinder(QVector3D(0.0F, 0.0F, 0.0F),
                         QVector3D(0.0F, 1.0F, 0.0F), config.shaft_radius,
                         k_shaft_slot, 1.0F, config.material_id),
  }};
  cache.push_back(
      {key, build_generated_equipment_archetype(
                "spear_lower_shaft_" + std::to_string(key.shaft_radius_key) +
                    "_" + std::to_string(key.material_id),
                primitives)});
  return cache.back().archetype;
}

auto spear_upper_shaft_archetype(const SpearRenderConfig &config)
    -> const RenderArchetype & {
  struct CachedArchetype {
    SpearArchetypeKey key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  SpearArchetypeKey const key{quantize_spear_value(config.shaft_radius),
                              config.material_id};
  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }
  std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
      generated_cylinder(QVector3D(0.0F, 0.0F, 0.0F),
                         QVector3D(0.0F, 1.0F, 0.0F),
                         config.shaft_radius * 0.95F, k_shaft_dark_slot, 1.0F,
                         config.material_id),
  }};
  cache.push_back(
      {key, build_generated_equipment_archetype(
                "spear_upper_shaft_" + std::to_string(key.shaft_radius_key) +
                    "_" + std::to_string(key.material_id),
                primitives)});
  return cache.back().archetype;
}

auto spearhead_archetype(const SpearRenderConfig &config)
    -> const RenderArchetype & {
  struct CachedArchetype {
    SpearArchetypeKey key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  SpearArchetypeKey const key{quantize_spear_value(config.shaft_radius),
                              config.material_id};
  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }
  std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
      generated_cone(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F),
                     config.shaft_radius * 1.8F, k_head_slot, 1.0F,
                     config.material_id),
  }};
  cache.push_back(
      {key, build_generated_equipment_archetype(
                "spear_head_" + std::to_string(key.shaft_radius_key) + "_" +
                    std::to_string(key.material_id),
                primitives)});
  return cache.back().archetype;
}

auto spear_grip_archetype(const SpearRenderConfig &config)
    -> const RenderArchetype & {
  struct CachedArchetype {
    SpearArchetypeKey key;
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  SpearArchetypeKey const key{quantize_spear_value(config.shaft_radius),
                              config.material_id};
  for (const auto &entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }
  std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
      generated_cylinder(
          QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F),
          config.shaft_radius * 1.5F, k_grip_slot, 1.0F, config.material_id),
  }};
  cache.push_back(
      {key, build_generated_equipment_archetype(
                "spear_grip_" + std::to_string(key.shaft_radius_key) + "_" +
                    std::to_string(key.material_id),
                primitives)});
  return cache.back().archetype;
}

auto spear_palette(const SpearRenderConfig &config,
                   const HumanoidPalette &palette) -> std::array<QVector3D, 4> {
  return {config.shaft_color, config.shaft_color * 0.98F,
          config.spearhead_color, palette.leather * 0.92F};
}

} // namespace

SpearRenderer::SpearRenderer(SpearRenderConfig config)
    : m_base(std::move(config)) {}

void SpearRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           EquipmentBatch &batch) {
  submit(m_base, ctx, frames, palette, anim, batch);
}

void SpearRenderer::submit(const SpearRenderConfig &m_config,
                           const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           EquipmentBatch &batch) {
  QVector3D const grip_pos = frames.hand_r.origin;

  QVector3D const spear_dir = compute_spear_direction(anim.inputs);

  QVector3D const shaft_base = grip_pos - spear_dir * 0.28F;
  QVector3D shaft_mid = grip_pos + spear_dir * (m_config.spear_length * 0.5F);
  QVector3D const shaft_tip = grip_pos + spear_dir * m_config.spear_length;

  shaft_mid.setY(shaft_mid.y() + 0.02F);
  auto const palette_slots = spear_palette(m_config, palette);
  QVector3D right_hint = frames.hand_r.right;
  if (right_hint.lengthSquared() < 1e-6F) {
    right_hint = QVector3D(0.0F, 0.0F, 1.0F);
  }

  append_equipment_archetype(batch, spear_lower_shaft_archetype(m_config),
                             oriented_segment_transform(ctx.model, shaft_base,
                                                        shaft_mid - shaft_base,
                                                        right_hint),
                             palette_slots);

  append_equipment_archetype(batch, spear_upper_shaft_archetype(m_config),
                             oriented_segment_transform(ctx.model, shaft_mid,
                                                        shaft_tip - shaft_mid,
                                                        right_hint),
                             palette_slots);

  QVector3D const spearhead_base = shaft_tip;
  QVector3D const spearhead_tip =
      shaft_tip + spear_dir * m_config.spearhead_length;
  append_equipment_archetype(
      batch, spearhead_archetype(m_config),
      oriented_segment_transform(ctx.model, spearhead_base,
                                 spearhead_tip - spearhead_base, right_hint),
      palette_slots);

  QVector3D const grip_end = grip_pos + spear_dir * 0.10F;
  append_equipment_archetype(batch, spear_grip_archetype(m_config),
                             oriented_segment_transform(ctx.model, grip_pos,
                                                        grip_end - grip_pos,
                                                        right_hint),
                             palette_slots);
}

} // namespace Render::GL
