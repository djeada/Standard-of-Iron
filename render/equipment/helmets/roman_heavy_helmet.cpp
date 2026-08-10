#include "roman_heavy_helmet.h"

#include <array>

#include "helmet_alignment.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/equipment_archetype_helpers.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/humanoid_attachment_archetype.h"
#include "render/humanoid/style_palette.h"

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
    constexpr int k_total = 37;
    std::array<GeneratedEquipmentPrimitive, k_total> primitives{};

    primitives[0] = generated_ellipsoid(QVector3D(0.0F, 0.38F, -0.06F),
                                        QVector3D(1.50F, 1.36F, 1.62F),
                                        k_steel_slot,
                                        1.0F,
                                        2);
    primitives[1] = generated_ellipsoid(QVector3D(0.0F, 0.88F, -0.10F),
                                        QVector3D(1.16F, 0.92F, 1.26F),
                                        k_steel_light_slot,
                                        1.0F,
                                        2);

    primitives[2] = generated_ellipsoid(QVector3D(0.0F, -0.28F, 1.14F),
                                        QVector3D(1.16F, 0.13F, 0.38F),
                                        k_steel_light_slot,
                                        1.0F,
                                        2);
    primitives[3] = generated_ellipsoid(QVector3D(-0.72F, -0.30F, 1.28F),
                                        QVector3D(0.40F, 0.09F, 0.20F),
                                        k_brass_slot,
                                        1.0F,
                                        2);
    primitives[4] = generated_ellipsoid(QVector3D(0.72F, -0.30F, 1.28F),
                                        QVector3D(0.40F, 0.09F, 0.20F),
                                        k_brass_slot,
                                        1.0F,
                                        2);

    primitives[5] = generated_cylinder(QVector3D(-0.78F, -0.30F, 1.20F),
                                       QVector3D(-1.20F, -0.44F, 0.30F),
                                       0.11F,
                                       k_brass_slot,
                                       1.0F,
                                       2);
    primitives[6] = generated_cylinder(QVector3D(0.78F, -0.30F, 1.20F),
                                       QVector3D(1.20F, -0.44F, 0.30F),
                                       0.11F,
                                       k_brass_slot,
                                       1.0F,
                                       2);

    primitives[7] = generated_ellipsoid(QVector3D(-0.34F, 0.46F, 1.44F),
                                        QVector3D(0.36F, 0.12F, 0.20F),
                                        k_brass_slot,
                                        1.0F,
                                        2);
    primitives[8] = generated_ellipsoid(QVector3D(0.34F, 0.46F, 1.44F),
                                        QVector3D(0.36F, 0.12F, 0.20F),
                                        k_brass_slot,
                                        1.0F,
                                        2);
    primitives[9] = generated_ellipsoid(QVector3D(-0.88F, 0.32F, 1.24F),
                                        QVector3D(0.34F, 0.11F, 0.19F),
                                        k_brass_slot,
                                        1.0F,
                                        2);
    primitives[10] = generated_ellipsoid(QVector3D(0.88F, 0.32F, 1.24F),
                                         QVector3D(0.34F, 0.11F, 0.19F),
                                         k_brass_slot,
                                         1.0F,
                                         2);

    primitives[11] = generated_ellipsoid(QVector3D(0.0F, -0.44F, -1.34F),
                                         QVector3D(1.26F, 0.23F, 0.46F),
                                         k_steel_slot,
                                         1.0F,
                                         2);
    primitives[12] = generated_ellipsoid(QVector3D(0.0F, -0.72F, -1.60F),
                                         QVector3D(1.28F, 0.21F, 0.44F),
                                         k_steel_slot,
                                         1.0F,
                                         2);
    primitives[13] = generated_ellipsoid(QVector3D(0.0F, -0.98F, -1.78F),
                                         QVector3D(1.18F, 0.19F, 0.40F),
                                         k_steel_slot,
                                         1.0F,
                                         2);
    primitives[14] = generated_ellipsoid(QVector3D(0.0F, -1.14F, -1.84F),
                                         QVector3D(1.04F, 0.12F, 0.28F),
                                         k_brass_slot,
                                         1.0F,
                                         2);

    for (int side = 0; side < 2; ++side) {
      float const s = (side == 0) ? -1.0F : 1.0F;
      int const base = 15 + (side * 3);
      primitives[base] = generated_cylinder(QVector3D(s * 1.16F, -0.34F, 0.36F),
                                            QVector3D(s * 1.34F, -0.39F, 0.31F),
                                            0.64F,
                                            k_steel_slot,
                                            1.0F,
                                            2);
      primitives[base + 1] = generated_cylinder(QVector3D(s * 1.02F, -0.94F, 0.52F),
                                                QVector3D(s * 1.20F, -0.99F, 0.47F),
                                                0.50F,
                                                k_steel_slot,
                                                1.0F,
                                                2);
      primitives[base + 2] = generated_cylinder(QVector3D(s * 0.80F, -1.40F, 0.62F),
                                                QVector3D(s * 0.96F, -1.45F, 0.57F),
                                                0.30F,
                                                k_steel_light_slot,
                                                1.0F,
                                                2);
    }

    primitives[21] = generated_sphere(
        QVector3D(-1.40F, -0.20F, 0.30F), 0.16F, k_brass_slot, 1.0F, 2);
    primitives[22] =
        generated_sphere(QVector3D(1.40F, -0.20F, 0.30F), 0.16F, k_brass_slot, 1.0F, 2);
    primitives[23] = generated_sphere(
        QVector3D(-1.28F, -0.76F, 0.50F), 0.13F, k_brass_slot, 1.0F, 2);
    primitives[24] =
        generated_sphere(QVector3D(1.28F, -0.76F, 0.50F), 0.13F, k_brass_slot, 1.0F, 2);

    primitives[25] = generated_box(QVector3D(0.0F, 1.50F, -0.10F),
                                   QVector3D(0.14F, 0.30F, 1.10F),
                                   k_brass_slot,
                                   1.0F,
                                   2);
    primitives[26] = generated_ellipsoid(QVector3D(0.0F, 1.403F, 1.35F),
                                         QVector3D(0.20F, 0.62F, 0.52F),
                                         k_crest_slot,
                                         1.0F,
                                         0);
    primitives[27] = generated_ellipsoid(QVector3D(0.0F, 1.837F, 1.00F),
                                         QVector3D(0.20F, 0.62F, 0.52F),
                                         k_crest_slot,
                                         1.0F,
                                         0);
    primitives[28] = generated_ellipsoid(QVector3D(0.0F, 2.084F, 0.65F),
                                         QVector3D(0.20F, 0.62F, 0.52F),
                                         k_crest_slot,
                                         1.0F,
                                         0);
    primitives[29] = generated_ellipsoid(QVector3D(0.0F, 2.218F, 0.30F),
                                         QVector3D(0.20F, 0.62F, 0.52F),
                                         k_crest_slot,
                                         1.0F,
                                         0);
    primitives[30] = generated_ellipsoid(QVector3D(0.0F, 2.260F, -0.05F),
                                         QVector3D(0.20F, 0.62F, 0.52F),
                                         k_crest_slot,
                                         1.0F,
                                         0);
    primitives[31] = generated_ellipsoid(QVector3D(0.0F, 2.215F, -0.40F),
                                         QVector3D(0.20F, 0.62F, 0.52F),
                                         k_crest_slot,
                                         1.0F,
                                         0);
    primitives[32] = generated_ellipsoid(QVector3D(0.0F, 2.079F, -0.75F),
                                         QVector3D(0.20F, 0.62F, 0.52F),
                                         k_crest_slot,
                                         1.0F,
                                         0);
    primitives[33] = generated_ellipsoid(QVector3D(0.0F, 1.832F, -1.10F),
                                         QVector3D(0.20F, 0.62F, 0.52F),
                                         k_crest_slot,
                                         1.0F,
                                         0);
    primitives[34] = generated_ellipsoid(QVector3D(0.0F, 1.406F, -1.45F),
                                         QVector3D(0.20F, 0.62F, 0.52F),
                                         k_crest_slot,
                                         1.0F,
                                         0);
    primitives[35] = generated_ellipsoid(QVector3D(0.0F, 1.06F, 1.64F),
                                         QVector3D(0.19F, 0.52F, 0.36F),
                                         k_crest_slot,
                                         1.0F,
                                         0);
    primitives[36] = generated_ellipsoid(QVector3D(0.0F, 0.86F, -1.86F),
                                         QVector3D(0.18F, 0.58F, 0.40F),
                                         k_crest_slot,
                                         1.0F,
                                         0);

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
