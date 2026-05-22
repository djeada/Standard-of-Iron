#include "light_cavalry_saddle_renderer.h"

#include <array>

#include "../../../render_archetype.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

namespace Render::GL {

namespace {

enum LightCavalrySaddlePaletteSlot : std::uint8_t {
  k_saddle_slot = 0U,
};

} // namespace

auto light_cavalry_saddle_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("light_cavalry_horse_saddle");
    builder.add_palette_box(QVector3D(0.0F, 0.022F, 0.00F),
                            QVector3D(0.26F, 0.038F, 0.50F),
                            k_saddle_slot,
                            nullptr,
                            1.0F,
                            4);
    builder.add_palette_box(QVector3D(0.0F, 0.048F, 0.00F),
                            QVector3D(0.18F, 0.044F, 0.28F),
                            k_saddle_slot,
                            nullptr,
                            1.0F,
                            4);
    builder.add_palette_cylinder(QVector3D(-0.10F, 0.072F, 0.16F),
                                 QVector3D(0.10F, 0.072F, 0.16F),
                                 0.026F,
                                 k_saddle_slot,
                                 nullptr,
                                 1.0F,
                                 4);
    builder.add_palette_cylinder(QVector3D(-0.09F, 0.064F, -0.15F),
                                 QVector3D(0.09F, 0.064F, -0.15F),
                                 0.024F,
                                 k_saddle_slot,
                                 nullptr,
                                 1.0F,
                                 4);
    builder.add_palette_box(QVector3D(-0.13F, 0.010F, 0.00F),
                            QVector3D(0.042F, 0.056F, 0.26F),
                            k_saddle_slot,
                            nullptr,
                            1.0F,
                            4);
    builder.add_palette_box(QVector3D(0.13F, 0.010F, 0.00F),
                            QVector3D(0.042F, 0.056F, 0.26F),
                            k_saddle_slot,
                            nullptr,
                            1.0F,
                            4);
    return std::move(builder).build();
  }();
  return archetype;
}

auto light_cavalry_saddle_fill_role_colors(const HorseVariant& variant,
                                           QVector3D* out,
                                           std::size_t max) -> std::uint32_t {
  if (max < k_light_cavalry_saddle_role_count) {
    return 0;
  }
  out[0] = variant.saddle_color;
  return k_light_cavalry_saddle_role_count;
}

auto light_cavalry_saddle_make_static_attachment(
    std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte,
    const HorseAttachmentFrame& bind_pose_frame,
    const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec =
      make_static_attachment_spec_from_horse_renderer(light_cavalry_saddle_archetype(),
                                                      socket_bone_index,
                                                      bind_pose_frame,
                                                      bind_palette_socket_bone);
  spec.palette_role_remap[k_saddle_slot] = base_role_byte;
  return spec;
}

} // namespace Render::GL
