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

  for (int const side : {-1, 1}) {
    auto const s = static_cast<float>(side);
    for (int lame = 0; lame < 3; ++lame) {
      float const t = static_cast<float>(lame) / 2.0F;
      p.push_back(generated_box({s * (0.255F + 0.075F * t), 0.055F - 0.105F * t, 0.0F},
                                {0.085F + 0.020F * t, 0.028F, 0.175F - 0.030F * t},
                                k_trim_slot,
                                1.0F,
                                2));
    }
  }

  p.push_back(generated_box(
      {0.0F, 0.075F, 0.155F}, {0.155F, 0.040F, 0.045F}, k_trim_slot, 1.0F, 2));
  p.push_back(generated_box(
      {0.0F, -0.055F, 0.170F}, {0.075F, 0.062F, 0.030F}, k_trim_slot, 1.0F, 2));
}

void add_scipio_regalia(std::vector<Primitive>& p) {
  for (int const side : {-1, 1}) {
    auto const s = static_cast<float>(side);

    p.push_back(generated_box(
        {s * 0.285F, 0.045F, 0.0F}, {0.105F, 0.032F, 0.165F}, k_trim_slot, 1.0F, 2));
    p.push_back(generated_cone({s * 0.30F, -0.045F, 0.015F},
                               {s * 0.46F, -0.175F, -0.010F},
                               0.075F,
                               k_cloth_slot,
                               1.0F,
                               1));
  }

  p.push_back(generated_cylinder(
      {0.0F, 0.020F, 0.150F}, {0.0F, 0.020F, 0.185F}, 0.085F, k_trim_slot, 1.0F, 2));
  p.push_back(generated_cylinder(
      {0.0F, 0.020F, 0.180F}, {0.0F, 0.020F, 0.196F}, 0.045F, k_cloth_slot, 1.0F, 2));
}

void add_marcellus_regalia(std::vector<Primitive>& p) {
  p.push_back(generated_box(
      {-0.275F, 0.055F, 0.0F}, {0.095F, 0.028F, 0.140F}, k_trim_slot, 1.0F, 2));
  p.push_back(generated_ellipsoid(
      {-0.31F, 0.02F, -0.01F}, {0.115F, 0.070F, 0.105F}, k_cloth_slot, 1.0F, 1));
  p.push_back(generated_cone(
      {-0.42F, 0.08F, 0.18F}, {-0.56F, -0.16F, 0.27F}, 0.075F, k_trim_slot, 1.0F, 2));
  p.push_back(generated_cone(
      {-0.28F, 0.06F, 0.20F}, {-0.34F, -0.20F, 0.29F}, 0.065F, k_trim_slot, 1.0F, 2));
  p.push_back(generated_cylinder(
      {-0.28F, 0.16F, 0.20F}, {0.26F, -0.22F, 0.24F}, 0.055F, k_cloth_slot, 1.0F, 1));
}

void add_hanno_regalia(std::vector<Primitive>& p) {

  p.push_back(generated_ellipsoid(
      {0.285F, -0.235F, -0.065F}, {0.145F, 0.130F, 0.125F}, k_trim_slot, 1.0F, 1));
  p.push_back(
      generated_sphere({0.285F, -0.235F, 0.055F}, 0.055F, k_cloth_slot, 1.0F, 1));
  p.push_back(generated_cylinder({0.145F, 0.020F, 0.060F},
                                 {0.300F, -0.170F, -0.020F},
                                 0.024F,
                                 k_cloth_slot,
                                 1.0F,
                                 1));

  for (int const side : {-1, 1}) {
    auto const s = static_cast<float>(side);
    p.push_back(generated_box(
        {s * 0.255F, 0.055F, 0.0F}, {0.090F, 0.030F, 0.150F}, k_trim_slot, 1.0F, 2));
  }

  p.push_back(generated_cylinder({-0.055F, 0.090F, 0.150F},
                                 {-0.055F, 0.090F, 0.180F},
                                 0.062F,
                                 k_trim_slot,
                                 1.0F,
                                 2));
}

void add_hasdrubal_regalia(std::vector<Primitive>& p) {
  for (int const side : {-1, 1}) {
    auto const s = static_cast<float>(side);
    p.push_back(generated_cone({s * 0.235F, 0.105F, 0.0F},
                               {s * 0.415F, 0.185F, -0.130F},
                               0.085F,
                               k_trim_slot,
                               1.0F,
                               2));
    p.push_back(generated_cone({s * 0.245F, 0.055F, -0.030F},
                               {s * 0.400F, -0.060F, -0.150F},
                               0.070F,
                               k_cloth_slot,
                               1.0F,
                               1));
  }
}

void add_hannibal_regalia(std::vector<Primitive>& p) {

  for (int i = 0; i < 4; ++i) {
    float const t = static_cast<float>(i) / 3.0F;
    p.push_back(generated_sphere(
        {-0.300F - 0.048F * t, -0.030F - 0.078F * t, -0.020F + 0.030F * t},
        0.088F - 0.016F * t,
        k_cloth_slot,
        1.0F,
        1));
  }
  p.push_back(generated_ellipsoid(
      {-0.300F, 0.015F, 0.010F}, {0.150F, 0.085F, 0.140F}, k_cloth_slot, 1.0F, 1));
  p.push_back(generated_box(
      {0.270F, 0.055F, 0.0F}, {0.100F, 0.032F, 0.160F}, k_trim_slot, 1.0F, 2));
  p.push_back(generated_cylinder(
      {0.0F, 0.010F, 0.155F}, {0.0F, 0.010F, 0.188F}, 0.080F, k_trim_slot, 1.0F, 2));
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
