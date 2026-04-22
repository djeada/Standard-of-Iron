#include "carthage_light_helmet.h"

#include "../generated_equipment.h"
#include "../humanoid_attachment_archetype.h"

#include "../../humanoid/style_palette.h"

#include <array>

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

enum CarthageLightPaletteSlot : std::uint8_t {
  k_metal_slot = 0U,
  k_metal_dark_slot = 1U,
  k_leather_slot = 2U,
  k_crest_slot = 3U,
};

auto carthage_light_shell_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 5> const primitives{{
        generated_sphere(QVector3D(0.0F, 0.58F, 0.0F), 1.55F, k_metal_slot,
                         1.0F, 2),
        generated_cylinder(QVector3D(0.0F, -0.38F, 0.0F),
                           QVector3D(0.0F, -0.05F, 0.0F), 1.64F,
                           k_metal_dark_slot, 1.0F, 2),
        generated_cylinder(QVector3D(0.0F, -0.20F, 0.0F),
                           QVector3D(0.0F, -0.02F, 0.0F), 1.16F, k_leather_slot,
                           1.0F, 2),
        generated_cone(QVector3D(0.0F, 2.13F, 0.0F),
                       QVector3D(0.0F, 3.76F, 0.47F), 0.465F, k_metal_slot,
                       1.0F, 2),
        generated_sphere(QVector3D(0.0F, 3.76F, 0.47F), 0.155F, k_metal_slot,
                         1.0F, 2),
    }};
    return build_generated_equipment_archetype("carthage_light_shell",
                                               primitives);
  }();
  return archetype;
}

auto carthage_light_neck_guard_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
        generated_cylinder(QVector3D(0.0F, -0.30F, -0.96F),
                           QVector3D(0.0F, -0.12F, -1.00F), 1.36F,
                           k_metal_dark_slot, 1.0F, 2),
    }};
    return build_generated_equipment_archetype("carthage_light_neck_guard",
                                               primitives);
  }();
  return archetype;
}

auto carthage_light_nasal_guard_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
        generated_cylinder(QVector3D(0.0F, -0.08F, 0.90F),
                           QVector3D(0.0F, 0.56F, 0.72F), 0.09F, k_metal_slot,
                           1.0F, 2),
    }};
    return build_generated_equipment_archetype("carthage_light_nasal_guard",
                                               primitives);
  }();
  return archetype;
}

auto carthage_light_cheek_guards_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 4> const primitives{{
        generated_cylinder(QVector3D(-1.44F, 0.06F, 0.20F),
                           QVector3D(-1.48F, -0.18F, 0.28F), 0.19F,
                           k_metal_dark_slot, 1.0F, 2),
        generated_cylinder(QVector3D(-1.48F, -0.18F, 0.28F),
                           QVector3D(-1.28F, -0.38F, 0.36F), 0.16F,
                           k_metal_dark_slot, 1.0F, 2),
        generated_cylinder(QVector3D(1.44F, 0.06F, 0.20F),
                           QVector3D(1.48F, -0.18F, 0.28F), 0.19F,
                           k_metal_dark_slot, 1.0F, 2),
        generated_cylinder(QVector3D(1.48F, -0.18F, 0.28F),
                           QVector3D(1.28F, -0.38F, 0.36F), 0.16F,
                           k_metal_dark_slot, 1.0F, 2),
    }};
    return build_generated_equipment_archetype("carthage_light_cheek_guards",
                                               primitives);
  }();
  return archetype;
}

auto carthage_light_crest_archetype(bool high_detail)
    -> const RenderArchetype & {
  static const RenderArchetype low_detail = [] {
    std::array<GeneratedEquipmentPrimitive, 9> primitives{};
    QVector3D const crest_front{0.0F, 2.22F, 0.08F};
    QVector3D const crest_back{0.0F, 2.06F, -0.24F};
    primitives[0] = generated_cylinder(crest_front, crest_back, 0.044F,
                                       k_metal_slot, 1.0F, 2);

    for (int i = 0; i < 8; ++i) {
      float const t = (i == 0) ? 0.0F : static_cast<float>(i) / 7.0F;
      QVector3D const base = crest_front * (1.0F - t) + crest_back * t +
                             QVector3D(0.0F, 0.03F, 0.0F);
      float const lateral = (static_cast<float>(i % 3) - 1.0F) * 0.022F;
      float const lift = 0.44F - 0.12F * std::abs(t - 0.5F);
      float const droop = 0.10F + 0.20F * t;
      QVector3D const tip = base + QVector3D(lateral, lift, -droop);
      primitives[1 + i] =
          generated_cylinder(base, tip, 0.048F, k_crest_slot, 0.90F, 0);
    }

    return build_generated_equipment_archetype("carthage_light_crest_low",
                                               primitives);
  }();

  static const RenderArchetype high_detail_archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 15> primitives{};
    QVector3D const crest_front{0.0F, 2.22F, 0.08F};
    QVector3D const crest_back{0.0F, 2.06F, -0.24F};
    primitives[0] = generated_cylinder(crest_front, crest_back, 0.044F,
                                       k_metal_slot, 1.0F, 2);

    for (int i = 0; i < 14; ++i) {
      float const t = (i == 0) ? 0.0F : static_cast<float>(i) / 13.0F;
      QVector3D const base = crest_front * (1.0F - t) + crest_back * t +
                             QVector3D(0.0F, 0.03F, 0.0F);
      float const lateral = (static_cast<float>(i % 3) - 1.0F) * 0.022F;
      float const lift = 0.44F - 0.12F * std::abs(t - 0.5F);
      float const droop = 0.10F + 0.20F * t;
      QVector3D const tip = base + QVector3D(lateral, lift, -droop);
      primitives[1 + i] =
          generated_cylinder(base, tip, 0.048F, k_crest_slot, 0.90F, 0);
    }

    return build_generated_equipment_archetype("carthage_light_crest_high",
                                               primitives);
  }();

  return high_detail ? high_detail_archetype : low_detail;
}

auto carthage_light_rivets_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 5> const primitives{{
        generated_sphere(QVector3D(-0.50F, 0.46F, 0.64F), 0.05F, k_metal_slot,
                         1.0F, 2),
        generated_sphere(QVector3D(-0.25F, 0.46F, 0.64F), 0.05F, k_metal_slot,
                         1.0F, 2),
        generated_sphere(QVector3D(0.0F, 0.46F, 0.64F), 0.05F, k_metal_slot,
                         1.0F, 2),
        generated_sphere(QVector3D(0.25F, 0.46F, 0.64F), 0.05F, k_metal_slot,
                         1.0F, 2),
        generated_sphere(QVector3D(0.50F, 0.46F, 0.64F), 0.05F, k_metal_slot,
                         1.0F, 2),
    }};
    return build_generated_equipment_archetype("carthage_light_rivets",
                                               primitives);
  }();
  return archetype;
}

auto carthage_light_palette(const HumanoidPalette &palette)
    -> std::array<QVector3D, 4> {
  QVector3D const metal =
      saturate_color(palette.metal * QVector3D(1.32F, 1.08F, 0.62F));
  QVector3D const metal_dark = metal * 0.78F;
  QVector3D const leather =
      saturate_color(palette.leather_dark * QVector3D(0.96F, 0.88F, 0.78F));
  return {metal, metal_dark, leather, QVector3D(0.78F, 0.08F, 0.08F)};
}

} // namespace

void CarthageLightHelmetRenderer::render(const DrawContext &ctx,
                                         const BodyFrames &frames,
                                         const HumanoidPalette &palette,
                                         const HumanoidAnimationContext &anim,
                                         EquipmentBatch &batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void CarthageLightHelmetRenderer::submit(
    const CarthageLightHelmetConfig &config, const DrawContext &ctx,
    const BodyFrames &frames, const HumanoidPalette &palette,
    const HumanoidAnimationContext &anim, EquipmentBatch &batch) {
  (void)anim;

  if (frames.head.radius <= 0.0F) {
    return;
  }

  auto const equipment_palette = carthage_light_palette(palette);
  QVector3D const head_offset{0.0F, 0.05F, 0.0F};

  append_humanoid_attachment_archetype(batch, ctx, frames.head,
                                       carthage_light_shell_archetype(),
                                       equipment_palette, head_offset);
  append_humanoid_attachment_archetype(batch, ctx, frames.head,
                                       carthage_light_neck_guard_archetype(),
                                       equipment_palette, head_offset);
  append_humanoid_attachment_archetype(batch, ctx, frames.head,
                                       carthage_light_cheek_guards_archetype(),
                                       equipment_palette, head_offset);

  if (config.has_nasal_guard) {
    append_humanoid_attachment_archetype(batch, ctx, frames.head,
                                         carthage_light_nasal_guard_archetype(),
                                         equipment_palette, head_offset);
  }
  if (config.has_crest) {
    append_humanoid_attachment_archetype(
        batch, ctx, frames.head,
        carthage_light_crest_archetype(config.detail_level >= 2),
        equipment_palette, head_offset);
  }
  if (config.detail_level > 0) {
    append_humanoid_attachment_archetype(batch, ctx, frames.head,
                                         carthage_light_rivets_archetype(),
                                         equipment_palette, head_offset);
  }
}

} // namespace Render::GL
