#include "roman_saddle_renderer.h"

#include <array>

#include "../../../render_archetype.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

namespace Render::GL {

namespace {

enum RomanSaddlePaletteSlot : std::uint8_t {
  k_saddle_slot = 0U,
};

} // namespace

auto roman_saddle_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("roman_horse_saddle");
    builder.add_palette_box(QVector3D(0.0F, 0.026F, 0.02F),
                            QVector3D(0.30F, 0.044F, 0.58F),
                            k_saddle_slot,
                            nullptr,
                            1.0F,
                            4);
    builder.add_palette_box(QVector3D(0.0F, 0.058F, 0.02F),
                            QVector3D(0.22F, 0.056F, 0.34F),
                            k_saddle_slot,
                            nullptr,
                            1.0F,
                            4);
    builder.add_palette_cylinder(QVector3D(-0.12F, 0.088F, 0.20F),
                                 QVector3D(0.12F, 0.088F, 0.20F),
                                 0.032F,
                                 k_saddle_slot,
                                 nullptr,
                                 1.0F,
                                 4);
    builder.add_palette_cylinder(QVector3D(-0.11F, 0.078F, -0.18F),
                                 QVector3D(0.11F, 0.078F, -0.18F),
                                 0.030F,
                                 k_saddle_slot,
                                 nullptr,
                                 1.0F,
                                 4);
    builder.add_palette_box(QVector3D(-0.15F, 0.014F, 0.03F),
                            QVector3D(0.050F, 0.072F, 0.30F),
                            k_saddle_slot,
                            nullptr,
                            1.0F,
                            4);
    builder.add_palette_box(QVector3D(0.15F, 0.014F, 0.03F),
                            QVector3D(0.050F, 0.072F, 0.30F),
                            k_saddle_slot,
                            nullptr,
                            1.0F,
                            4);
    return std::move(builder).build();
  }();
  return archetype;
}

auto roman_saddle_fill_role_colors(const HorseVariant& variant,
                                   QVector3D* out,
                                   std::size_t max) -> std::uint32_t {
  if (max < k_roman_saddle_role_count) {
    return 0;
  }
  out[0] = variant.saddle_color;
  return k_roman_saddle_role_count;
}

auto roman_saddle_make_static_attachment(std::uint16_t socket_bone_index,
                                         std::uint8_t base_role_byte,
                                         const HorseAttachmentFrame& bind_pose_frame,
                                         const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = make_static_attachment_spec_from_horse_renderer(roman_saddle_archetype(),
                                                              socket_bone_index,
                                                              bind_pose_frame,
                                                              bind_palette_socket_bone);
  spec.palette_role_remap[k_saddle_slot] = base_role_byte;
  return spec;
}

} // namespace Render::GL
