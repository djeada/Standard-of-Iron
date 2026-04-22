#include "carthage_heavy_helmet.h"

#include "../generated_equipment.h"
#include "../humanoid_attachment_archetype.h"

#include "../../humanoid/style_palette.h"

#include <array>

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

enum CarthageHeavyPaletteSlot : std::uint8_t {
  k_metal_slot = 0U,
  k_metal_dark_slot = 1U,
  k_metal_light_slot = 2U,
  k_crest_slot = 3U,
};

auto carthage_heavy_shell_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 4> const primitives{{
        generated_sphere(QVector3D(0.0F, 0.60F, 0.0F), 1.65F, k_metal_slot,
                         1.0F, 2),
        generated_cylinder(QVector3D(0.0F, -0.44F, 0.0F),
                           QVector3D(0.0F, -0.06F, 0.0F), 1.78F,
                           k_metal_dark_slot, 1.0F, 2),
        generated_cylinder(QVector3D(0.0F, 2.28F, 0.0F),
                           QVector3D(0.0F, 2.72F, 0.0F), 0.08F,
                           k_metal_light_slot, 1.0F, 2),
        generated_sphere(QVector3D(0.0F, 2.72F, 0.0F), 0.12F,
                         k_metal_light_slot, 1.0F, 2),
    }};
    return build_generated_equipment_archetype("carthage_heavy_shell",
                                               primitives);
  }();
  return archetype;
}

auto carthage_heavy_neck_guard_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
        generated_cylinder(QVector3D(0.0F, -0.36F, -1.00F),
                           QVector3D(0.0F, -0.14F, -1.06F), 1.42F,
                           k_metal_dark_slot, 1.0F, 2),
    }};
    return build_generated_equipment_archetype("carthage_heavy_neck_guard",
                                               primitives);
  }();
  return archetype;
}

auto carthage_heavy_cheek_guards_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 2> const primitives{{
        generated_cylinder(QVector3D(-1.50F, 0.06F, 0.20F),
                           QVector3D(-1.22F, -0.46F, 0.28F), 0.22F,
                           k_metal_dark_slot, 1.0F, 2),
        generated_cylinder(QVector3D(1.50F, 0.06F, 0.20F),
                           QVector3D(1.22F, -0.46F, 0.28F), 0.22F,
                           k_metal_dark_slot, 1.0F, 2),
    }};
    return build_generated_equipment_archetype("carthage_heavy_cheek_guards",
                                               primitives);
  }();
  return archetype;
}

auto carthage_heavy_face_plate_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
        generated_cylinder(QVector3D(0.0F, -0.06F, 0.90F),
                           QVector3D(0.0F, 0.58F, 0.70F), 0.10F, k_metal_slot,
                           1.0F, 2),
    }};
    return build_generated_equipment_archetype("carthage_heavy_face_plate",
                                               primitives);
  }();
  return archetype;
}

auto carthage_heavy_crest_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 13> primitives{};
    primitives[0] = generated_cylinder(QVector3D(0.0F, 2.64F, 0.28F),
                                       QVector3D(0.0F, 2.04F, -0.58F), 0.044F,
                                       k_metal_light_slot, 1.0F, 2);

    QVector3D const crest_front{0.0F, 2.64F, 0.28F};
    QVector3D const crest_back{0.0F, 2.04F, -0.58F};
    for (int i = 0; i < 12; ++i) {
      float const t = static_cast<float>(i) / 11.0F;
      float const lateral = (static_cast<float>(i % 3) - 1.0F) * 0.036F;
      QVector3D const base = crest_front * (1.0F - t) + crest_back * t;
      float const lift = 0.70F - 0.34F * std::abs(t - 0.42F);
      float const droop = 0.16F * t;
      QVector3D const tip = base + QVector3D(lateral, lift, -droop);
      primitives[1 + i] =
          generated_cylinder(base, tip, 0.052F, k_crest_slot, 0.92F, 0);
    }

    return build_generated_equipment_archetype("carthage_heavy_crest",
                                               primitives);
  }();
  return archetype;
}

auto carthage_heavy_rivets_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 5> const primitives{{
        generated_sphere(QVector3D(-0.52F, 0.28F, 0.64F), 0.05F,
                         k_metal_light_slot, 1.0F, 2),
        generated_sphere(QVector3D(-0.26F, 0.28F, 0.64F), 0.05F,
                         k_metal_light_slot, 1.0F, 2),
        generated_sphere(QVector3D(0.0F, 0.28F, 0.64F), 0.05F,
                         k_metal_light_slot, 1.0F, 2),
        generated_sphere(QVector3D(0.26F, 0.28F, 0.64F), 0.05F,
                         k_metal_light_slot, 1.0F, 2),
        generated_sphere(QVector3D(0.52F, 0.28F, 0.64F), 0.05F,
                         k_metal_light_slot, 1.0F, 2),
    }};
    return build_generated_equipment_archetype("carthage_heavy_rivets",
                                               primitives);
  }();
  return archetype;
}

auto carthage_heavy_palette(const HumanoidPalette &palette,
                            const CarthageHeavyHelmetConfig &config)
    -> std::array<QVector3D, 4> {
  QVector3D const metal =
      saturate_color(palette.metal * QVector3D(1.40F, 1.15F, 0.65F));
  QVector3D const metal_dark = metal * 0.78F;
  QVector3D const metal_light = saturate_color(metal * 1.08F);
  QVector3D const crest = config.crest_color.lengthSquared() > 0.01F
                              ? config.crest_color
                              : QVector3D(0.80F, 0.08F, 0.08F);
  return {metal, metal_dark, metal_light, crest};
}

} // namespace

void CarthageHeavyHelmetRenderer::render(const DrawContext &ctx,
                                         const BodyFrames &frames,
                                         const HumanoidPalette &palette,
                                         const HumanoidAnimationContext &anim,
                                         EquipmentBatch &batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void CarthageHeavyHelmetRenderer::submit(
    const CarthageHeavyHelmetConfig &config, const DrawContext &ctx,
    const BodyFrames &frames, const HumanoidPalette &palette,
    const HumanoidAnimationContext &anim, EquipmentBatch &batch) {
  (void)anim;

  if (frames.head.radius <= 0.0F) {
    return;
  }

  auto const equipment_palette = carthage_heavy_palette(palette, config);
  QVector3D const head_offset{0.0F, 0.05F, 0.0F};

  append_humanoid_attachment_archetype(batch, ctx, frames.head,
                                       carthage_heavy_shell_archetype(),
                                       equipment_palette, head_offset);
  if (config.has_neck_guard) {
    append_humanoid_attachment_archetype(batch, ctx, frames.head,
                                         carthage_heavy_neck_guard_archetype(),
                                         equipment_palette, head_offset);
  }
  if (config.has_cheek_guards) {
    append_humanoid_attachment_archetype(
        batch, ctx, frames.head, carthage_heavy_cheek_guards_archetype(),
        equipment_palette, head_offset);
  }
  if (config.has_face_plate) {
    append_humanoid_attachment_archetype(batch, ctx, frames.head,
                                         carthage_heavy_face_plate_archetype(),
                                         equipment_palette, head_offset);
  }
  if (config.has_hair_crest) {
    append_humanoid_attachment_archetype(batch, ctx, frames.head,
                                         carthage_heavy_crest_archetype(),
                                         equipment_palette, head_offset);
  }
  if (config.detail_level > 0) {
    append_humanoid_attachment_archetype(batch, ctx, frames.head,
                                         carthage_heavy_rivets_archetype(),
                                         equipment_palette, head_offset);
  }
}

} // namespace Render::GL
