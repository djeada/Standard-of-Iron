#include "crupper_renderer.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include "../../../gl/primitives.h"
#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

enum CrupperPaletteSlot : std::uint8_t {
  k_armor_slot = 0U,
  k_flap_slot = 1U,
};

}

auto crupper_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_crupper_barding");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, 0.02F, -0.15F),
                                             QVector3D(0.48F, 0.32F, 0.28F)),
                             k_armor_slot, nullptr, 1.0F, 1);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.28F, -0.05F, -0.20F),
                                             QVector3D(0.20F, 0.25F, 0.22F)),
                             k_flap_slot, nullptr, 1.0F, 1);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(-0.28F, -0.05F, -0.20F),
                                             QVector3D(0.20F, 0.25F, 0.22F)),
                             k_flap_slot, nullptr, 1.0F, 1);
    return std::move(builder).build();
  }();
  return archetype;
}

auto crupper_fill_role_colors(const HorseVariant &variant, QVector3D *out,
                              std::size_t max) -> std::uint32_t {
  if (max < kCrupperRoleCount) {
    return 0;
  }
  const QVector3D armor_color = variant.tack_color * 0.88F;
  out[k_armor_slot] = armor_color;
  out[k_flap_slot] = armor_color * 0.95F;
  return kCrupperRoleCount;
}

auto crupper_make_static_attachment(std::uint16_t socket_bone_index,
                                    std::uint8_t base_role_byte,
                                    const HorseAttachmentFrame &bind_pose_frame,
                                    const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = make_static_attachment_spec_from_horse_renderer(
      crupper_archetype(), socket_bone_index, bind_pose_frame,
      bind_palette_socket_bone);
  spec.palette_role_remap[k_armor_slot] =
      static_cast<std::uint8_t>(base_role_byte + k_armor_slot);
  spec.palette_role_remap[k_flap_slot] =
      static_cast<std::uint8_t>(base_role_byte + k_flap_slot);
  return spec;
}

void CrupperRenderer::submit(const DrawContext &ctx,
                             const HorseBodyFrames &frames,
                             const HorseVariant &variant,
                             const HorseAnimationContext &,
                             EquipmentBatch &batch) {
  QVector3D const armor_color = variant.tack_color * 0.88F;
  std::array<QVector3D, 2> const palette{armor_color, armor_color * 0.95F};
  append_horse_attachment_archetype(batch, ctx, frames.rump,
                                    crupper_archetype(), palette);
}

} // namespace Render::GL
