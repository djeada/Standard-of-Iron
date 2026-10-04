#include "allied_garments.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>

#include "animation/rig/humanoid_proportions.h"
#include "render/equipment/attachment_builder.h"
#include "render/geom/parts.h"
#include "render/gl/primitives.h"
#include "render/gl/shared_geometry_cache.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/runtime/style_palette.h"
#include "render/humanoid/schema/skeleton_schema.h"
#include "sheet_mesh.h"

namespace Render::GL {

using Render::Geom::oriented_cylinder;
using Render::GL::Humanoid::saturate_color;
using Render::Humanoid::HumanoidBone;

namespace {

constexpr float k_calf_knee_r = 1.30F;
constexpr float k_calf_swell_r = 1.46F;
constexpr float k_calf_ankle_r = 0.78F;
constexpr float k_calf_swell_at = 0.30F;

constexpr float k_braccae_top = 0.0F;
constexpr float k_braccae_bottom = 0.90F;
constexpr float k_braccae_clearance = 1.07F;
constexpr float k_braccae_gap = 0.0030F;
constexpr float k_braccae_thickness = 0.0030F;
constexpr int k_braccae_rings = 9;
constexpr int k_braccae_columns = 14;

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

auto make_braccae_mesh() -> std::unique_ptr<Mesh> {
  using HP = HumanProportions;
  SheetGrid grid;
  grid.columns = k_braccae_columns;
  grid.rows = k_braccae_rings;
  grid.positions.reserve(static_cast<std::size_t>(grid.columns) *
                         static_cast<std::size_t>(grid.rows));
  grid.uvs.reserve(grid.positions.capacity());
  for (int row = 0; row < grid.rows; ++row) {
    float const v = static_cast<float>(row) / static_cast<float>(grid.rows - 1);
    float const along =
        HP::LOWER_LEG_LEN * (k_braccae_top + (k_braccae_bottom - k_braccae_top) * v);
    float const height = HP::LOWER_LEG_LEN - along;

    float const ankle_bunch = 0.0040F * std::pow(v, 4.0F);
    float const r = calf_radius_at(along) * k_braccae_clearance + k_braccae_gap +
                    ankle_bunch;
    for (int col = 0; col < grid.columns; ++col) {
      float const u = static_cast<float>(col) / static_cast<float>(grid.columns - 1);
      float const angle = u * 2.0F * std::numbers::pi_v<float>;
      grid.positions.emplace_back(r * std::sin(angle), height, r * std::cos(angle));
      grid.uvs.emplace_back(u, v);
    }
  }
  return make_thick_sheet_mesh(grid, true, k_braccae_thickness);
}

auto braccae_mesh() -> Mesh* {
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("equipment/garments/gallic_braccae"),
      [] { return make_braccae_mesh(); });
}

enum IberianTunicSlot : std::uint8_t {
  k_trim_slot = 0U,
  k_belt_slot = 1U,
  k_plaque_slot = 2U,
  k_skirt_slot = 3U,
};

auto bind_position(HumanoidBone bone) -> QVector3D {
  auto const palette = Render::Humanoid::humanoid_bind_palette();
  return palette[static_cast<std::size_t>(bone)].column(3).toVector3D();
}

struct TunicLayout {
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};
  QVector3D waist;
  QVector3D hem;
  QVector3D collar;
  float skirt_right{0.0F};
  float skirt_forward{0.0F};
  float waist_right{0.0F};
  float waist_forward{0.0F};
  float collar_right{0.0F};
  float collar_forward{0.0F};
};

auto tunic_layout() -> const TunicLayout& {
  static const TunicLayout layout = [] {
    using HP = HumanProportions;
    TunicLayout l;
    QVector3D const pelvis = bind_position(HumanoidBone::Pelvis);
    QVector3D const hip_l = bind_position(HumanoidBone::HipL);
    QVector3D const hip_r = bind_position(HumanoidBone::HipR);
    QVector3D const knee_l = bind_position(HumanoidBone::KneeL);
    QVector3D const neck = bind_position(HumanoidBone::Neck);
    QVector3D const chest = bind_position(HumanoidBone::Chest);

    float const hip_half = 0.5F * std::abs(hip_l.x() - hip_r.x());
    float const thigh_len = std::max(0.05F, hip_l.y() - knee_l.y());

    l.waist = QVector3D(pelvis.x(), pelvis.y() + HP::TORSO_BOT_R * 0.42F, pelvis.z());
    l.hem = QVector3D(pelvis.x(), hip_l.y() - thigh_len * 0.42F, pelvis.z());
    l.skirt_right = hip_half + HP::UPPER_LEG_R * 1.42F;
    l.skirt_forward = HP::TORSO_BOT_R * 0.66F;
    l.waist_right = HP::TORSO_BOT_R * 1.00F;
    l.waist_forward = HP::TORSO_BOT_R * 0.64F;

    float const collar_y = neck.y() - (neck.y() - chest.y()) * 0.10F;
    l.collar = QVector3D(neck.x(), collar_y, neck.z());
    l.collar_right = HP::NECK_RADIUS * 1.55F;
    l.collar_forward = HP::NECK_RADIUS * 1.40F;
    return l;
  }();
  return layout;
}

auto pelvis_tunic_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    TunicLayout const& l = tunic_layout();
    RenderArchetypeBuilder builder{"iberian_tunic_skirt"};

    QVector3D const skirt_top = l.waist - l.up * 0.01F;
    builder.add_palette_mesh(get_unit_cylinder(),
                             oriented_cylinder(skirt_top,
                                               l.hem,
                                               l.right,
                                               l.skirt_right,
                                               l.skirt_forward),
                             k_skirt_slot,
                             nullptr,
                             1.0F,
                             0);

    float const band = 0.030F;
    builder.add_palette_mesh(get_unit_cylinder(),
                             oriented_cylinder(l.hem + l.up * band,
                                               l.hem - l.up * 0.006F,
                                               l.right,
                                               l.skirt_right * 1.035F,
                                               l.skirt_forward * 1.06F),
                             k_trim_slot,
                             nullptr,
                             1.0F,
                             0);

    float const belt_half = 0.026F;
    builder.add_palette_mesh(get_unit_cylinder(),
                             oriented_cylinder(l.waist + l.up * belt_half,
                                               l.waist - l.up * belt_half,
                                               l.right,
                                               std::max(l.waist_right, l.skirt_right) *
                                                   1.05F,
                                               std::max(l.waist_forward,
                                                        l.skirt_forward) *
                                                   1.10F),
                             k_belt_slot,
                             nullptr,
                             1.0F,
                             0);

    QMatrix4x4 plaque;
    plaque.translate(l.waist +
                     l.forward * (std::max(l.waist_forward, l.skirt_forward) * 1.10F));
    plaque.scale(0.034F, 0.030F, 0.010F);
    builder.add_palette_mesh(
        get_unit_sphere(), plaque, k_plaque_slot, nullptr, 1.0F, 2);
    return std::move(builder).build();
  }();
  return archetype;
}

auto collar_tunic_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    TunicLayout const& l = tunic_layout();
    RenderArchetypeBuilder builder{"iberian_tunic_collar"};
    builder.add_palette_mesh(get_unit_cylinder(),
                             oriented_cylinder(l.collar + l.up * 0.012F,
                                               l.collar - l.up * 0.014F,
                                               l.right,
                                               l.collar_right,
                                               l.collar_forward),
                             k_trim_slot,
                             nullptr,
                             1.0F,
                             0);
    return std::move(builder).build();
  }();
  return archetype;
}

void remap_tunic_roles(Render::Creature::StaticAttachmentSpec& spec,
                       std::uint8_t base_role_byte,
                       std::uint8_t cloth_role_byte) {
  spec.palette_role_remap[k_trim_slot] = base_role_byte;
  spec.palette_role_remap[k_belt_slot] = static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_plaque_slot] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  spec.palette_role_remap[k_skirt_slot] = cloth_role_byte;
}

} // namespace

auto gallic_braccae_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder{"gallic_braccae"};
    builder.add_palette_mesh(braccae_mesh(), QMatrix4x4{}, 0U, nullptr, 1.0F, 0);
    return std::move(builder).build();
  }();
  return archetype;
}

auto gallic_braccae_make_static_attachment(std::uint16_t socket_bone_index,
                                           std::uint8_t cloth_role_byte,
                                           const QMatrix4x4& bind_shin_frame)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &gallic_braccae_archetype(),
      .socket_bone_index = socket_bone_index,
      .unit_local_pose_at_bind = bind_shin_frame,
  });
  spec.palette_role_remap[0] = cloth_role_byte;
  return spec;
}

auto iberian_tunic_fill_role_colors(const HumanoidPalette& palette,
                                    QVector3D* out,
                                    std::size_t max) -> std::uint32_t {
  if (max < k_iberian_tunic_role_count) {
    return 0;
  }
  out[k_trim_slot] = QVector3D(0.66F, 0.08F, 0.09F);
  out[k_belt_slot] = saturate_color(palette.leather_dark * 0.85F);
  out[k_plaque_slot] = QVector3D(0.80F, 0.62F, 0.32F);
  return k_iberian_tunic_role_count;
}

auto iberian_tunic_make_static_attachments(std::uint8_t base_role_byte,
                                           std::uint8_t cloth_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 2> {
  using Render::Humanoid::HumanoidBone;
  auto skirt = Render::Equipment::build_static_attachment({
      .archetype = &pelvis_tunic_archetype(),
      .socket_bone_index = static_cast<std::uint16_t>(HumanoidBone::Pelvis),
  });
  TunicLayout const& l = tunic_layout();
  skirt.drape.enabled = true;
  skirt.drape.pelvis_bone = static_cast<std::uint16_t>(HumanoidBone::Pelvis);
  skirt.drape.leg_l_bone = static_cast<std::uint16_t>(HumanoidBone::HipL);
  skirt.drape.leg_r_bone = static_cast<std::uint16_t>(HumanoidBone::HipR);
  skirt.drape.top_y = l.waist.y() - 0.04F;
  skirt.drape.bottom_y = l.hem.y();
  skirt.drape.leg_share = 0.85F;
  skirt.drape.leg_crossfade_half_width = std::max(0.02F, l.skirt_right * 0.45F);
  remap_tunic_roles(skirt, base_role_byte, cloth_role_byte);

  auto collar = Render::Equipment::build_static_attachment({
      .archetype = &collar_tunic_archetype(),
      .socket_bone_index = static_cast<std::uint16_t>(HumanoidBone::Chest),
  });
  remap_tunic_roles(collar, base_role_byte, cloth_role_byte);
  return {skirt, collar};
}

} // namespace Render::GL
