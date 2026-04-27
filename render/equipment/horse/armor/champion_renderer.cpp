#include "champion_renderer.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include "../../../gl/primitives.h"
#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

enum ChampionPaletteSlot : std::uint8_t {
  k_body_slot = 0U,
  k_trim_slot = 1U,
  k_shadow_slot = 2U,
};

}

auto champion_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_champion_barding");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, 0.0F, 0.0F),
                                             QVector3D(0.42F, 0.35F, 0.38F)),
                             k_body_slot, nullptr, 1.0F, 1);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, 0.12F, 0.05F),
                                             QVector3D(0.38F, 0.18F, 0.32F)),
                             k_trim_slot, nullptr, 1.0F, 1);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, -0.12F, 0.05F),
                                             QVector3D(0.38F, 0.18F, 0.32F)),
                             k_shadow_slot, nullptr, 1.0F, 1);
    return std::move(builder).build();
  }();
  return archetype;
}

auto champion_fill_role_colors(const HorseVariant &variant, QVector3D *out,
                               std::size_t max) -> std::uint32_t {
  if (max < kChampionRoleCount) {
    return 0;
  }
  const QVector3D armor_color = variant.tack_color * 0.82F;
  out[k_body_slot] = armor_color;
  out[k_trim_slot] = armor_color * 1.05F;
  out[k_shadow_slot] = armor_color * 0.95F;
  return kChampionRoleCount;
}

auto champion_make_static_attachment(
    std::uint16_t socket_bone_index, std::uint8_t base_role_byte,
    const HorseAttachmentFrame &bind_pose_frame,
    const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = make_static_attachment_spec_from_horse_renderer(
      champion_archetype(), socket_bone_index, bind_pose_frame,
      bind_palette_socket_bone);
  spec.palette_role_remap[k_body_slot] =
      static_cast<std::uint8_t>(base_role_byte + k_body_slot);
  spec.palette_role_remap[k_trim_slot] =
      static_cast<std::uint8_t>(base_role_byte + k_trim_slot);
  spec.palette_role_remap[k_shadow_slot] =
      static_cast<std::uint8_t>(base_role_byte + k_shadow_slot);
  return spec;
}

void ChampionRenderer::submit(const DrawContext &ctx,
                              const HorseBodyFrames &frames,
                              const HorseVariant &variant,
                              const HorseAnimationContext &,
                              EquipmentBatch &batch) {
  QVector3D const armor_color = variant.tack_color * 0.82F;
  std::array<QVector3D, 3> const palette{armor_color, armor_color * 1.05F,
                                         armor_color * 0.95F};
  append_horse_attachment_archetype(batch, ctx, frames.chest,
                                    champion_archetype(), palette);
}

} // namespace Render::GL
