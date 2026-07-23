#include "roman_light_helmet.h"

#include <array>
#include <cmath>

#include "../../humanoid/style_palette.h"
#include "../attachment_builder.h"
#include "../generated_equipment.h"
#include "../humanoid_attachment_archetype.h"
#include "helmet_alignment.h"

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

enum RomanLightPaletteSlot : std::uint8_t {
  k_metal_slot = 0U,
  k_shadow_slot = 1U,
  k_crest_slot = 2U,
};

auto roman_light_palette(const HumanoidPalette& palette) -> std::array<QVector3D, 3> {
  QVector3D const steel =
      saturate_color(palette.metal * QVector3D(0.90F, 0.96F, 1.14F));
  QVector3D const shadow =
      saturate_color(steel * QVector3D(0.44F, 0.48F, 0.62F) +
                     palette.leather_dark * QVector3D(0.18F, 0.14F, 0.10F));
  QVector3D const crest = saturate_color(
      QVector3D(0.18F, 0.01F, 0.03F) + palette.cloth * QVector3D(0.48F, 0.10F, 0.10F));
  return {steel, shadow, crest};
}

} // namespace

auto roman_light_helmet_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    constexpr int k_crest_fiber_count = 8;
    constexpr int k_total = 20 + k_crest_fiber_count;
    std::array<GeneratedEquipmentPrimitive, k_total> primitives{};

    primitives[0] = generated_ellipsoid(QVector3D(0.0F, 0.58F, -0.08F),
                                        QVector3D(1.42F, 1.18F, 1.50F),
                                        k_metal_slot,
                                        1.0F,
                                        2);

    primitives[1] = generated_cylinder(QVector3D(0.0F, -0.22F, 0.0F),
                                       QVector3D(0.0F, -0.04F, 0.0F),
                                       1.34F,
                                       k_shadow_slot,
                                       1.0F,
                                       2);

    primitives[2] = generated_box(QVector3D(0.0F, 0.46F, 0.90F),
                                  QVector3D(1.02F, 0.14F, 0.16F),
                                  k_shadow_slot,
                                  1.0F,
                                  2);

    primitives[3] = generated_box(QVector3D(0.0F, 0.14F, 0.88F),
                                  QVector3D(0.26F, 0.46F, 0.10F),
                                  k_shadow_slot,
                                  1.0F,
                                  2);

    primitives[4] = generated_cylinder(QVector3D(-1.02F, 0.54F, 0.72F),
                                       QVector3D(1.02F, 0.54F, 0.72F),
                                       0.08F,
                                       k_metal_slot,
                                       1.0F,
                                       2);

    primitives[5] = generated_cone(QVector3D(-0.78F, 1.34F, 0.12F),
                                   QVector3D(-1.36F, 1.06F, -0.44F),
                                   0.20F,
                                   k_shadow_slot,
                                   1.0F,
                                   2);

    primitives[6] = generated_cone(QVector3D(0.78F, 1.34F, 0.12F),
                                   QVector3D(1.36F, 1.06F, -0.44F),
                                   0.20F,
                                   k_shadow_slot,
                                   1.0F,
                                   2);

    primitives[7] = generated_cylinder(QVector3D(-1.16F, 0.10F, 0.22F),
                                       QVector3D(-1.28F, -0.34F, 0.34F),
                                       0.18F,
                                       k_shadow_slot,
                                       1.0F,
                                       2);

    primitives[8] = generated_cylinder(QVector3D(-1.28F, -0.34F, 0.34F),
                                       QVector3D(-0.98F, -0.62F, 0.48F),
                                       0.15F,
                                       k_shadow_slot,
                                       1.0F,
                                       2);

    primitives[9] = generated_cylinder(QVector3D(1.16F, 0.10F, 0.22F),
                                       QVector3D(1.28F, -0.34F, 0.34F),
                                       0.18F,
                                       k_shadow_slot,
                                       1.0F,
                                       2);

    primitives[10] = generated_cylinder(QVector3D(1.28F, -0.34F, 0.34F),
                                        QVector3D(0.98F, -0.62F, 0.48F),
                                        0.15F,
                                        k_shadow_slot,
                                        1.0F,
                                        2);

    primitives[11] = generated_cylinder(QVector3D(0.0F, -0.24F, -0.82F),
                                        QVector3D(0.0F, -0.10F, -1.14F),
                                        1.30F,
                                        k_shadow_slot,
                                        1.0F,
                                        2);

    primitives[12] = generated_cylinder(QVector3D(0.0F, -0.08F, -1.10F),
                                        QVector3D(0.0F, -0.04F, -1.16F),
                                        1.34F,
                                        k_metal_slot,
                                        1.0F,
                                        2);

    QVector3D const crest_front{0.0F, 1.78F, 0.52F};
    QVector3D const crest_back{0.0F, 1.62F, -0.70F};

    primitives[13] =
        generated_cylinder(crest_front, crest_back, 0.04F, k_metal_slot, 1.0F, 2);
    primitives[14] = generated_sphere(crest_front, 0.06F, k_metal_slot, 1.0F, 2);
    primitives[15] = generated_sphere(crest_back, 0.05F, k_metal_slot, 1.0F, 2);

    primitives[16] = generated_cylinder(QVector3D(-0.08F, 0.78F, 0.98F),
                                        QVector3D(-0.82F, 0.66F, 0.72F),
                                        0.065F,
                                        k_metal_slot,
                                        1.0F,
                                        2);
    primitives[17] = generated_cylinder(QVector3D(0.08F, 0.78F, 0.98F),
                                        QVector3D(0.82F, 0.66F, 0.72F),
                                        0.065F,
                                        k_metal_slot,
                                        1.0F,
                                        2);
    primitives[18] =
        generated_sphere(QVector3D(-1.22F, 0.02F, 0.04F), 0.14F, k_metal_slot, 1.0F, 2);
    primitives[19] =
        generated_sphere(QVector3D(1.22F, 0.02F, 0.04F), 0.14F, k_metal_slot, 1.0F, 2);

    for (int i = 0; i < k_crest_fiber_count; ++i) {
      float const t =
          static_cast<float>(i) / static_cast<float>(k_crest_fiber_count - 1);
      QVector3D const base =
          crest_front * (1.0F - t) + crest_back * t + QVector3D(0.0F, 0.03F, 0.0F);
      float const lateral = (i % 3 == 1) ? 0.03F : (i % 3 == 2) ? -0.03F : 0.0F;
      float const lift = 0.52F - 0.16F * std::abs(t - 0.35F);
      float const trail = 0.08F + 0.18F * t;
      QVector3D const tip = base + QVector3D(lateral, lift, -trail);
      primitives[20 + i] = generated_cylinder(base, tip, 0.05F, k_crest_slot, 0.92F, 0);
    }

    return build_generated_equipment_archetype("roman_light_helmet", primitives);
  }();
  return archetype;
}

auto roman_light_helmet_fill_role_colors(const HumanoidPalette& palette,
                                         QVector3D* out,
                                         std::size_t max) -> std::uint32_t {
  if (max < k_roman_light_helmet_role_count) {
    return 0;
  }
  auto const colors = roman_light_palette(palette);
  out[0] = colors[0];
  out[1] = colors[1];
  out[2] = colors[2];
  return k_roman_light_helmet_role_count;
}

auto roman_light_helmet_make_static_attachment(
    std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte,
    const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {

  constexpr float k_head_socket_radius = 0.16F;
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &roman_light_helmet_archetype(),
      .socket_bone_index = socket_bone_index,
      .uniform_scale = k_helmet_uniform_scale,
      .authored_local_offset = k_helmet_local_offset * k_helmet_uniform_scale,
      .bind_radius = k_head_socket_radius,
      .bind_socket_transform = bind_palette_socket_bone,
  });
  spec.palette_role_remap[k_metal_slot] = base_role_byte;
  spec.palette_role_remap[k_shadow_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_crest_slot] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  return spec;
}

void RomanLightHelmetRenderer::render(const DrawContext& ctx,
                                      const BodyFrames& frames,
                                      const HumanoidPalette& palette,
                                      const HumanoidAnimationContext& anim,
                                      EquipmentBatch& batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void RomanLightHelmetRenderer::submit(const RomanLightHelmetConfig&,
                                      const DrawContext& ctx,
                                      const BodyFrames& frames,
                                      const HumanoidPalette& palette,
                                      const HumanoidAnimationContext& anim,
                                      EquipmentBatch& batch) {
  (void)anim;

  if (frames.head.radius <= 0.0F) {
    return;
  }

  auto const colors = roman_light_palette(palette);
  append_humanoid_attachment_archetype(batch,
                                       ctx,
                                       frames.head,
                                       roman_light_helmet_archetype(),
                                       colors,
                                       k_helmet_local_offset,
                                       k_helmet_uniform_scale);
}

} // namespace Render::GL
