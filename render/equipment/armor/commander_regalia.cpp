#include "commander_regalia.h"

#include <cmath>
#include <numbers>
#include <string_view>
#include <vector>

#include "render/equipment/attachment_builder.h"
#include "render/equipment/generated_equipment.h"
#include "render/gl/shared_geometry_cache.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/runtime/humanoid_renderer.h"
#include "sheet_mesh.h"
#include "torso_local_archetype_utils.h"

namespace Render::GL {
namespace {

enum RegaliaPaletteSlot : std::uint8_t {
  k_cloth_slot = 0U,
  k_trim_slot = 1U,
};

using Primitive = GeneratedEquipmentPrimitive;

auto build_regalia(CommanderRegaliaStyle style,
                   std::string_view name) -> RenderArchetype {
  bool const roman = style == CommanderRegaliaStyle::Fabius ||
                     style == CommanderRegaliaStyle::Scipio ||
                     style == CommanderRegaliaStyle::Marcellus;
  bool const consul = style == CommanderRegaliaStyle::Scipio;
  bool const archer = style == CommanderRegaliaStyle::Marcellus ||
                      style == CommanderRegaliaStyle::Hasdrubal;
  auto const& frames = Render::Humanoid::humanoid_bind_body_frames();
  float const waist_y = frames.waist.origin.y() - frames.torso.origin.y();
  std::vector<Primitive> primitives;
  primitives.reserve(96U);

  primitives.push_back(generated_ellipsoid(
      {0.0F, waist_y + 0.025F, 0.0F}, {0.195F, 0.037F, 0.125F}, k_cloth_slot, 1.0F, 1));
  primitives.push_back(generated_box({0.0F, waist_y + 0.025F, 0.127F},
                                     {0.037F, 0.028F, 0.008F},
                                     k_trim_slot,
                                     1.0F,
                                     2));
  for (int side : {-1, 1}) {
    for (int i = 1; i <= 3; ++i) {
      float const x = static_cast<float>(side * i) * 0.044F;
      primitives.push_back(
          generated_sphere({x, waist_y + 0.025F, 0.128F - 0.15F * std::abs(x)},
                           0.008F,
                           k_trim_slot,
                           1.0F,
                           2));
    }
  }

  for (int side : {-1, 1}) {
    float const x = static_cast<float>(side);
    primitives.push_back(generated_cylinder({x * 0.13F, -0.025F, 0.115F},
                                            {x * 0.13F, -0.025F, 0.132F},
                                            consul ? 0.030F : 0.024F,
                                            k_trim_slot,
                                            1.0F,
                                            2));
    primitives.push_back(generated_cylinder({x * 0.05F, 0.012F, 0.11F},
                                            {x * 0.11F, -0.035F, 0.128F},
                                            0.008F,
                                            k_trim_slot,
                                            1.0F,
                                            2));
  }
  float const emblem_y = -0.145F;
  primitives.push_back(generated_cylinder({0.0F, emblem_y, 0.128F},
                                          {0.0F, emblem_y, 0.145F},
                                          consul ? 0.046F : 0.033F,
                                          k_trim_slot,
                                          1.0F,
                                          2));
  primitives.push_back(generated_cylinder({0.0F, emblem_y, 0.144F},
                                          {0.0F, emblem_y, 0.151F},
                                          0.021F,
                                          k_cloth_slot,
                                          1.0F,
                                          1));
  if (roman) {

    for (int side : {-1, 1}) {
      float const sign = static_cast<float>(side);
      for (int feather = 0; feather < 4; ++feather) {
        float const t = static_cast<float>(feather);
        primitives.push_back(generated_cone(
            {sign * (0.026F + t * 0.014F), emblem_y + 0.012F, 0.144F},
            {sign * (0.070F + t * 0.015F), emblem_y + 0.045F - t * 0.012F, 0.145F},
            0.011F,
            k_trim_slot,
            1.0F,
            2));
      }
      for (int leaf = 0; leaf < 4; ++leaf) {
        float const t = static_cast<float>(leaf);
        primitives.push_back(generated_ellipsoid(
            {sign * (0.032F + 0.013F * t), emblem_y - 0.055F + 0.008F * t, 0.145F},
            {0.010F, 0.018F, 0.004F},
            k_trim_slot,
            1.0F,
            2));
      }
    }
  } else {

    for (int side : {-1, 1}) {
      float const sign = static_cast<float>(side);
      primitives.push_back(generated_cylinder({0.0F, emblem_y - 0.038F, 0.143F},
                                              {sign * 0.047F, emblem_y - 0.11F, 0.13F},
                                              0.008F,
                                              k_trim_slot,
                                              1.0F,
                                              2));
    }
    primitives.push_back(generated_cylinder({-0.045F, emblem_y - 0.052F, 0.143F},
                                            {0.045F, emblem_y - 0.052F, 0.143F},
                                            0.008F,
                                            k_trim_slot,
                                            1.0F,
                                            2));
  }

  RenderArchetypeBuilder builder{std::string(name)};
  for (auto const& primitive : primitives) {
    add_generated_equipment_primitive(builder, primitive);
  }

  constexpr int strips = 18;
  float const length = archer ? 0.19F : (consul ? 0.27F : 0.23F);
  for (int strip = 0; strip < strips; ++strip) {
    auto const key = geometry_key("equipment/commander_pteruges",
                                  static_cast<std::uint64_t>(style) * strips + strip);
    auto* mesh = SharedGeometryCache::instance().get_or_build(key, [=] {
      SheetGrid grid;
      grid.columns = 3;
      grid.rows = 4;
      for (int row = 0; row < grid.rows; ++row) {
        float const v = static_cast<float>(row) / 3.0F;
        for (int col = 0; col < grid.columns; ++col) {
          float const u = static_cast<float>(col) / 2.0F;
          float const angle = (static_cast<float>(strip) + 0.10F + 0.80F * u) * 2.0F *
                              std::numbers::pi_v<float> / strips;
          float const hem = row == 3 ? 0.018F * std::abs(2.0F * u - 1.0F) : 0.0F;
          grid.positions.emplace_back((0.19F + 0.035F * v) * std::sin(angle),
                                      waist_y - length * v + hem,
                                      (0.12F + 0.045F * v) * std::cos(angle));
          grid.uvs.emplace_back(u, v);
        }
      }
      return make_thick_sheet_mesh(grid, false, 0.006F);
    });
    builder.add_palette_mesh(mesh, QMatrix4x4{}, k_cloth_slot, nullptr, 1.0F, 1);
    float const angle =
        (static_cast<float>(strip) + 0.5F) * 2.0F * std::numbers::pi_v<float> / strips;
    add_generated_equipment_primitive(builder,
                                      generated_sphere({0.222F * std::sin(angle),
                                                        waist_y - length + 0.028F,
                                                        0.163F * std::cos(angle)},
                                                       0.009F,
                                                       k_trim_slot,
                                                       1.0F,
                                                       2));
  }
  return std::move(builder).build();
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
                                              std::uint8_t leather_role_byte,
                                              std::uint8_t metal_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  auto const& torso = Render::Humanoid::humanoid_bind_body_frames().torso;
  TorsoLocalFrame const torso_local = make_torso_local_frame(QMatrix4x4{}, torso);
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &commander_regalia_archetype(style),
      .socket_bone_index = torso_socket_bone_index,
      .unit_local_pose_at_bind = torso_local.world,
  });
  spec.palette_role_remap[k_cloth_slot] = leather_role_byte;
  spec.palette_role_remap[k_trim_slot] = metal_role_byte;
  auto const& frames = Render::Humanoid::humanoid_bind_body_frames();
  float const waist_y = frames.waist.origin.y();
  fit_armor_to_waist(spec, waist_y, waist_y - 0.27F);
  spec.drape.leg_l_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HipL);
  spec.drape.leg_r_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HipR);
  spec.drape.leg_share = 0.35F;
  spec.drape.leg_crossfade_half_width = 0.12F;
  return spec;
}

} // namespace Render::GL
