#include "roman_heavy_helmet.h"

#include <array>
#include <cmath>

#include "../../humanoid/style_palette.h"
#include "../attachment_builder.h"
#include "../equipment_archetype_helpers.h"
#include "../generated_equipment.h"
#include "../humanoid_attachment_archetype.h"
#include "helmet_alignment.h"

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

enum RomanHeavyPaletteSlot : std::uint8_t {
  k_steel_slot = 0U,
  k_steel_light_slot = 1U,
  k_brass_slot = 2U,
  k_crest_slot = 3U,
};

auto roman_heavy_palette(const HumanoidPalette& palette) -> std::array<QVector3D, 4> {
  QVector3D const steel =
      saturate_color(palette.metal * QVector3D(0.88F, 0.92F, 1.08F));
  QVector3D const steel_light = saturate_color(steel * 1.06F);
  QVector3D const brass =
      saturate_color(palette.metal * QVector3D(1.40F, 1.15F, 0.65F));
  return {steel, steel_light, brass, QVector3D(0.96F, 0.12F, 0.12F)};
}

} // namespace

auto roman_heavy_helmet_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    constexpr int k_fiber_count = 10;
    constexpr int k_total = 18 + k_fiber_count;
    std::array<GeneratedEquipmentPrimitive, k_total> primitives{};

    primitives[0] = generated_ellipsoid(QVector3D(0.0F, 0.54F, -0.10F),
                                        QVector3D(1.34F, 1.18F, 1.48F),
                                        k_steel_slot,
                                        1.0F,
                                        2);

    primitives[1] = generated_cone(QVector3D(0.0F, 1.34F, -0.08F),
                                   QVector3D(0.0F, 1.82F, -0.10F),
                                   0.76F,
                                   k_steel_light_slot,
                                   1.0F,
                                   2);

    primitives[2] = generated_cylinder(QVector3D(0.0F, 0.05F, 0.0F),
                                       QVector3D(0.0F, 0.13F, 0.0F),
                                       1.22F,
                                       k_brass_slot,
                                       1.0F,
                                       2);

    primitives[3] = generated_cylinder(QVector3D(0.0F, 1.33F, 0.0F),
                                       QVector3D(0.0F, 1.41F, 0.0F),
                                       1.15F,
                                       k_brass_slot,
                                       1.0F,
                                       2);

    primitives[4] = generated_cylinder(QVector3D(0.0F, -0.48F, -0.78F),
                                       QVector3D(0.0F, -0.18F, -1.08F),
                                       1.44F,
                                       k_steel_slot,
                                       1.0F,
                                       2);

    primitives[5] = generated_cylinder(QVector3D(0.0F, -0.20F, -1.10F),
                                       QVector3D(0.0F, -0.16F, -1.14F),
                                       1.46F,
                                       k_brass_slot,
                                       1.0F,
                                       2);

    primitives[6] = generated_cylinder(QVector3D(-1.14F, 0.44F, 0.82F),
                                       QVector3D(1.14F, 0.44F, 0.82F),
                                       0.10F,
                                       k_steel_light_slot,
                                       1.0F,
                                       2);

    primitives[7] = generated_cylinder(QVector3D(-1.14F, 0.06F, 0.14F),
                                       QVector3D(-0.88F, -0.52F, 0.26F),
                                       0.22F,
                                       k_steel_slot,
                                       1.0F,
                                       2);

    primitives[8] = generated_cylinder(QVector3D(1.14F, 0.06F, 0.14F),
                                       QVector3D(0.88F, -0.52F, 0.26F),
                                       0.22F,
                                       k_steel_slot,
                                       1.0F,
                                       2);

    // Reinforcing brows, ear bosses and articulated cheek flares are the
    // characteristic Imperial Roman face of the heavy helmet.
    primitives[9] = generated_cylinder(QVector3D(-0.08F, 0.78F, 1.00F),
                                       QVector3D(-0.86F, 0.66F, 0.72F),
                                       0.075F,
                                       k_brass_slot,
                                       1.0F,
                                       2);
    primitives[10] = generated_cylinder(QVector3D(0.08F, 0.78F, 1.00F),
                                        QVector3D(0.86F, 0.66F, 0.72F),
                                        0.075F,
                                        k_brass_slot,
                                        1.0F,
                                        2);
    primitives[11] =
        generated_sphere(QVector3D(-1.16F, 0.00F, 0.06F), 0.16F, k_brass_slot, 1.0F, 2);
    primitives[12] =
        generated_sphere(QVector3D(1.16F, 0.00F, 0.06F), 0.16F, k_brass_slot, 1.0F, 2);
    primitives[13] = generated_cylinder(QVector3D(-0.88F, -0.52F, 0.26F),
                                        QVector3D(-0.66F, -0.76F, 0.50F),
                                        0.18F,
                                        k_steel_light_slot,
                                        1.0F,
                                        2);
    primitives[14] = generated_cylinder(QVector3D(0.88F, -0.52F, 0.26F),
                                        QVector3D(0.66F, -0.76F, 0.50F),
                                        0.18F,
                                        k_steel_light_slot,
                                        1.0F,
                                        2);

    QVector3D const crest_front{0.0F, 1.90F, 0.52F};
    QVector3D const crest_back{0.0F, 1.82F, -0.64F};

    primitives[15] =
        generated_cylinder(crest_front, crest_back, 0.036F, k_brass_slot, 1.0F, 2);

    primitives[16] = generated_sphere(crest_front, 0.058F, k_brass_slot, 1.0F, 2);

    primitives[17] = generated_sphere(crest_back, 0.048F, k_brass_slot, 1.0F, 2);

    for (int i = 0; i < k_fiber_count; ++i) {
      float const t = static_cast<float>(i) / static_cast<float>(k_fiber_count - 1);

      QVector3D const base =
          crest_front * (1.0F - t) + crest_back * t + QVector3D(0.0F, 0.025F, 0.0F);

      float const dx = (t - 0.20F) / 0.40F;
      float const lift = 0.70F * std::exp(-dx * dx) + 0.15F * (1.0F - t) + 0.05F;

      float const lateral = (i % 3 == 1) ? 0.022F : (i % 3 == 2) ? -0.022F : 0.0F;

      QVector3D const tip = base + QVector3D(lateral, lift, 0.0F);
      primitives[18 + i] = generated_cylinder(base, tip, 0.052F, k_crest_slot, 1.0F, 0);
    }

    return build_generated_equipment_archetype("roman_heavy_helmet", primitives);
  }();
  return archetype;
}

auto roman_heavy_helmet_fill_role_colors(const HumanoidPalette& palette,
                                         QVector3D* out,
                                         std::size_t max) -> std::uint32_t {
  return fill_role_colors(roman_heavy_palette(palette), out, max);
}

auto roman_heavy_helmet_make_static_attachment(
    std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte,
    const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr float k_head_socket_radius = 0.16F;
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &roman_heavy_helmet_archetype(),
      .socket_bone_index = socket_bone_index,
      .uniform_scale = k_helmet_uniform_scale,
      .authored_local_offset = k_helmet_local_offset * k_helmet_uniform_scale,
      .bind_radius = k_head_socket_radius,
      .bind_socket_transform = bind_palette_socket_bone,
  });
  fill_sequential_role_remap(spec, base_role_byte, k_roman_heavy_helmet_role_count);
  return spec;
}

void RomanHeavyHelmetRenderer::render(const DrawContext& ctx,
                                      const BodyFrames& frames,
                                      const HumanoidPalette& palette,
                                      const HumanoidAnimationContext& anim,
                                      EquipmentBatch& batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void RomanHeavyHelmetRenderer::submit(const RomanHeavyHelmetConfig&,
                                      const DrawContext& ctx,
                                      const BodyFrames& frames,
                                      const HumanoidPalette& palette,
                                      const HumanoidAnimationContext& anim,
                                      EquipmentBatch& batch) {
  (void)anim;

  if (frames.head.radius <= 0.0F) {
    return;
  }

  auto const colors = roman_heavy_palette(palette);
  append_humanoid_attachment_archetype(batch,
                                       ctx,
                                       frames.head,
                                       roman_heavy_helmet_archetype(),
                                       colors,
                                       k_helmet_local_offset,
                                       k_helmet_uniform_scale);
}

} // namespace Render::GL
