#include "reins_renderer.h"

#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include <array>

namespace Render::GL {

namespace {

constexpr std::uint8_t k_rein_slot = 0U;
constexpr float k_rein_radius = 0.0025F;
constexpr float k_bit_side_offset = 0.11F;
constexpr float k_bit_forward_offset = 0.02F;
constexpr float k_bit_up_offset = -0.03F;

auto local_from_frame(const HorseAttachmentFrame &base,
                      const QVector3D &world_point) -> QVector3D {
  return QVector3D(
      QVector3D::dotProduct(world_point - base.origin, base.right),
      QVector3D::dotProduct(world_point - base.origin, base.up),
      QVector3D::dotProduct(world_point - base.origin, base.forward));
}

void add_rein_segments(RenderArchetypeBuilder &builder,
                       const HorseAttachmentFrame &muzzle,
                       const HorseAttachmentFrame &back) {
  (void)back;
  std::array<QVector3D, 2> bits{};

  for (int i = 0; i < 2; ++i) {
    const float side = (i == 0) ? 1.0F : -1.0F;

    const QVector3D bit_pos_world =
        muzzle.origin + muzzle.right * side * k_bit_side_offset +
        muzzle.forward * k_bit_forward_offset + muzzle.up * k_bit_up_offset;
    bits[i] = local_from_frame(back, bit_pos_world);
  }

  builder.add_palette_cylinder(bits[0], bits[1], k_rein_radius, k_rein_slot,
                               nullptr, 1.0F, 4);
}

} // namespace

auto reins_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    const auto back = horse_baseline_back_center_frame();
    const auto muzzle = horse_baseline_muzzle_frame();
    RenderArchetypeBuilder builder("horse_reins");
    add_rein_segments(builder, muzzle, back);
    return std::move(builder).build();
  }();
  return archetype;
}

auto reins_fill_role_colors(const HorseVariant &variant, QVector3D *out,
                            std::size_t max) -> std::uint32_t {
  if (max < kReinsRoleCount) {
    return 0;
  }
  out[0] = variant.tack_color;
  return kReinsRoleCount;
}

auto reins_make_static_attachment(std::uint16_t socket_bone_index,
                                  std::uint8_t base_role_byte,
                                  const HorseAttachmentFrame &bind_pose_frame,
                                  const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = make_static_attachment_spec_from_horse_renderer(
      reins_archetype(), socket_bone_index, bind_pose_frame,
      bind_palette_socket_bone);
  spec.palette_role_remap[k_rein_slot] = base_role_byte;
  return spec;
}

void ReinsRenderer::submit(const DrawContext &ctx,
                           const HorseBodyFrames &frames,
                           const HorseVariant &variant,
                           const HorseAnimationContext &,
                           EquipmentBatch &batch) {
  std::array<QVector3D, 1> const palette{variant.tack_color};
  append_equipment_archetype(
      batch, reins_archetype(),
      frames.back_center.make_local_transform(ctx.model, QVector3D(), 1.0F),
      palette);
}

} // namespace Render::GL
