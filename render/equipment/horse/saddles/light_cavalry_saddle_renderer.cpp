#include "light_cavalry_saddle_renderer.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

enum LightCavalrySaddlePaletteSlot : std::uint8_t {
  k_saddle_slot = 0U,
};

}

auto light_cavalry_saddle_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("light_cavalry_horse_saddle");
    builder.add_palette_box(QVector3D(0.0F, 0.008F, 0.0F),
                            QVector3D(0.18F, 0.09F, 0.48F) * 0.21F,
                            k_saddle_slot, nullptr, 1.0F, 4);
    builder.add_palette_box(QVector3D(0.0F, 0.016F, 0.11F),
                            QVector3D(0.07F, 0.28F, 0.16F) * 0.15F,
                            k_saddle_slot, nullptr, 1.0F, 4);
    builder.add_palette_box(QVector3D(0.0F, 0.020F, -0.12F),
                            QVector3D(0.08F, 0.30F, 0.18F) * 0.16F,
                            k_saddle_slot, nullptr, 1.0F, 4);
    return std::move(builder).build();
  }();
  return archetype;
}

auto light_cavalry_saddle_fill_role_colors(const HorseVariant &variant,
                                           QVector3D *out,
                                           std::size_t max) -> std::uint32_t {
  if (max < kLightCavalrySaddleRoleCount) {
    return 0;
  }
  out[0] = variant.saddle_color;
  return kLightCavalrySaddleRoleCount;
}

auto light_cavalry_saddle_make_static_attachment(
    std::uint16_t socket_bone_index, std::uint8_t base_role_byte,
    const HorseAttachmentFrame &bind_pose_frame,
    const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = make_static_attachment_spec_from_horse_renderer(
      light_cavalry_saddle_archetype(), socket_bone_index, bind_pose_frame,
      bind_palette_socket_bone);
  spec.palette_role_remap[k_saddle_slot] = base_role_byte;
  return spec;
}

} // namespace Render::GL
