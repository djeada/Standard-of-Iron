#include "scale_barding_renderer.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include "../../../gl/primitives.h"
#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

enum ScaleBardingPaletteSlot : std::uint8_t {
  k_barding_slot = 0U,
};

}

auto scale_barding_chest_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_scale_barding_chest");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, -0.05F, 0.0F),
                                             QVector3D(0.40F, 0.32F, 0.35F)),
                             k_barding_slot, nullptr, 1.0F, 1);
    return std::move(builder).build();
  }();
  return archetype;
}

auto scale_barding_barrel_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_scale_barding_barrel");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.35F, -0.10F, 0.0F),
                                             QVector3D(0.12F, 0.28F, 0.48F)),
                             k_barding_slot, nullptr, 1.0F, 1);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(-0.35F, -0.10F, 0.0F),
                                             QVector3D(0.12F, 0.28F, 0.48F)),
                             k_barding_slot, nullptr, 1.0F, 1);
    return std::move(builder).build();
  }();
  return archetype;
}

auto scale_barding_neck_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_scale_barding_neck");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, 0.0F, 0.15F),
                                             QVector3D(0.36F, 0.30F, 0.38F)),
                             k_barding_slot, nullptr, 1.0F, 1);
    return std::move(builder).build();
  }();
  return archetype;
}

auto scale_barding_fill_role_colors(const HorseVariant &variant, QVector3D *out,
                                    std::size_t max) -> std::uint32_t {
  if (max < kScaleBardingRoleCount) {
    return 0;
  }
  out[0] = variant.tack_color * 0.85F;
  return kScaleBardingRoleCount;
}

auto scale_barding_make_static_attachment(
    const RenderArchetype &archetype, std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte, const HorseAttachmentFrame &bind_pose_frame,
    const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = make_static_attachment_spec_from_horse_renderer(
      archetype, socket_bone_index, bind_pose_frame, bind_palette_socket_bone);
  spec.palette_role_remap[k_barding_slot] = base_role_byte;
  return spec;
}

void ScaleBardingRenderer::submit(const DrawContext &ctx,
                                  const HorseBodyFrames &frames,
                                  const HorseVariant &variant,
                                  const HorseAnimationContext &,
                                  EquipmentBatch &batch) {
  std::array<QVector3D, 1> const palette{variant.tack_color * 0.85F};
  append_horse_attachment_archetype(batch, ctx, frames.chest,
                                    scale_barding_chest_archetype(), palette);
  append_horse_attachment_archetype(batch, ctx, frames.barrel,
                                    scale_barding_barrel_archetype(), palette);
  append_horse_attachment_archetype(batch, ctx, frames.neck_base,
                                    scale_barding_neck_archetype(), palette);
}

} // namespace Render::GL
