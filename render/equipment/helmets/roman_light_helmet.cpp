#include "roman_light_helmet.h"

#include <array>
#include <cstddef>
#include <utility>

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
      saturate_color(palette.metal * QVector3D(1.18F, 1.02F, 0.72F));
  QVector3D const shadow =
      saturate_color(steel * QVector3D(0.44F, 0.48F, 0.62F) +
                     palette.leather_dark * QVector3D(0.18F, 0.14F, 0.10F));
  QVector3D const crest = saturate_color(
      QVector3D(0.62F, 0.06F, 0.05F) + palette.cloth * QVector3D(0.34F, 0.06F, 0.06F));
  return {steel, shadow, crest};
}

} // namespace

auto roman_light_helmet_archetype() -> const RenderArchetype& {
  // Montefortino: the bowl the Republic issued to everyone who was not in the
  // heavy line. Same Roman language as the Gallic galea worn by the swordsmen --
  // brow peak, hinged cheek plates, flared neck guard -- but distinguished by
  // the cast top knob and its single upright plume instead of a fore-aft crest.
  static const RenderArchetype archetype = [] {
    constexpr int k_total = 29;
    std::array<GeneratedEquipmentPrimitive, k_total> primitives{};

    primitives[0] = generated_ellipsoid(QVector3D(0.0F, 0.34F, -0.06F),
                                        QVector3D(1.46F, 1.34F, 1.58F),
                                        k_metal_slot,
                                        1.0F,
                                        2);
    primitives[1] = generated_ellipsoid(QVector3D(0.0F, 0.82F, -0.08F),
                                        QVector3D(1.12F, 0.86F, 1.20F),
                                        k_metal_slot,
                                        1.0F,
                                        2);

    // Cast top knob, the feature that names the type.
    primitives[2] = generated_cylinder(QVector3D(0.0F, 1.52F, -0.08F),
                                       QVector3D(0.0F, 1.80F, -0.08F),
                                       0.19F,
                                       k_shadow_slot,
                                       1.0F,
                                       2);
    primitives[3] =
        generated_sphere(QVector3D(0.0F, 1.92F, -0.08F), 0.28F, k_metal_slot, 1.0F, 2);

    // Brow peak, held shallow enough that the nose comes out under it.
    primitives[4] = generated_ellipsoid(QVector3D(0.0F, -0.28F, 1.10F),
                                        QVector3D(1.10F, 0.12F, 0.36F),
                                        k_metal_slot,
                                        1.0F,
                                        2);

    // Rim reinforce sweeping back from the peak to the cheek hinges.
    primitives[5] = generated_cylinder(QVector3D(-0.74F, -0.30F, 1.16F),
                                       QVector3D(-1.16F, -0.44F, 0.28F),
                                       0.10F,
                                       k_shadow_slot,
                                       1.0F,
                                       2);
    primitives[6] = generated_cylinder(QVector3D(0.74F, -0.30F, 1.16F),
                                       QVector3D(1.16F, -0.44F, 0.28F),
                                       0.10F,
                                       k_shadow_slot,
                                       1.0F,
                                       2);

    // Neck guard: three flattened plates stepping back and down off the nape,
    // plus a rolled rim. A cone was tried here first and its base cap read as an
    // upturned bowl stuck to the back of the head.
    primitives[7] = generated_ellipsoid(QVector3D(0.0F, -0.42F, -1.28F),
                                        QVector3D(1.18F, 0.21F, 0.42F),
                                        k_metal_slot,
                                        1.0F,
                                        2);
    primitives[8] = generated_ellipsoid(QVector3D(0.0F, -0.68F, -1.52F),
                                        QVector3D(1.20F, 0.19F, 0.40F),
                                        k_metal_slot,
                                        1.0F,
                                        2);
    primitives[9] = generated_ellipsoid(QVector3D(0.0F, -0.92F, -1.68F),
                                        QVector3D(1.10F, 0.17F, 0.36F),
                                        k_metal_slot,
                                        1.0F,
                                        2);
    primitives[10] = generated_ellipsoid(QVector3D(0.0F, -1.06F, -1.74F),
                                         QVector3D(0.96F, 0.10F, 0.24F),
                                         k_shadow_slot,
                                         1.0F,
                                         2);

    // Cheek guards, a size down from the heavy galea so the two helmets stay
    // apart in silhouette.
    for (int side = 0; side < 2; ++side) {
      float const s = (side == 0) ? -1.0F : 1.0F;
      int const base = 11 + (side * 3);
      primitives[base] = generated_cylinder(QVector3D(s * 1.12F, -0.36F, 0.34F),
                                            QVector3D(s * 1.30F, -0.41F, 0.29F),
                                            0.58F,
                                            k_metal_slot,
                                            1.0F,
                                            2);
      primitives[base + 1] = generated_cylinder(QVector3D(s * 0.98F, -0.98F, 0.48F),
                                                QVector3D(s * 1.14F, -1.03F, 0.43F),
                                                0.44F,
                                                k_metal_slot,
                                                1.0F,
                                                2);
      primitives[base + 2] = generated_cylinder(QVector3D(s * 0.78F, -1.42F, 0.58F),
                                                QVector3D(s * 0.92F, -1.47F, 0.53F),
                                                0.26F,
                                                k_shadow_slot,
                                                1.0F,
                                                2);
    }

    primitives[17] = generated_sphere(
        QVector3D(-1.34F, -0.22F, 0.28F), 0.14F, k_metal_slot, 1.0F, 2);
    primitives[18] =
        generated_sphere(QVector3D(1.34F, -0.22F, 0.28F), 0.14F, k_metal_slot, 1.0F, 2);
    primitives[19] = generated_sphere(
        QVector3D(-1.22F, -0.72F, 0.46F), 0.12F, k_shadow_slot, 1.0F, 2);
    primitives[20] = generated_sphere(
        QVector3D(1.22F, -0.72F, 0.46F), 0.12F, k_shadow_slot, 1.0F, 2);
    primitives[21] = generated_sphere(
        QVector3D(-0.76F, -0.56F, -1.42F), 0.11F, k_shadow_slot, 1.0F, 2);
    primitives[22] = generated_sphere(
        QVector3D(0.76F, -0.56F, -1.42F), 0.11F, k_shadow_slot, 1.0F, 2);

    // Plume: a tapering horsehair brush stacked straight off the knob. Splayed
    // strands read as a tulip at close range, so the tuft is built as mass that
    // swells above the crown and comes back to a point.
    primitives[23] = generated_cylinder(QVector3D(0.0F, 2.02F, -0.08F),
                                        QVector3D(0.0F, 2.22F, -0.08F),
                                        0.15F,
                                        k_shadow_slot,
                                        1.0F,
                                        2);
    constexpr std::array<std::pair<float, float>, 5> k_plume_lobes{{
        {2.20F, 0.28F},
        {2.54F, 0.40F},
        {2.90F, 0.44F},
        {3.24F, 0.34F},
        {3.48F, 0.18F},
    }};
    for (std::size_t i = 0; i < k_plume_lobes.size(); ++i) {
      auto const [height, radius] = k_plume_lobes[i];
      primitives[24 + i] =
          generated_ellipsoid(QVector3D(0.0F, height, -0.08F),
                              QVector3D(radius, radius * 1.05F, radius),
                              k_crest_slot,
                              1.0F,
                              0);
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
