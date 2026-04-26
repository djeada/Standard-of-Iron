#include "carthage_heavy_helmet.h"

#include "../attachment_builder.h"
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

constexpr QVector3D k_authored_local_offset{0.0F, 0.05F, 0.0F};

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

auto carthage_heavy_helmet_shell_archetype() -> const RenderArchetype & {
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

auto carthage_heavy_helmet_neck_guard_archetype() -> const RenderArchetype & {
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

auto carthage_heavy_helmet_cheek_guards_archetype() -> const RenderArchetype & {
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

auto carthage_heavy_helmet_face_plate_archetype() -> const RenderArchetype & {
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

auto carthage_heavy_helmet_crest_archetype() -> const RenderArchetype & {
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

auto carthage_heavy_helmet_rivets_archetype() -> const RenderArchetype & {
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

auto carthage_heavy_helmet_fill_role_colors(const HumanoidPalette &palette,
                                            QVector3D *out,
                                            std::size_t max) -> std::uint32_t {
  if (max < kCarthageHeavyHelmetRoleCount) {
    return 0;
  }
  auto const colors =
      carthage_heavy_palette(palette, CarthageHeavyHelmetConfig{});
  out[0] = colors[0];
  out[1] = colors[1];
  out[2] = colors[2];
  out[3] = colors[3];
  return kCarthageHeavyHelmetRoleCount;
}

auto carthage_heavy_helmet_make_static_attachment(
    const RenderArchetype &sub_archetype, std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte, const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr float kHeadSocketRadius = 0.16F;
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &sub_archetype,
      .socket_bone_index = socket_bone_index,
      .authored_local_offset = k_authored_local_offset,
      .bind_radius = kHeadSocketRadius,
      .bind_socket_transform = bind_palette_socket_bone,
  });
  spec.palette_role_remap[k_metal_slot] = base_role_byte;
  spec.palette_role_remap[k_metal_dark_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_metal_light_slot] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  spec.palette_role_remap[k_crest_slot] =
      static_cast<std::uint8_t>(base_role_byte + 3U);
  return spec;
}

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
                                       carthage_heavy_helmet_shell_archetype(),
                                       equipment_palette, head_offset);
  if (config.has_neck_guard) {
    append_humanoid_attachment_archetype(
        batch, ctx, frames.head, carthage_heavy_helmet_neck_guard_archetype(),
        equipment_palette, head_offset);
  }
  if (config.has_cheek_guards) {
    append_humanoid_attachment_archetype(
        batch, ctx, frames.head, carthage_heavy_helmet_cheek_guards_archetype(),
        equipment_palette, head_offset);
  }
  if (config.has_face_plate) {
    append_humanoid_attachment_archetype(
        batch, ctx, frames.head, carthage_heavy_helmet_face_plate_archetype(),
        equipment_palette, head_offset);
  }
  if (config.has_hair_crest) {
    append_humanoid_attachment_archetype(
        batch, ctx, frames.head, carthage_heavy_helmet_crest_archetype(),
        equipment_palette, head_offset);
  }
  if (config.detail_level > 0) {
    append_humanoid_attachment_archetype(
        batch, ctx, frames.head, carthage_heavy_helmet_rivets_archetype(),
        equipment_palette, head_offset);
  }
}

} // namespace Render::GL
