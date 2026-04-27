#include "saddle_bag_renderer.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include "../../../geom/transforms.h"
#include "../../../gl/primitives.h"
#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

enum SaddleBagPaletteSlot : std::uint8_t {
  k_bag_slot = 0U,
  k_strap_slot = 1U,
};

}

auto saddle_bag_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_saddle_bags");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.28F, -0.12F, -0.15F),
                                             QVector3D(0.18F, 0.22F, 0.30F)),
                             k_bag_slot, nullptr, 1.0F, 4);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(-0.28F, -0.12F, -0.15F),
                                             QVector3D(0.18F, 0.22F, 0.30F)),
                             k_bag_slot, nullptr, 1.0F, 4);
    for (float side : {1.0F, -1.0F}) {
      QVector3D const strap_top(side * 0.28F, 0.02F, -0.10F);
      QVector3D const strap_bottom(side * 0.28F, -0.12F, -0.15F);
      builder.add_palette_mesh(
          get_unit_cylinder(),
          Render::Geom::cylinder_between(strap_top, strap_bottom, 0.012F),
          k_strap_slot, nullptr, 1.0F, 4);
    }
    return std::move(builder).build();
  }();
  return archetype;
}

auto saddle_bag_fill_role_colors(const HorseVariant &variant, QVector3D *out,
                                 std::size_t max) -> std::uint32_t {
  if (max < kSaddleBagRoleCount) {
    return 0;
  }
  out[k_bag_slot] = variant.saddle_color * 0.85F;
  out[k_strap_slot] = variant.tack_color;
  return kSaddleBagRoleCount;
}

auto saddle_bag_make_static_attachment(
    std::uint16_t socket_bone_index, std::uint8_t base_role_byte,
    const HorseAttachmentFrame &bind_pose_frame,
    const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = make_static_attachment_spec_from_horse_renderer(
      saddle_bag_archetype(), socket_bone_index, bind_pose_frame,
      bind_palette_socket_bone);
  spec.palette_role_remap[k_bag_slot] =
      static_cast<std::uint8_t>(base_role_byte + k_bag_slot);
  spec.palette_role_remap[k_strap_slot] =
      static_cast<std::uint8_t>(base_role_byte + k_strap_slot);
  return spec;
}

void SaddleBagRenderer::submit(const DrawContext &ctx,
                               const HorseBodyFrames &frames,
                               const HorseVariant &variant,
                               const HorseAnimationContext &,
                               EquipmentBatch &batch) {
  QVector3D const bag_color = variant.saddle_color * 0.85F;
  std::array<QVector3D, 2> const palette{bag_color, variant.tack_color};
  append_horse_attachment_archetype(batch, ctx, frames.back_center,
                                    saddle_bag_archetype(), palette);
}

} // namespace Render::GL
