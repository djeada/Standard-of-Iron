#include "commander_regalia.h"

#include <string_view>
#include <vector>

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_spec.h"
#include "../attachment_builder.h"
#include "../generated_equipment.h"
#include "torso_local_archetype_utils.h"

namespace Render::GL {
namespace {

enum RegaliaPaletteSlot : std::uint8_t {
  k_cloth_slot = 0U,
  k_trim_slot = 1U,
};

using Primitive = GeneratedEquipmentPrimitive;

void add_fabius_regalia(std::vector<Primitive>& p) {
  p.push_back(generated_ellipsoid(
      {-0.34F, 0.02F, 0.0F}, {0.23F, 0.10F, 0.21F}, k_trim_slot, 1.0F, 2));
  p.push_back(generated_ellipsoid(
      {0.34F, 0.02F, 0.0F}, {0.23F, 0.10F, 0.21F}, k_trim_slot, 1.0F, 2));
  p.push_back(generated_cone(
      {-0.31F, -0.03F, -0.01F}, {-0.55F, -0.13F, -0.04F}, 0.11F, k_cloth_slot, 1.0F, 2));
  p.push_back(generated_cone(
      {0.31F, -0.03F, -0.01F}, {0.55F, -0.13F, -0.04F}, 0.11F, k_cloth_slot, 1.0F, 2));
  p.push_back(generated_cylinder(
      {-0.23F, -0.10F, 0.18F}, {0.23F, -0.10F, 0.18F}, 0.055F, k_trim_slot, 1.0F, 2));
}

void add_scipio_regalia(std::vector<Primitive>& p) {
  for (int side : {-1, 1}) {
    float const s = static_cast<float>(side);
    p.push_back(generated_cone({s * 0.27F, -0.01F, 0.02F},
                               {s * 0.53F, 0.07F, -0.02F},
                               0.11F,
                               k_trim_slot,
                               1.0F,
                               2));
    p.push_back(generated_cone({s * 0.30F, -0.07F, 0.02F},
                               {s * 0.50F, -0.20F, -0.02F},
                               0.09F,
                               k_cloth_slot,
                               1.0F,
                               2));
  }
}

void add_marcellus_regalia(std::vector<Primitive>& p) {
  p.push_back(generated_ellipsoid(
      {-0.31F, 0.06F, -0.01F}, {0.14F, 0.08F, 0.115F}, k_cloth_slot, 1.0F, 1));
  p.push_back(generated_sphere({-0.39F, 0.075F, 0.02F}, 0.065F, k_cloth_slot, 1.0F, 1));
  p.push_back(generated_cone(
      {-0.42F, 0.08F, 0.18F}, {-0.56F, -0.16F, 0.27F}, 0.075F, k_trim_slot, 1.0F, 2));
  p.push_back(generated_cone(
      {-0.28F, 0.06F, 0.20F}, {-0.34F, -0.20F, 0.29F}, 0.065F, k_trim_slot, 1.0F, 2));
  p.push_back(generated_cylinder(
      {-0.28F, 0.16F, 0.20F}, {0.26F, -0.22F, 0.24F}, 0.055F, k_cloth_slot, 1.0F, 1));
}

void add_hanno_regalia(std::vector<Primitive>& p) {
  p.push_back(generated_ellipsoid(
      {-0.39F, 0.14F, -0.01F}, {0.24F, 0.18F, 0.20F}, k_trim_slot, 1.0F, 2));
  p.push_back(generated_ellipsoid(
      {0.39F, 0.14F, -0.01F}, {0.24F, 0.18F, 0.20F}, k_trim_slot, 1.0F, 2));
  p.push_back(generated_sphere({-0.39F, 0.14F, 0.19F}, 0.08F, k_cloth_slot, 1.0F, 2));
  p.push_back(generated_sphere({0.39F, 0.14F, 0.19F}, 0.08F, k_cloth_slot, 1.0F, 2));
  p.push_back(generated_box(
      {0.0F, 0.08F, 0.25F}, {0.22F, 0.25F, 0.055F}, k_cloth_slot, 1.0F, 1));
  p.push_back(generated_cone(
      {0.0F, 0.18F, 0.30F}, {0.0F, -0.24F, 0.34F}, 0.13F, k_trim_slot, 1.0F, 2));
}

void add_hasdrubal_regalia(std::vector<Primitive>& p) {
  for (int side : {-1, 1}) {
    float const s = static_cast<float>(side);
    p.push_back(generated_cone({s * 0.24F, 0.16F, 0.0F},
                               {s * 0.59F, 0.30F, -0.15F},
                               0.12F,
                               k_trim_slot,
                               1.0F,
                               2));
    p.push_back(generated_cone({s * 0.25F, 0.10F, -0.04F},
                               {s * 0.51F, -0.04F, -0.18F},
                               0.09F,
                               k_cloth_slot,
                               1.0F,
                               1));
  }
}

void add_hannibal_regalia(std::vector<Primitive>& p) {
  for (int i = 0; i < 5; ++i) {
    float const t = static_cast<float>(i) / 4.0F;
    p.push_back(
        generated_sphere({-0.34F - 0.055F * t, -0.01F - 0.055F * t, -0.03F + 0.04F * t},
                         0.105F - 0.015F * t,
                         k_cloth_slot,
                         1.0F,
                         1));
  }
  p.push_back(generated_ellipsoid(
      {-0.35F, -0.02F, 0.02F}, {0.20F, 0.105F, 0.16F}, k_cloth_slot, 1.0F, 1));
  p.push_back(generated_cone(
      {-0.42F, 0.01F, 0.20F}, {-0.64F, -0.25F, 0.30F}, 0.075F, k_trim_slot, 1.0F, 2));
  p.push_back(generated_cone(
      {-0.30F, -0.04F, 0.22F}, {-0.39F, -0.30F, 0.31F}, 0.070F, k_trim_slot, 1.0F, 2));
  p.push_back(generated_ellipsoid(
      {0.31F, -0.04F, 0.0F}, {0.14F, 0.08F, 0.15F}, k_trim_slot, 1.0F, 2));
}

auto build_regalia(CommanderRegaliaStyle style,
                   std::string_view name) -> RenderArchetype {
  std::vector<Primitive> primitives;
  primitives.reserve(16U);
  switch (style) {
  case CommanderRegaliaStyle::Fabius:
    add_fabius_regalia(primitives);
    break;
  case CommanderRegaliaStyle::Scipio:
    add_scipio_regalia(primitives);
    break;
  case CommanderRegaliaStyle::Marcellus:
    add_marcellus_regalia(primitives);
    break;
  case CommanderRegaliaStyle::Hanno:
    add_hanno_regalia(primitives);
    break;
  case CommanderRegaliaStyle::Hasdrubal:
    add_hasdrubal_regalia(primitives);
    break;
  case CommanderRegaliaStyle::Hannibal:
    add_hannibal_regalia(primitives);
    break;
  }
  return build_generated_equipment_archetype(name, primitives);
}

} // namespace

auto commander_regalia_archetype(CommanderRegaliaStyle style)
    -> const RenderArchetype& {
  static const RenderArchetype fabius =
      build_regalia(CommanderRegaliaStyle::Fabius, "commander_regalia_fabius");
  static const RenderArchetype scipio =
      build_regalia(CommanderRegaliaStyle::Scipio, "commander_regalia_scipio");
  static const RenderArchetype marcellus =
      build_regalia(CommanderRegaliaStyle::Marcellus, "commander_regalia_marcellus");
  static const RenderArchetype hanno =
      build_regalia(CommanderRegaliaStyle::Hanno, "commander_regalia_hanno");
  static const RenderArchetype hasdrubal =
      build_regalia(CommanderRegaliaStyle::Hasdrubal, "commander_regalia_hasdrubal");
  static const RenderArchetype hannibal =
      build_regalia(CommanderRegaliaStyle::Hannibal, "commander_regalia_hannibal");
  switch (style) {
  case CommanderRegaliaStyle::Fabius:
    return fabius;
  case CommanderRegaliaStyle::Scipio:
    return scipio;
  case CommanderRegaliaStyle::Marcellus:
    return marcellus;
  case CommanderRegaliaStyle::Hanno:
    return hanno;
  case CommanderRegaliaStyle::Hasdrubal:
    return hasdrubal;
  case CommanderRegaliaStyle::Hannibal:
    return hannibal;
  }
  return fabius;
}

auto commander_regalia_make_static_attachment(CommanderRegaliaStyle style,
                                              std::uint16_t torso_socket_bone_index,
                                              std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  auto const& torso = Render::Humanoid::humanoid_bind_body_frames().torso;
  TorsoLocalFrame const torso_local = make_torso_local_frame(QMatrix4x4{}, torso);
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &commander_regalia_archetype(style),
      .socket_bone_index = torso_socket_bone_index,
      .unit_local_pose_at_bind = torso_local.world,
  });
  spec.palette_role_remap[k_cloth_slot] = base_role_byte;
  spec.palette_role_remap[k_trim_slot] = static_cast<std::uint8_t>(base_role_byte + 1U);
  return spec;
}

} // namespace Render::GL
