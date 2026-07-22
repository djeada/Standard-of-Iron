#include "commander_helmets.h"

#include <array>
#include <string_view>
#include <vector>

#include "../attachment_builder.h"
#include "../equipment_archetype_helpers.h"
#include "../generated_equipment.h"
#include "../humanoid_attachment_archetype.h"
#include "helmet_alignment.h"

namespace Render::GL {
namespace {

enum CommanderHelmetPaletteSlot : std::uint8_t {
  k_metal_slot = 0U,
  k_dark_slot = 1U,
  k_accent_slot = 2U,
  k_plume_slot = 3U,
};

using Primitive = GeneratedEquipmentPrimitive;

void add_base_helmet(std::vector<Primitive>& primitives, bool face_guard) {
  primitives.push_back(generated_ellipsoid(QVector3D(0.0F, 0.48F, -0.06F),
                                           QVector3D(1.42F, 1.20F, 1.48F),
                                           k_metal_slot,
                                           1.0F,
                                           2));
  primitives.push_back(generated_cylinder(QVector3D(0.0F, -0.34F, -0.02F),
                                          QVector3D(0.0F, -0.16F, -0.02F),
                                          1.43F,
                                          k_dark_slot,
                                          1.0F,
                                          2));
  primitives.push_back(generated_cylinder(QVector3D(0.0F, 0.28F, 1.18F),
                                          QVector3D(0.0F, -0.56F, 1.06F),
                                          face_guard ? 0.13F : 0.075F,
                                          k_accent_slot,
                                          1.0F,
                                          2));
  primitives.push_back(generated_cylinder(QVector3D(-1.20F, 0.06F, 0.20F),
                                          QVector3D(-0.96F, -0.72F, 0.36F),
                                          0.20F,
                                          k_dark_slot,
                                          1.0F,
                                          2));
  primitives.push_back(generated_cylinder(QVector3D(1.20F, 0.06F, 0.20F),
                                          QVector3D(0.96F, -0.72F, 0.36F),
                                          0.20F,
                                          k_dark_slot,
                                          1.0F,
                                          2));
  primitives.push_back(generated_cylinder(QVector3D(-0.92F, 0.78F, 0.88F),
                                          QVector3D(0.92F, 0.78F, 0.88F),
                                          0.075F,
                                          k_accent_slot,
                                          1.0F,
                                          2));
}

void add_fabius_crest(std::vector<Primitive>& primitives) {
  primitives.push_back(generated_box(QVector3D(0.0F, 1.70F, -0.05F),
                                     QVector3D(0.16F, 0.22F, 1.25F),
                                     k_accent_slot,
                                     1.0F,
                                     2));
  for (int i = 0; i < 9; ++i) {
    float const t = static_cast<float>(i) / 8.0F;
    QVector3D const root(0.0F, 1.82F, 0.82F - t * 1.70F);
    float const crown = 0.84F - 0.22F * t;
    QVector3D const tip(0.0F, 1.82F + crown, 0.78F - t * 1.92F);
    primitives.push_back(generated_cylinder(root, tip, 0.075F, k_plume_slot, 1.0F, 0));
  }
  primitives.push_back(generated_cone(QVector3D(-0.94F, 0.96F, -0.18F),
                                      QVector3D(-1.54F, 1.36F, -0.42F),
                                      0.16F,
                                      k_dark_slot,
                                      1.0F,
                                      2));
  primitives.push_back(generated_cone(QVector3D(0.94F, 0.96F, -0.18F),
                                      QVector3D(1.54F, 1.36F, -0.42F),
                                      0.16F,
                                      k_dark_slot,
                                      1.0F,
                                      2));
}

void add_scipio_crest(std::vector<Primitive>& primitives) {
  primitives.push_back(generated_cylinder(QVector3D(-1.54F, 1.62F, -0.02F),
                                          QVector3D(1.54F, 1.62F, -0.02F),
                                          0.10F,
                                          k_accent_slot,
                                          1.0F,
                                          2));
  for (int i = 0; i < 11; ++i) {
    float const t = static_cast<float>(i) / 10.0F;
    float const x = -1.48F + t * 2.96F;
    float const lift = 0.82F - 0.20F * (2.0F * t - 1.0F) * (2.0F * t - 1.0F);
    primitives.push_back(generated_cylinder(QVector3D(x, 1.68F, -0.02F),
                                            QVector3D(x * 1.08F, 1.68F + lift, -0.20F),
                                            0.080F,
                                            k_plume_slot,
                                            1.0F,
                                            0));
  }
  for (int side : {-1, 1}) {
    float const s = static_cast<float>(side);
    primitives.push_back(generated_cylinder(QVector3D(s * 0.22F, 0.92F, 1.02F),
                                            QVector3D(s * 0.78F, 1.20F, 0.92F),
                                            0.055F,
                                            k_accent_slot,
                                            1.0F,
                                            2));
    primitives.push_back(generated_cone(QVector3D(s * 0.50F, 1.05F, 0.96F),
                                        QVector3D(s * 0.88F, 1.44F, 0.90F),
                                        0.10F,
                                        k_accent_slot,
                                        1.0F,
                                        2));
  }
}

void add_marcellus_crest(std::vector<Primitive>& primitives) {
  primitives.push_back(generated_cone(QVector3D(-0.70F, 1.26F, -0.05F),
                                      QVector3D(-0.92F, 2.66F, -0.24F),
                                      0.34F,
                                      k_dark_slot,
                                      1.0F,
                                      2));
  primitives.push_back(generated_cone(QVector3D(0.70F, 1.26F, -0.05F),
                                      QVector3D(0.92F, 2.66F, -0.24F),
                                      0.34F,
                                      k_dark_slot,
                                      1.0F,
                                      2));
  primitives.push_back(generated_cone(QVector3D(-0.62F, 1.36F, -0.02F),
                                      QVector3D(-0.80F, 2.38F, -0.18F),
                                      0.20F,
                                      k_plume_slot,
                                      1.0F,
                                      0));
  primitives.push_back(generated_cone(QVector3D(0.62F, 1.36F, -0.02F),
                                      QVector3D(0.80F, 2.38F, -0.18F),
                                      0.20F,
                                      k_plume_slot,
                                      1.0F,
                                      0));
  primitives.push_back(generated_cone(QVector3D(0.0F, 1.46F, -0.32F),
                                      QVector3D(0.0F, 2.30F, -1.24F),
                                      0.24F,
                                      k_accent_slot,
                                      1.0F,
                                      2));
}

void add_hanno_crest(std::vector<Primitive>& primitives) {
  for (int i = -1; i <= 1; ++i) {
    float const x = static_cast<float>(i) * 0.62F;
    float const height = i == 0 ? 3.10F : 2.62F;
    primitives.push_back(generated_cone(QVector3D(x, 1.30F, -0.05F),
                                        QVector3D(x * 1.16F, height, -0.16F),
                                        i == 0 ? 0.28F : 0.23F,
                                        i == 0 ? k_accent_slot : k_dark_slot,
                                        1.0F,
                                        2));
  }
  primitives.push_back(
      generated_sphere(QVector3D(-1.48F, 0.42F, 0.0F), 0.28F, k_accent_slot, 1.0F, 2));
  primitives.push_back(
      generated_sphere(QVector3D(1.48F, 0.42F, 0.0F), 0.28F, k_accent_slot, 1.0F, 2));
  primitives.push_back(generated_cylinder(QVector3D(-1.48F, 0.42F, -0.12F),
                                          QVector3D(1.48F, 0.42F, -0.12F),
                                          0.075F,
                                          k_plume_slot,
                                          1.0F,
                                          2));
}

void add_hasdrubal_crest(std::vector<Primitive>& primitives) {
  QVector3D last(0.0F, 1.52F, 0.40F);
  for (int i = 1; i <= 8; ++i) {
    float const t = static_cast<float>(i) / 8.0F;
    QVector3D const next(
        0.20F * t, 1.52F + 1.18F * (1.0F - 0.46F * t), 0.40F - 2.42F * t);
    primitives.push_back(
        generated_cylinder(last, next, 0.11F - 0.045F * t, k_plume_slot, 1.0F, 0));
    last = next;
  }
  primitives.push_back(generated_cone(QVector3D(-1.06F, 0.90F, -0.12F),
                                      QVector3D(-1.84F, 1.48F, -0.76F),
                                      0.18F,
                                      k_accent_slot,
                                      1.0F,
                                      2));
  primitives.push_back(generated_cone(QVector3D(1.06F, 0.90F, -0.12F),
                                      QVector3D(1.84F, 1.48F, -0.76F),
                                      0.18F,
                                      k_accent_slot,
                                      1.0F,
                                      2));
}

void add_hannibal_crest(std::vector<Primitive>& primitives) {
  primitives.push_back(generated_box(QVector3D(0.0F, 1.72F, -0.08F),
                                     QVector3D(0.22F, 0.20F, 1.34F),
                                     k_accent_slot,
                                     1.0F,
                                     2));
  for (int row = 0; row < 2; ++row) {
    for (int i = 0; i < 9; ++i) {
      float const t = static_cast<float>(i) / 8.0F;
      float const side = row == 0 ? -0.14F : 0.14F;
      QVector3D const root(side, 1.82F, 0.88F - t * 1.82F);
      QVector3D const tip(side * 1.8F, 2.92F - 0.20F * t, 0.78F - t * 2.18F);
      primitives.push_back(
          generated_cylinder(root, tip, 0.085F, k_plume_slot, 1.0F, 0));
    }
  }
  primitives.push_back(generated_cone(QVector3D(-1.04F, 0.96F, -0.08F),
                                      QVector3D(-1.86F, 1.72F, -0.60F),
                                      0.24F,
                                      k_dark_slot,
                                      1.0F,
                                      2));
  primitives.push_back(generated_cone(QVector3D(1.04F, 0.96F, -0.08F),
                                      QVector3D(1.52F, 1.38F, -0.42F),
                                      0.19F,
                                      k_dark_slot,
                                      1.0F,
                                      2));
  primitives.push_back(generated_cone(QVector3D(0.0F, 0.74F, 1.20F),
                                      QVector3D(0.0F, -0.78F, 1.02F),
                                      0.20F,
                                      k_accent_slot,
                                      1.0F,
                                      2));
}

auto build_commander_helmet(CommanderHelmetStyle style,
                            std::string_view debug_name) -> RenderArchetype {
  std::vector<Primitive> primitives;
  primitives.reserve(40U);
  add_base_helmet(primitives, style == CommanderHelmetStyle::Hannibal);
  switch (style) {
  case CommanderHelmetStyle::Fabius:
    add_fabius_crest(primitives);
    break;
  case CommanderHelmetStyle::Scipio:
    add_scipio_crest(primitives);
    break;
  case CommanderHelmetStyle::Marcellus:
    add_marcellus_crest(primitives);
    break;
  case CommanderHelmetStyle::Hanno:
    add_hanno_crest(primitives);
    break;
  case CommanderHelmetStyle::Hasdrubal:
    add_hasdrubal_crest(primitives);
    break;
  case CommanderHelmetStyle::Hannibal:
    add_hannibal_crest(primitives);
    break;
  }
  return build_generated_equipment_archetype(debug_name, primitives);
}

auto commander_colors(CommanderHelmetStyle style, const HumanoidPalette& palette)
    -> std::array<QVector3D, k_commander_helmet_role_count> {
  (void)palette;
  switch (style) {
  case CommanderHelmetStyle::Fabius:
    return {QVector3D(0.24F, 0.28F, 0.34F),
            QVector3D(0.08F, 0.09F, 0.11F),
            QVector3D(0.70F, 0.72F, 0.68F),
            QVector3D(0.48F, 0.025F, 0.03F)};
  case CommanderHelmetStyle::Scipio:
    return {QVector3D(0.54F, 0.32F, 0.08F),
            QVector3D(0.12F, 0.09F, 0.07F),
            QVector3D(1.0F, 0.72F, 0.18F),
            QVector3D(0.82F, 0.055F, 0.04F)};
  case CommanderHelmetStyle::Marcellus:
    return {QVector3D(0.28F, 0.30F, 0.34F),
            QVector3D(0.10F, 0.11F, 0.13F),
            QVector3D(0.64F, 0.08F, 0.06F),
            QVector3D(0.32F, 0.035F, 0.025F)};
  case CommanderHelmetStyle::Hanno:
    return {QVector3D(0.38F, 0.21F, 0.065F),
            QVector3D(0.16F, 0.09F, 0.05F),
            QVector3D(0.96F, 0.64F, 0.16F),
            QVector3D(0.26F, 0.035F, 0.32F)};
  case CommanderHelmetStyle::Hasdrubal:
    return {QVector3D(0.22F, 0.14F, 0.06F),
            QVector3D(0.07F, 0.10F, 0.10F),
            QVector3D(0.20F, 0.58F, 0.54F),
            QVector3D(0.20F, 0.055F, 0.28F)};
  case CommanderHelmetStyle::Hannibal:
    return {QVector3D(0.15F, 0.11F, 0.065F),
            QVector3D(0.035F, 0.04F, 0.05F),
            QVector3D(0.86F, 0.56F, 0.12F),
            QVector3D(0.018F, 0.02F, 0.025F)};
  }
  return {QVector3D(0.25F, 0.27F, 0.30F),
          QVector3D(0.08F, 0.08F, 0.09F),
          QVector3D(0.55F, 0.34F, 0.10F),
          QVector3D(0.5F, 0.05F, 0.05F)};
}

} // namespace

auto commander_helmet_archetype(CommanderHelmetStyle style) -> const RenderArchetype& {
  static const RenderArchetype fabius =
      build_commander_helmet(CommanderHelmetStyle::Fabius, "commander_helmet_fabius");
  static const RenderArchetype scipio =
      build_commander_helmet(CommanderHelmetStyle::Scipio, "commander_helmet_scipio");
  static const RenderArchetype marcellus = build_commander_helmet(
      CommanderHelmetStyle::Marcellus, "commander_helmet_marcellus");
  static const RenderArchetype hanno =
      build_commander_helmet(CommanderHelmetStyle::Hanno, "commander_helmet_hanno");
  static const RenderArchetype hasdrubal = build_commander_helmet(
      CommanderHelmetStyle::Hasdrubal, "commander_helmet_hasdrubal");
  static const RenderArchetype hannibal = build_commander_helmet(
      CommanderHelmetStyle::Hannibal, "commander_helmet_hannibal");
  switch (style) {
  case CommanderHelmetStyle::Fabius:
    return fabius;
  case CommanderHelmetStyle::Scipio:
    return scipio;
  case CommanderHelmetStyle::Marcellus:
    return marcellus;
  case CommanderHelmetStyle::Hanno:
    return hanno;
  case CommanderHelmetStyle::Hasdrubal:
    return hasdrubal;
  case CommanderHelmetStyle::Hannibal:
    return hannibal;
  }
  return fabius;
}

auto commander_helmet_fill_role_colors(CommanderHelmetStyle style,
                                       const HumanoidPalette& palette,
                                       QVector3D* out,
                                       std::size_t max) -> std::uint32_t {
  if (out == nullptr || max < k_commander_helmet_role_count) {
    return 0U;
  }
  auto const colors = commander_colors(style, palette);
  for (std::size_t i = 0; i < colors.size(); ++i) {
    out[i] = colors[i];
  }
  return k_commander_helmet_role_count;
}

auto commander_helmet_make_static_attachment(CommanderHelmetStyle style,
                                             std::uint16_t socket_bone_index,
                                             std::uint8_t base_role_byte,
                                             const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr float k_head_socket_radius = 0.16F;
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &commander_helmet_archetype(style),
      .socket_bone_index = socket_bone_index,
      .uniform_scale = k_helmet_uniform_scale * 1.06F,
      .authored_local_offset = k_helmet_local_offset * k_helmet_uniform_scale,
      .bind_radius = k_head_socket_radius,
      .bind_socket_transform = bind_palette_socket_bone,
  });
  fill_sequential_role_remap(spec, base_role_byte, k_commander_helmet_role_count);
  return spec;
}

} // namespace Render::GL
