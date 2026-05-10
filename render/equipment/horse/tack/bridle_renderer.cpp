#include "bridle_renderer.h"

#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include <array>

namespace Render::GL {

namespace {

constexpr std::uint8_t k_bridle_slot = 0U;
constexpr float k_headband_radius = 0.012F;
constexpr float k_noseband_radius = 0.010F;
constexpr float k_cheek_radius = 0.011F;
constexpr float k_throatlatch_radius = 0.010F;

auto local_from_frame(const HorseAttachmentFrame &base,
                      const QVector3D &world_point) -> QVector3D {
  return QVector3D(
      QVector3D::dotProduct(world_point - base.origin, base.right),
      QVector3D::dotProduct(world_point - base.origin, base.up),
      QVector3D::dotProduct(world_point - base.origin, base.forward));
}

void add_bridle_segment(RenderArchetypeBuilder &builder,
                        const HorseAttachmentFrame &base,
                        const QVector3D &start_world,
                        const QVector3D &end_world, float radius) {
  builder.add_palette_cylinder(local_from_frame(base, start_world),
                               local_from_frame(base, end_world), radius,
                               k_bridle_slot, nullptr, 1.0F, 4);
}

} // namespace

auto bridle_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    const auto head = horse_baseline_head_frame();
    const auto muzzle = horse_baseline_muzzle_frame();

    RenderArchetypeBuilder builder("horse_bridle");

    QVector3D const headband_start =
        head.origin + head.right * 0.22F + head.up * 0.15F;
    QVector3D const headband_end =
        head.origin - head.right * 0.22F + head.up * 0.15F;
    add_bridle_segment(builder, head, headband_start, headband_end,
                       k_headband_radius);

    QVector3D const noseband_start =
        muzzle.origin + muzzle.right * 0.20F + muzzle.forward * 0.10F;
    QVector3D const noseband_end =
        muzzle.origin - muzzle.right * 0.20F + muzzle.forward * 0.10F;
    add_bridle_segment(builder, head, noseband_start, noseband_end,
                       k_noseband_radius);

    for (int i = 0; i < 2; ++i) {
      float const side = (i == 0) ? 1.0F : -1.0F;
      QVector3D const cheek_top =
          head.origin + head.right * side * 0.22F + head.up * 0.15F;
      QVector3D const cheek_bottom =
          muzzle.origin + muzzle.right * side * 0.20F + muzzle.forward * 0.10F;
      add_bridle_segment(builder, head, cheek_top, cheek_bottom,
                         k_cheek_radius);
    }

    QVector3D const throatlatch_start = head.origin - head.up * 0.10F;
    QVector3D const throatlatch_end = head.origin - head.up * 0.25F;
    add_bridle_segment(builder, head, throatlatch_start, throatlatch_end,
                       k_throatlatch_radius);
    return std::move(builder).build();
  }();
  return archetype;
}

auto bridle_fill_role_colors(const HorseVariant &variant, QVector3D *out,
                             std::size_t max) -> std::uint32_t {
  if (max < k_bridle_role_count) {
    return 0;
  }
  out[0] = variant.tack_color;
  return k_bridle_role_count;
}

auto bridle_make_static_attachment(std::uint16_t socket_bone_index,
                                   std::uint8_t base_role_byte,
                                   const HorseAttachmentFrame &bind_pose_frame,
                                   const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = make_static_attachment_spec_from_horse_renderer(
      bridle_archetype(), socket_bone_index, bind_pose_frame,
      bind_palette_socket_bone);
  spec.palette_role_remap[k_bridle_slot] = base_role_byte;
  return spec;
}

void BridleRenderer::submit(const DrawContext &ctx,
                            const HorseBodyFrames &frames,
                            const HorseVariant &variant,
                            const HorseAnimationContext &,
                            EquipmentBatch &batch) {
  std::array<QVector3D, 1> const palette{variant.tack_color};
  append_horse_attachment_archetype(batch, ctx, frames.head, bridle_archetype(),
                                    palette);
}

} // namespace Render::GL
