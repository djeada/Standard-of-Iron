#include "blanket_renderer.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

enum BlanketPaletteSlot : std::uint8_t {
  k_blanket_slot = 0U,
};

}

auto blanket_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_blanket");
    builder.add_palette_box(QVector3D(0.0F, -0.005F, 0.0F),
                            QVector3D(0.20F, 0.015F, 0.34F) * 0.86F,
                            k_blanket_slot, nullptr, 1.0F, 4);
    builder.add_palette_box(QVector3D(0.18F, -0.055F, 0.0F),
                            QVector3D(0.06F, 0.11F, 0.25F) * 0.70F,
                            k_blanket_slot, nullptr, 1.0F, 4);
    builder.add_palette_box(QVector3D(-0.18F, -0.055F, 0.0F),
                            QVector3D(0.06F, 0.11F, 0.25F) * 0.70F,
                            k_blanket_slot, nullptr, 1.0F, 4);
    return std::move(builder).build();
  }();
  return archetype;
}

auto blanket_fill_role_colors(const HorseVariant &variant, QVector3D *out,
                              std::size_t max) -> std::uint32_t {
  if (max < kBlanketRoleCount) {
    return 0;
  }
  out[0] = variant.blanket_color;
  return kBlanketRoleCount;
}

auto blanket_make_static_attachment(std::uint16_t socket_bone_index,
                                    std::uint8_t base_role_byte,
                                    const HorseAttachmentFrame &bind_pose_frame,
                                    const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = make_static_attachment_spec_from_horse_renderer(
      blanket_archetype(), socket_bone_index, bind_pose_frame,
      bind_palette_socket_bone);
  spec.palette_role_remap[k_blanket_slot] = base_role_byte;
  return spec;
}

void BlanketRenderer::submit(const DrawContext &ctx,
                             const HorseBodyFrames &frames,
                             const HorseVariant &variant,
                             const HorseAnimationContext &,
                             EquipmentBatch &batch) {
  std::array<QVector3D, 1> const palette{variant.blanket_color};
  append_horse_attachment_archetype(batch, ctx, frames.back_center,
                                    blanket_archetype(), palette);
}

} // namespace Render::GL
