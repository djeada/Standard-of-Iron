#include "carthage_saddle_renderer.h"

#include <array>

#include "../../../render_archetype.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

namespace Render::GL {

namespace {

enum CarthageSaddlePaletteSlot : std::uint8_t {
  k_saddle_slot = 0U,
};

} // namespace

auto carthage_saddle_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("carthage_horse_saddle");
    builder.add_palette_box(QVector3D(0.0F, 0.024F, 0.01F),
                            QVector3D(0.32F, 0.042F, 0.62F),
                            k_saddle_slot,
                            nullptr,
                            1.0F,
                            4);
    builder.add_palette_box(QVector3D(0.0F, 0.056F, 0.00F),
                            QVector3D(0.24F, 0.054F, 0.36F),
                            k_saddle_slot,
                            nullptr,
                            1.0F,
                            4);
    builder.add_palette_cylinder(QVector3D(-0.13F, 0.084F, 0.18F),
                                 QVector3D(0.13F, 0.084F, 0.18F),
                                 0.030F,
                                 k_saddle_slot,
                                 nullptr,
                                 1.0F,
                                 4);
    builder.add_palette_cylinder(QVector3D(-0.12F, 0.072F, -0.20F),
                                 QVector3D(0.12F, 0.072F, -0.20F),
                                 0.029F,
                                 k_saddle_slot,
                                 nullptr,
                                 1.0F,
                                 4);
    builder.add_palette_box(QVector3D(-0.16F, 0.012F, 0.00F),
                            QVector3D(0.052F, 0.068F, 0.32F),
                            k_saddle_slot,
                            nullptr,
                            1.0F,
                            4);
    builder.add_palette_box(QVector3D(0.16F, 0.012F, 0.00F),
                            QVector3D(0.052F, 0.068F, 0.32F),
                            k_saddle_slot,
                            nullptr,
                            1.0F,
                            4);
    return std::move(builder).build();
  }();
  return archetype;
}

auto carthage_saddle_fill_role_colors(const HorseVariant& variant,
                                      QVector3D* out,
                                      std::size_t max) -> std::uint32_t {
  if (max < k_carthage_saddle_role_count) {
    return 0;
  }
  out[0] = variant.saddle_color;
  return k_carthage_saddle_role_count;
}

auto carthage_saddle_make_static_attachment(std::uint16_t socket_bone_index,
                                            std::uint8_t base_role_byte,
                                            const HorseAttachmentFrame& bind_pose_frame,
                                            const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec =
      make_static_attachment_spec_from_horse_renderer(carthage_saddle_archetype(),
                                                      socket_bone_index,
                                                      bind_pose_frame,
                                                      bind_palette_socket_bone);
  spec.palette_role_remap[k_saddle_slot] = base_role_byte;
  return spec;
}

} // namespace Render::GL
