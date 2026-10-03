#include "commander_helmets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "helmet_alignment.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/equipment_archetype_helpers.h"
#include "render/equipment/generated_equipment.h"
#include "render/equipment/helmets/commander_helmet_parts.h"
#include "render/equipment/humanoid_attachment_archetype.h"

namespace Render::GL {
namespace {

using namespace Render::GL::CommanderHelmetParts;

using Primitive = GeneratedEquipmentPrimitive;

void add_lobed_mass(std::vector<Primitive>& primitives,
                    std::span<const QVector3D> spine,
                    float root_radius,
                    float tip_radius,
                    std::uint8_t slot,
                    float lateral_scale = 1.0F) {
  if (spine.size() < 2U) {
    return;
  }
  float total = 0.0F;
  for (std::size_t i = 1U; i < spine.size(); ++i) {
    total += (spine[i] - spine[i - 1U]).length();
  }
  if (total <= 1.0e-4F) {
    return;
  }

  std::vector<float> stops;
  stops.reserve(32U);
  for (float travelled = 0.0F; travelled < total && stops.size() < 26U;) {
    float const t = travelled / total;
    stops.push_back(t);
    travelled +=
        std::max(0.03F, 0.42F * (root_radius + ((tip_radius - root_radius) * t)));
  }
  stops.push_back(1.0F);
  for (float const t : stops) {
    float remaining = t * total;
    QVector3D point = spine.back();
    for (std::size_t k = 1U; k < spine.size(); ++k) {
      float const segment = (spine[k] - spine[k - 1U]).length();
      if (remaining <= segment || k + 1U == spine.size()) {
        float const u =
            segment > 1.0e-4F ? std::clamp(remaining / segment, 0.0F, 1.0F) : 0.0F;
        point = spine[k - 1U] + ((spine[k] - spine[k - 1U]) * u);
        break;
      }
      remaining -= segment;
    }
    float const r = root_radius + ((tip_radius - root_radius) * t);
    primitives.push_back(
        generated_ellipsoid(point, QVector3D(r * lateral_scale, r, r), slot, 1.0F, 0));
  }
}

void add_base_helmet(std::vector<Primitive>& primitives, bool face_guard) {
  append_specs(primitives, std::span{k_base_helmet_primitives});

  for (int side = 0; side < 2; ++side) {
    float const s = (side == 0) ? -1.0F : 1.0F;
    primitives.push_back(generated_cylinder(QVector3D(s * 0.52F, -0.24F, 1.22F),
                                            QVector3D(s * 1.16F, -0.40F, 0.34F),
                                            0.11F,
                                            k_accent_slot,
                                            1.0F,
                                            2));
  }

  primitives.push_back(generated_cylinder(QVector3D(0.0F, -0.06F, 1.60F),
                                          QVector3D(0.0F, -0.80F, 1.44F),
                                          face_guard ? 0.16F : 0.09F,
                                          k_accent_slot,
                                          1.0F,
                                          2));

  for (int side = 0; side < 2; ++side) {
    float const s = (side == 0) ? -1.0F : 1.0F;
    primitives.push_back(generated_cylinder(QVector3D(s * 1.14F, -0.30F, 0.32F),
                                            QVector3D(s * 1.32F, -0.35F, 0.27F),
                                            0.60F,
                                            k_metal_slot,
                                            1.0F,
                                            2));
    primitives.push_back(generated_cylinder(QVector3D(s * 1.02F, -0.92F, 0.48F),
                                            QVector3D(s * 1.20F, -0.97F, 0.43F),
                                            0.46F,
                                            k_metal_slot,
                                            1.0F,
                                            2));
    primitives.push_back(generated_cone(QVector3D(s * 0.94F, -1.18F, 0.56F),
                                        QVector3D(s * 0.70F, -1.88F, 0.62F),
                                        0.30F,
                                        k_metal_slot,
                                        1.0F,
                                        2));
    primitives.push_back(generated_sphere(
        QVector3D(s * 1.36F, -0.18F, 0.26F), 0.16F, k_accent_slot, 1.0F, 2));
    primitives.push_back(generated_sphere(
        QVector3D(s * 1.24F, -0.74F, 0.46F), 0.12F, k_dark_slot, 1.0F, 2));
  }
}

void add_roman_base_helmet(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_roman_base_helmet_primitives});

  for (int side : {-1, 1}) {
    float const sign = static_cast<float>(side);

    primitives.push_back(generated_ellipsoid(
        {sign * 1.20F, -0.69F, 0.46F}, {0.13F, 0.75F, 0.46F}, k_metal_slot, 1.0F, 2));
    primitives.push_back(generated_cylinder({sign * 1.27F, -0.32F, 0.84F},
                                            {sign * 1.19F, -1.22F, 0.68F},
                                            0.052F,
                                            k_accent_slot,
                                            1.0F,
                                            2));
    primitives.push_back(
        generated_sphere({sign * 1.35F, -0.15F, 0.40F}, 0.10F, k_accent_slot, 1.0F, 2));

    for (int leaf = 0; leaf < 5; ++leaf) {
      float const t = static_cast<float>(leaf) / 4.0F;
      primitives.push_back(generated_ellipsoid(
          {sign * (0.22F + 0.98F * t), 0.25F + 0.28F * t, 1.48F - 0.57F * t},
          {0.17F, 0.095F, 0.065F},
          k_accent_slot,
          1.0F,
          2));
    }
  }
  primitives.push_back(generated_ellipsoid(
      {0.0F, -0.16F, 1.43F}, {1.06F, 0.07F, 0.12F}, k_accent_slot, 1.0F, 2));
}

void add_crest_hair(std::vector<Primitive>& primitives, bool transverse) {
  for (int strand = 0; strand < 27; ++strand) {
    float const t = static_cast<float>(strand) / 26.0F;
    float const along = (2.0F * t - 1.0F) * 1.48F;
    float const height = 1.75F + 1.04F * std::sin(t * 3.14159265F);
    QVector3D root = transverse ? QVector3D(along * 0.91F, height - 0.34F, -0.04F)
                                : QVector3D(0.0F, height - 0.34F, along - 0.05F);
    QVector3D tip = transverse ? QVector3D(along, height + 0.09F, -0.04F)
                               : QVector3D(0.0F, height + 0.09F, along - 0.05F);
    primitives.push_back(generated_cone(root, tip, 0.082F, k_plume_slot, 1.0F, 0));
  }
}

void add_fabius_crest(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_fabius_crest_primitives});
  add_crest_hair(primitives, false);
}

void add_scipio_crest(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_scipio_crest_primitives});

  add_crest_hair(primitives, true);
}

void add_marcellus_crest(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_marcellus_crest_primitives});
  std::array<QVector3D, 6> const tail{{
      {0.0F, 1.70F, 0.36F},
      {0.0F, 2.02F, -0.04F},
      {0.0F, 2.06F, -0.56F},
      {0.0F, 1.88F, -1.08F},
      {0.0F, 1.56F, -1.54F},
      {0.0F, 1.14F, -1.90F},
  }};
  add_lobed_mass(primitives, tail, 0.46F, 0.20F, k_plume_slot, 0.44F);
}

void add_hanno_crest(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_hanno_crest_primitives});

  std::array<QVector3D, 7> const centre{{
      {0.0F, 1.42F, -0.06F},
      {0.0F, 1.78F, -0.09F},
      {0.0F, 2.14F, -0.12F},
      {0.0F, 2.48F, -0.15F},
      {0.0F, 2.78F, -0.18F},
      {0.0F, 2.94F, -0.21F},
      {0.0F, 3.10F, -0.24F},
  }};
  add_lobed_mass(primitives, centre, 0.58F, 0.26F, k_plume_slot, 0.42F);
  for (int side = 0; side < 2; ++side) {
    float const s = (side == 0) ? -1.0F : 1.0F;
    std::array<QVector3D, 6> const wing{{
        {s * 0.56F, 1.34F, -0.06F},
        {s * 0.68F, 1.68F, -0.10F},
        {s * 0.80F, 2.00F, -0.14F},
        {s * 0.92F, 2.28F, -0.18F},
        {s * 1.02F, 2.50F, -0.22F},
        {s * 1.10F, 2.68F, -0.26F},
    }};
    add_lobed_mass(primitives, wing, 0.48F, 0.22F, k_accent_slot, 0.42F);
  }

  for (int side = 0; side < 2; ++side) {
    float const s = (side == 0) ? -1.0F : 1.0F;
    primitives.push_back(generated_sphere(
        QVector3D(s * 1.32F, 0.16F, 0.44F), 0.24F, k_accent_slot, 1.0F, 2));
    primitives.push_back(generated_ellipsoid(QVector3D(s * 0.74F, 0.30F, 1.16F),
                                             QVector3D(0.44F, 0.12F, 0.20F),
                                             k_plume_slot,
                                             1.0F,
                                             2));
  }
}

void add_hasdrubal_crest(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_hasdrubal_crest_primitives});

  std::array<QVector3D, 7> const tail{{
      {0.0F, 1.50F, 0.40F},
      {0.0F, 1.86F, 0.02F},
      {0.0F, 1.98F, -0.50F},
      {0.0F, 1.94F, -1.04F},
      {0.0F, 1.76F, -1.52F},
      {0.0F, 1.48F, -1.92F},
      {0.0F, 1.12F, -2.20F},
  }};
  add_lobed_mass(primitives, tail, 0.52F, 0.24F, k_plume_slot, 0.45F);
}

void add_hannibal_crest(std::vector<Primitive>& primitives) {
  append_specs(primitives, std::span{k_hannibal_crest_primitives});

  for (int side = 0; side < 2; ++side) {
    float const s = (side == 0) ? -1.0F : 1.0F;
    std::array<QVector3D, 7> const ridge{{
        {s * 0.20F, 1.36F, 0.76F},
        {s * 0.21F, 1.96F, 0.40F},
        {s * 0.23F, 2.22F, -0.14F},
        {s * 0.26F, 2.26F, -0.70F},
        {s * 0.30F, 2.06F, -1.20F},
        {s * 0.34F, 1.74F, -1.62F},
        {s * 0.38F, 1.32F, -1.96F},
    }};
    add_lobed_mass(primitives, ridge, 0.46F, 0.24F, k_plume_slot, 0.46F);
  }
}

auto build_commander_helmet(CommanderHelmetStyle style,
                            std::string_view debug_name) -> RenderArchetype {
  std::vector<Primitive> primitives;
  primitives.reserve(96U);
  bool const roman = style == CommanderHelmetStyle::Fabius ||
                     style == CommanderHelmetStyle::Scipio ||
                     style == CommanderHelmetStyle::Marcellus;
  if (roman) {
    add_roman_base_helmet(primitives);
  } else {
    add_base_helmet(primitives, style == CommanderHelmetStyle::Hannibal);
  }
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
    return {QVector3D(0.36F, 0.38F, 0.40F),
            QVector3D(0.17F, 0.18F, 0.21F),
            QVector3D(0.78F, 0.60F, 0.31F),
            QVector3D(0.40F, 0.025F, 0.035F)};
  case CommanderHelmetStyle::Scipio:
    return {QVector3D(0.58F, 0.49F, 0.31F),
            QVector3D(0.20F, 0.16F, 0.105F),
            QVector3D(0.90F, 0.66F, 0.22F),
            QVector3D(0.54F, 0.025F, 0.05F)};
  case CommanderHelmetStyle::Marcellus:
    return {QVector3D(0.29F, 0.31F, 0.34F),
            QVector3D(0.14F, 0.15F, 0.18F),
            QVector3D(0.70F, 0.50F, 0.24F),
            QVector3D(0.62F, 0.045F, 0.025F)};
  case CommanderHelmetStyle::Hanno:
    return {QVector3D(0.48F, 0.39F, 0.24F),
            QVector3D(0.16F, 0.13F, 0.095F),
            QVector3D(0.84F, 0.58F, 0.20F),
            QVector3D(0.29F, 0.045F, 0.31F)};
  case CommanderHelmetStyle::Hasdrubal:
    return {QVector3D(0.36F, 0.31F, 0.22F),
            QVector3D(0.10F, 0.16F, 0.17F),
            QVector3D(0.32F, 0.52F, 0.44F),
            QVector3D(0.19F, 0.04F, 0.24F)};
  case CommanderHelmetStyle::Hannibal:
    return {QVector3D(0.38F, 0.31F, 0.22F),
            QVector3D(0.075F, 0.08F, 0.095F),
            QVector3D(0.78F, 0.48F, 0.14F),
            QVector3D(0.055F, 0.055F, 0.070F)};
  }
  return {QVector3D(0.52F, 0.55F, 0.58F),
          QVector3D(0.20F, 0.20F, 0.22F),
          QVector3D(0.62F, 0.42F, 0.16F),
          QVector3D(0.55F, 0.10F, 0.09F)};
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
      .uniform_scale = k_helmet_uniform_scale,
      .authored_local_offset = k_helmet_local_offset * k_helmet_uniform_scale,
      .bind_radius = k_head_socket_radius,
      .bind_socket_transform = bind_palette_socket_bone,
  });
  fill_sequential_role_remap(spec, base_role_byte, k_commander_helmet_role_count);
  return spec;
}

} // namespace Render::GL
