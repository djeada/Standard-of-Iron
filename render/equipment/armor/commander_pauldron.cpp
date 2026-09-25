#include "commander_pauldron.h"

#include <array>
#include <cmath>
#include <numbers>

#include "render/equipment/attachment_builder.h"
#include "render/gl/shared_geometry_cache.h"
#include "sheet_mesh.h"

namespace Render::GL {

namespace {

constexpr std::uint8_t k_plate_slot = 0U;
constexpr std::uint8_t k_strap_slot = 1U;

constexpr float k_deg = std::numbers::pi_v<float> / 180.0F;

// One lame of the guard, in the shoulder frame (x out along the shoulder
// line, y up, z across the shoulder). `from_deg`/`to_deg` sweep over the
// shoulder from the neck side (negative) down the outside of the arm; the
// lame wraps front and back of the shoulder by `wrap_deg`.
struct Lame {
  float from_deg;
  float to_deg;
  float radius;
  float flare;
  float wrap_deg;
};

// The body's deltoid is an ellipsoid of about 0.079 x 0.066 x 0.076 m radii
// on the shoulder bone. The top lame clears it, and each lame below sits a
// little further out so it laps over the next like shingles.
// Metal lames alternate with a narrow dark leather band where they overlap,
// which is what makes the guard read as segmented plate at game distance
// instead of one pale cap.
struct LameSpec {
  Lame lame;
  std::uint8_t slot;
};
constexpr std::array<LameSpec, 5> k_lames{{
    {{-34.0F, 22.0F, 0.087F, 0.004F, 76.0F}, k_plate_slot},
    {{18.0F, 30.0F, 0.090F, 0.001F, 74.0F}, k_strap_slot},
    {{26.0F, 70.0F, 0.093F, 0.006F, 72.0F}, k_plate_slot},
    {{66.0F, 78.0F, 0.098F, 0.001F, 70.0F}, k_strap_slot},
    {{74.0F, 124.0F, 0.101F, 0.014F, 68.0F}, k_plate_slot},
}};

constexpr float k_lame_thickness = 0.0045F;
constexpr float k_vertical_squash = 0.92F;

auto make_lame_mesh(const Lame& lame) -> std::unique_ptr<Mesh> {
  constexpr int k_columns = 13;
  constexpr int k_rows = 6;
  SheetGrid grid;
  grid.columns = k_columns;
  grid.rows = k_rows;
  for (int row = 0; row < k_rows; ++row) {
    float const v = static_cast<float>(row) / static_cast<float>(k_rows - 1);
    float const phi = (lame.from_deg + (lame.to_deg - lame.from_deg) * v) * k_deg;
    // A rolled lower edge on each lame.
    float const r = lame.radius + lame.flare * v * v;
    for (int col = 0; col < k_columns; ++col) {
      float const u = static_cast<float>(col) / static_cast<float>(k_columns - 1);
      float const psi = (u * 2.0F - 1.0F) * lame.wrap_deg * k_deg;
      float const ring = r * std::cos(psi);
      grid.positions.emplace_back(ring * std::sin(phi),
                                  ring * std::cos(phi) * k_vertical_squash,
                                  r * std::sin(psi));
      grid.uvs.emplace_back(u, v);
    }
  }
  // Columns run -z to +z and rows sweep from the top of the shoulder out
  // along +x, so cross(du, dv) already points away from the shoulder.
  return make_thick_sheet_mesh(grid, false, k_lame_thickness);
}

auto lame_mesh(std::size_t index) -> Mesh* {
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("equipment/commander_pauldron/lame_v2", index),
      [index] { return make_lame_mesh(k_lames[index].lame); });
}

auto build_archetype() -> RenderArchetype {
  RenderArchetypeBuilder builder{"commander_pauldron"};
  for (std::size_t i = 0; i < k_lames.size(); ++i) {
    builder.add_palette_mesh(
        lame_mesh(i), QMatrix4x4{}, k_lames[i].slot, nullptr, 1.0F, 2);
  }
  return std::move(builder).build();
}

} // namespace

auto commander_pauldron_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = build_archetype();
  return archetype;
}

auto commander_pauldron_make_static_attachment(std::uint16_t shoulder_bone_index,
                                               std::uint8_t metal_role_byte,
                                               std::uint8_t strap_role_byte,
                                               const QMatrix4x4& bind_shoulder_frame)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &commander_pauldron_archetype(),
      .socket_bone_index = shoulder_bone_index,
      .unit_local_pose_at_bind = bind_shoulder_frame,
  });
  spec.palette_role_remap[k_plate_slot] = metal_role_byte;
  spec.palette_role_remap[k_strap_slot] = strap_role_byte;
  return spec;
}

} // namespace Render::GL
