#include "roman_greaves.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <memory>
#include <string>

#include "animation/rig/humanoid_proportions.h"
#include "render/entity/registry.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/equipment_submit.h"
#include "render/gl/shared_geometry_cache.h"
#include "render/humanoid/runtime/style_palette.h"
#include "render/render_archetype.h"
#include "sheet_mesh.h"

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

enum RomanGreavesPaletteSlot : std::uint8_t {
  k_greaves_slot = 0U,
};

// The greave is a curved bronze shell that follows the calf it covers. The
// body's lower leg is two tapered cylinders hung off the knee bone: 1.30R at
// the knee, swelling to 1.46R a third of the way down, then narrowing to
// 0.78R at the ankle (R = LOWER_LEG_R). A greave built at one constant radius
// is buried in the calf swell and floats off the ankle, so the leg read as
// skin and plate edges fighting each other. The shell below takes the calf
// radius at every height and stands just proud of it.
constexpr float k_calf_knee_r = 1.30F;
constexpr float k_calf_swell_r = 1.46F;
constexpr float k_calf_ankle_r = 0.78F;
constexpr float k_calf_swell_at = 0.30F;

constexpr float k_greave_top = 0.07F;
constexpr float k_greave_bottom = 0.88F;
constexpr float k_greave_half_arc = 2.05F;
constexpr float k_greave_clearance = 1.05F;
constexpr float k_greave_gap = 0.0040F;
constexpr float k_greave_thickness = 0.0035F;
constexpr int k_greave_rings = 9;
constexpr int k_greave_columns = 11;

auto make_leg_attachment_transform(const QMatrix4x4& parent,
                                   const AttachmentFrame& shin) -> QMatrix4x4 {
  QMatrix4x4 local;
  local.setColumn(0, QVector4D(shin.right, 0.0F));
  local.setColumn(1, QVector4D(shin.up, 0.0F));
  local.setColumn(2, QVector4D(shin.forward, 0.0F));
  local.setColumn(3, QVector4D(shin.origin, 1.0F));
  return parent * local;
}

auto calf_radius_at(float along) -> float {
  using HP = HumanProportions;
  float const swell_s = HP::LOWER_LEG_LEN * k_calf_swell_at;
  if (along <= swell_s) {
    float const t = along / swell_s;
    return HP::LOWER_LEG_R * (k_calf_knee_r + (k_calf_swell_r - k_calf_knee_r) * t);
  }
  float const t =
      std::clamp((along - swell_s) / (HP::LOWER_LEG_LEN - swell_s), 0.0F, 1.0F);
  return HP::LOWER_LEG_R * (k_calf_swell_r + (k_calf_ankle_r - k_calf_swell_r) * t);
}

// Shin-frame shell: origin at the ankle, +Y up the shin to the knee, +Z to
// the front of the leg. Built for the reference LOWER_LEG_R.
auto make_greave_shell_mesh() -> std::unique_ptr<Mesh> {
  using HP = HumanProportions;
  SheetGrid grid;
  grid.columns = k_greave_columns;
  grid.rows = k_greave_rings;
  grid.positions.reserve(static_cast<std::size_t>(grid.columns * grid.rows));
  grid.uvs.reserve(grid.positions.capacity());
  for (int row = 0; row < grid.rows; ++row) {
    float const v = static_cast<float>(row) / static_cast<float>(grid.rows - 1);
    float const along =
        HP::LOWER_LEG_LEN * (k_greave_top + (k_greave_bottom - k_greave_top) * v);
    float const height = HP::LOWER_LEG_LEN - along;
    // Rolled lip at the top and a slight flare over the instep.
    float const top_lip = 0.0035F * std::exp(-v * v / 0.006F);
    float const ankle_flare = 0.0030F * std::pow(v, 6.0F);
    float const base_r = calf_radius_at(along) * k_greave_clearance + k_greave_gap;
    for (int col = 0; col < grid.columns; ++col) {
      float const u = static_cast<float>(col) / static_cast<float>(grid.columns - 1);
      float const angle = (u * 2.0F - 1.0F) * k_greave_half_arc;
      // Anatomical greaves carry a shallow ridge down the shin bone.
      float const ridge = 0.0030F * std::exp(-(angle * angle) / 0.10F);
      float const r = base_r + top_lip + ankle_flare + ridge;
      grid.positions.emplace_back(r * std::sin(angle), height, r * std::cos(angle));
      grid.uvs.emplace_back(u, v);
    }
  }
  // Columns sweep from the leg's left to right around the front and rows run
  // down the shin, so cross(du, dv) points into the leg: flip it outward.
  return make_thick_sheet_mesh(grid, true, k_greave_thickness);
}

auto greave_shell_mesh() -> Mesh* {
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("equipment/greaves/anatomical_shell"),
      [] { return make_greave_shell_mesh(); });
}

auto roman_greaves_archetype(float shin_radius) -> const RenderArchetype& {
  using HP = HumanProportions;
  struct CachedArchetype {
    int key{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const key = std::lround(shin_radius * 1000.0F);
  for (const auto& entry : cache) {
    if (entry.key == key) {
      return entry.archetype;
    }
  }

  float const radial = shin_radius / HP::LOWER_LEG_R;
  QMatrix4x4 local;
  local.scale(radial, 1.0F, radial);

  RenderArchetypeBuilder builder{"roman_greaves_" + std::to_string(key)};
  builder.add_palette_mesh(
      greave_shell_mesh(), local, k_greaves_slot, nullptr, 1.0F, 5);

  cache.push_back({key, std::move(builder).build()});
  return cache.back().archetype;
}

} // namespace

void RomanGreavesRenderer::render(const DrawContext& ctx,
                                  const BodyFrames& frames,
                                  const HumanoidPalette& palette,
                                  const HumanoidAnimationContext& anim,
                                  EquipmentBatch& batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void RomanGreavesRenderer::submit(const RomanGreavesConfig&,
                                  const DrawContext& ctx,
                                  const BodyFrames& frames,
                                  const HumanoidPalette& palette,
                                  const HumanoidAnimationContext& anim,
                                  EquipmentBatch& batch) {
  (void)anim;

  QVector3D const greaves_color =
      saturate_color(palette.metal * QVector3D(0.95F, 0.88F, 0.68F));
  std::array<QVector3D, 1> const palette_slots{greaves_color};

  auto render_greave = [&](const AttachmentFrame& shin) {
    if (shin.radius <= 0.0F) {
      return;
    }

    append_equipment_archetype(batch,
                               roman_greaves_archetype(shin.radius),
                               make_leg_attachment_transform(ctx.model, shin),
                               palette_slots);
  };

  render_greave(frames.shin_l);
  render_greave(frames.shin_r);
}

auto roman_greaves_archetype() -> const RenderArchetype& {
  using HP = HumanProportions;
  return roman_greaves_archetype(HP::LOWER_LEG_R);
}

auto roman_greaves_fill_role_colors(const HumanoidPalette& palette,
                                    QVector3D* out,
                                    std::size_t max) -> std::uint32_t {
  if (max < k_roman_greaves_role_count) {
    return 0;
  }
  out[0] = saturate_color(palette.metal * QVector3D(0.74F, 0.79F, 0.86F));
  return k_roman_greaves_role_count;
}

auto roman_greaves_make_static_attachment(std::uint16_t socket_bone_index,
                                          std::uint8_t base_role_byte,
                                          const QMatrix4x4& bind_shin_frame)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &roman_greaves_archetype(),
      .socket_bone_index = socket_bone_index,
      .unit_local_pose_at_bind = bind_shin_frame,
  });
  spec.palette_role_remap[k_greaves_slot] = base_role_byte;
  return spec;
}

} // namespace Render::GL
