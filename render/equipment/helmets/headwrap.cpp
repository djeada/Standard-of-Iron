#include "headwrap.h"

#include "../attachment_builder.h"
#include "../generated_equipment.h"
#include "../humanoid_attachment_archetype.h"

#include "../../humanoid/style_palette.h"

#include <array>

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

enum HeadwrapPaletteSlot : std::uint8_t {
  k_band_slot = 0U,
  k_knot_slot = 1U,
  k_tail_slot = 2U,
};

constexpr QVector3D k_authored_local_offset{0.0F, 0.0F, 0.0F};

auto headwrap_palette(const HumanoidPalette &palette)
    -> std::array<QVector3D, 3> {
  QVector3D const cloth =
      saturate_color(palette.cloth * QVector3D(0.9F, 1.05F, 1.05F));
  return {cloth, cloth * 1.05F, cloth * QVector3D(0.92F, 0.98F, 1.05F)};
}

} // namespace

auto headwrap_helmet_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 3> const primitives{{
        generated_cylinder(QVector3D(0.0F, 0.30F, 0.0F),
                           QVector3D(0.0F, 0.70F, 0.0F), 1.08F, k_band_slot),
        generated_sphere(QVector3D(0.10F, 0.60F, 0.72F), 0.32F, k_knot_slot),
        generated_cylinder(QVector3D(0.02F, 0.55F, 0.66F),
                           QVector3D(0.04F, 0.27F, 0.58F), 0.28F, k_tail_slot),
    }};
    return build_generated_equipment_archetype("headwrap", primitives);
  }();
  return archetype;
}

auto headwrap_fill_role_colors(const HumanoidPalette &palette, QVector3D *out,
                               std::size_t max) -> std::uint32_t {
  if (max < kHeadwrapRoleCount) {
    return 0;
  }
  auto const colors = headwrap_palette(palette);
  out[0] = colors[0];
  out[1] = colors[1];
  out[2] = colors[2];
  return kHeadwrapRoleCount;
}

auto headwrap_make_static_attachment(std::uint16_t socket_bone_index,
                                     std::uint8_t base_role_byte,
                                     const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr float kHeadSocketRadius = 0.16F;
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &headwrap_helmet_archetype(),
      .socket_bone_index = socket_bone_index,
      .authored_local_offset = k_authored_local_offset,
      .bind_radius = kHeadSocketRadius,
      .bind_socket_transform = bind_palette_socket_bone,
  });
  spec.palette_role_remap[k_band_slot] = base_role_byte;
  spec.palette_role_remap[k_knot_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_tail_slot] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  return spec;
}

void HeadwrapRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                              const HumanoidPalette &palette,
                              const HumanoidAnimationContext &anim,
                              EquipmentBatch &batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void HeadwrapRenderer::submit(const HeadwrapConfig &, const DrawContext &ctx,
                              const BodyFrames &frames,
                              const HumanoidPalette &palette,
                              const HumanoidAnimationContext &anim,
                              EquipmentBatch &batch) {
  (void)anim;

  if (frames.head.radius <= 0.0F) {
    return;
  }

  auto const equipment_palette = headwrap_palette(palette);
  append_humanoid_attachment_archetype(
      batch, ctx, frames.head, headwrap_helmet_archetype(), equipment_palette);
}

} // namespace Render::GL
