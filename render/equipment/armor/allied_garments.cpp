#include "allied_garments.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "animation/rig/humanoid_proportions.h"
#include "render/equipment/attachment_builder.h"
#include "render/geom/parts.h"
#include "render/geom/transforms.h"
#include "render/gl/primitives.h"
#include "render/gl/shared_geometry_cache.h"
#include "render/humanoid/asset/humanoid_derived_meshes.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/asset/mesh_helpers.h"
#include "render/humanoid/runtime/style_palette.h"
#include "render/humanoid/schema/skeleton_schema.h"
#include "sheet_mesh.h"
#include "torso_local_archetype_utils.h"

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::Geom::oriented_cylinder;
using Render::Geom::sphere_at;
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
    float const r =
        calf_radius_at(along) * k_braccae_clearance + k_braccae_gap + ankle_bunch;
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

enum TunicSlot : std::uint8_t {
  k_tunic_slot = 0U,
  k_accent_slot = 1U,
  k_belt_slot = 2U,
  k_ornament_slot = 3U,
};

struct TunicFit {
  TorsoLocalFrame local;
  QVector3D up;
  QVector3D right;
  QVector3D forward;
  QVector3D top;
  QVector3D bottom;
  float torso_r{0.0F};
  float torso_depth{0.0F};
  float waist_y{0.0F};
};

auto tunic_fit() -> const TunicFit& {
  static const TunicFit fit = [] {
    const auto& bind = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame& torso = bind.torso;
    const AttachmentFrame& waist = bind.waist;
    const AttachmentFrame& head = bind.head;
    TunicFit f;
    f.local = make_torso_local_frame(QMatrix4x4{}, torso);
    f.up = safe_attachment_axis(torso.up, QVector3D(0.0F, 1.0F, 0.0F));
    f.right = safe_attachment_axis(torso.right, QVector3D(1.0F, 0.0F, 0.0F));
    f.forward = safe_attachment_axis(torso.forward, QVector3D(0.0F, 0.0F, 1.0F));
    f.torso_r = torso.radius;
    f.torso_depth = torso.depth > 0.0F ? torso.depth : torso.radius * 0.75F;
    float const waist_r = waist.radius > 0.0F ? waist.radius : torso.radius * 0.85F;
    float const head_r = head.radius > 0.0F ? head.radius : torso.radius * 0.6F;
    QVector3D const head_up = safe_attachment_axis(head.up, f.up);
    QVector3D const waist_up = safe_attachment_axis(waist.up, f.up);

    f.top = torso.origin + f.up * (f.torso_r * 0.50F);
    QVector3D const head_guard = head.origin - head_up * (head_r * 1.45F);
    if (QVector3D::dotProduct(f.top - head_guard, f.up) > 0.0F) {
      f.top = head_guard - f.up * (f.torso_r * 0.05F);
    }
    f.bottom = waist.origin - waist_up * (waist_r * 0.36F);
    f.top += f.forward * (f.torso_r * 0.010F);
    f.bottom += f.forward * (f.torso_r * 0.010F);
    f.waist_y = waist.origin.y();

    return f;
  }();
  return fit;
}

auto torso_mesh() -> Mesh* {
  Mesh* mesh = Render::Humanoid::humanoid_mesh_part(
      Render::Humanoid::HumanoidMeshPart::TorsoNoBottomCap);
  return mesh != nullptr ? mesh : get_unit_torso();
}

auto torso_layer(const TunicFit& f,
                 const QVector3D& top,
                 const QVector3D& bottom,
                 float radius_scale,
                 float depth_scale) -> QMatrix4x4 {
  float const radius = f.torso_r * radius_scale;
  float const depth = f.torso_depth * depth_scale;
  QMatrix4x4 m = oriented_cylinder(f.local.point(top),
                                   f.local.point(bottom),
                                   f.local.direction(f.right),
                                   radius,
                                   radius * std::max(0.15F, depth / radius));
  align_torso_mesh_forward(m);
  return m;
}

auto ring_at(const TunicFit& f,
             const QVector3D& centre,
             float half_height,
             float radius_scale,
             float depth_scale) -> QMatrix4x4 {
  return oriented_cylinder(f.local.point(centre + f.up * half_height),
                           f.local.point(centre - f.up * half_height),
                           f.local.direction(f.right),
                           f.torso_r * radius_scale,
                           f.torso_depth * depth_scale);
}

constexpr float k_tunic_radius = 1.34F;
constexpr float k_tunic_depth = 1.22F;

void add_tunic_body(RenderArchetypeBuilder& builder, const TunicFit& f) {
  builder.add_palette_mesh(
      torso_mesh(),
      torso_layer(f, f.top, f.bottom, k_tunic_radius, k_tunic_depth),
      k_tunic_slot,
      nullptr,
      1.0F,
      1);
}

void add_belt(RenderArchetypeBuilder& builder, const TunicFit& f, float half_height) {
  QVector3D const belt = f.bottom + f.up * (half_height + 0.006F);
  builder.add_palette_mesh(
      get_unit_cylinder(),
      ring_at(f, belt, half_height, k_tunic_radius * 1.04F, k_tunic_depth * 1.06F),
      k_belt_slot,
      nullptr,
      1.0F,
      1);
}

auto woven_bands_mesh() -> Mesh* {

  Mesh const* source = torso_mesh();
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("equipment/garments/gallic_woven_bands"), [source] {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        auto clip = [](const std::vector<Vertex>& polygon, float y, bool above) {
          std::vector<Vertex> result;
          if (polygon.empty()) {
            return result;
          }
          Vertex previous = polygon.back();
          bool previous_inside =
              above ? previous.position[1] >= y : previous.position[1] <= y;
          for (Vertex const& current : polygon) {
            bool const inside =
                above ? current.position[1] >= y : current.position[1] <= y;
            if (inside != previous_inside) {
              float const t = (y - previous.position[1]) /
                              (current.position[1] - previous.position[1]);
              Vertex edge{};
              for (std::size_t axis = 0; axis < 3; ++axis) {
                edge.position[axis] =
                    std::lerp(previous.position[axis], current.position[axis], t);
                edge.normal[axis] =
                    std::lerp(previous.normal[axis], current.normal[axis], t);
              }
              for (std::size_t axis = 0; axis < 2; ++axis) {
                edge.tex_coord[axis] =
                    std::lerp(previous.tex_coord[axis], current.tex_coord[axis], t);
              }
              result.push_back(edge);
            }
            if (inside) {
              result.push_back(current);
            }
            previous = current;
            previous_inside = inside;
          }
          return result;
        };
        auto const& source_vertices = source->get_vertices();
        auto const& source_indices = source->get_indices();
        for (float centre : {-0.30F, -0.08F, 0.14F, 0.36F}) {
          for (std::size_t i = 0; i + 2 < source_indices.size(); i += 3) {
            std::vector<Vertex> polygon{source_vertices[source_indices[i]],
                                        source_vertices[source_indices[i + 1]],
                                        source_vertices[source_indices[i + 2]]};
            polygon = clip(polygon, centre - 0.026F, true);
            polygon = clip(polygon, centre + 0.026F, false);
            if (polygon.size() < 3) {
              continue;
            }
            auto const base = static_cast<unsigned int>(vertices.size());
            vertices.insert(vertices.end(), polygon.begin(), polygon.end());
            for (unsigned int j = 1; j + 1 < polygon.size(); ++j) {
              indices.insert(indices.end(), {base, base + j, base + j + 1});
            }
          }
        }
        return std::make_unique<Mesh>(vertices, indices);
      });
}

auto gallic_tunic_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    TunicFit const& f = tunic_fit();
    RenderArchetypeBuilder builder{"gallic_tunic"};
    add_tunic_body(builder, f);

    builder.add_palette_mesh(
        woven_bands_mesh(),
        torso_layer(
            f, f.top, f.bottom, k_tunic_radius * 1.012F, k_tunic_depth * 1.012F),
        k_accent_slot,
        nullptr,
        1.0F,
        1);
    add_belt(builder, f, 0.020F);

    QVector3D const neck = f.local.point(f.top + f.up * 0.025F);
    constexpr int k_torc_segments = 12;
    constexpr float k_gap = 0.48F;
    auto torc_point = [&](float angle) {
      return neck + QVector3D(0.075F * std::sin(angle), 0.0F, 0.065F * std::cos(angle));
    };
    for (int i = 0; i < k_torc_segments; ++i) {
      float const a0 = k_gap + (2.0F * std::numbers::pi_v<float> - 2.0F * k_gap) *
                                   static_cast<float>(i) / k_torc_segments;
      float const a1 = k_gap + (2.0F * std::numbers::pi_v<float> - 2.0F * k_gap) *
                                   static_cast<float>(i + 1) / k_torc_segments;
      builder.add_palette_mesh(get_unit_cylinder(),
                               cylinder_between(torc_point(a0), torc_point(a1), 0.008F),
                               k_ornament_slot,
                               nullptr,
                               1.0F,
                               3);
    }
    for (float angle : {k_gap, -k_gap}) {
      builder.add_palette_mesh(get_unit_sphere(),
                               sphere_at(torc_point(angle), 0.013F),
                               k_ornament_slot,
                               nullptr,
                               1.0F,
                               3);
    }
    QVector3D const buckle =
        f.local.point(f.bottom + f.up * 0.026F) +
        QVector3D(0.0F, 0.0F, f.torso_depth * k_tunic_depth * 1.06F);
    builder.add_palette_box(
        buckle, QVector3D(0.025F, 0.016F, 0.008F), k_ornament_slot, nullptr, 1.0F, 3);

    return std::move(builder).build();
  }();
  return archetype;
}

auto iberian_tunic_archetype() -> const RenderArchetype& {
  static const RenderArchetype archetype = [] {
    TunicFit const& f = tunic_fit();
    RenderArchetypeBuilder builder{"iberian_tunic"};
    add_tunic_body(builder, f);

    builder.add_palette_mesh(get_unit_cylinder(),
                             ring_at(f, f.top - f.up * 0.010F, 0.012F, 0.80F, 0.98F),
                             k_accent_slot,
                             nullptr,
                             1.0F,
                             1);

    add_belt(builder, f, 0.030F);
    QMatrix4x4 plaque;
    plaque.translate(
        f.local.point(f.bottom + f.up * 0.036F) +
        QVector3D(0.0F, 0.0F, f.torso_depth * k_tunic_depth * 1.06F + 0.006F));
    plaque.scale(0.047F, 0.024F, 0.010F);
    builder.add_palette_mesh(
        get_unit_cube(), plaque, k_ornament_slot, nullptr, 1.0F, 3);

    QVector3D const plaque_centre = plaque.column(3).toVector3D();
    builder.add_palette_box(plaque_centre + QVector3D(0.0F, 0.0F, 0.010F),
                            QVector3D(0.031F, 0.014F, 0.002F),
                            k_belt_slot);
    for (float x : {-0.036F, 0.036F}) {
      builder.add_palette_mesh(
          get_unit_sphere(),
          sphere_at(plaque_centre + QVector3D(x, 0.0F, 0.012F), 0.005F),
          k_ornament_slot,
          nullptr,
          1.0F,
          3);
    }
    return std::move(builder).build();
  }();
  return archetype;
}

auto hem_band_archetype(bool left) -> RenderArchetype {
  HumanoidBone const hip = left ? HumanoidBone::HipL : HumanoidBone::HipR;
  HumanoidBone const knee = left ? HumanoidBone::KneeL : HumanoidBone::KneeR;
  auto const palette = Render::Humanoid::humanoid_bind_palette();
  QVector3D const hip_pos =
      palette[static_cast<std::size_t>(hip)].column(3).toVector3D();
  QVector3D const knee_pos =
      palette[static_cast<std::size_t>(knee)].column(3).toVector3D();
  QVector3D const axis = knee_pos - hip_pos;
  RenderArchetypeBuilder builder{left ? "iberian_hem_band_l" : "iberian_hem_band_r"};
  builder.add_palette_mesh(get_unit_cylinder(),
                           cylinder_between(hip_pos + axis * 0.70F,
                                            hip_pos + axis * 0.80F,
                                            HumanProportions::UPPER_LEG_R * 1.14F),
                           0U);
  return std::move(builder).build();
}

auto tunic_static_attachment(const RenderArchetype& archetype,
                             std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  TunicFit const& f = tunic_fit();
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &archetype,
      .socket_bone_index = static_cast<std::uint16_t>(HumanoidBone::Chest),
      .unit_local_pose_at_bind = f.local.world,
  });
  fit_armor_to_waist(spec, f.waist_y, f.bottom.y());
  for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(k_allied_tunic_role_count);
       ++i) {
    spec.palette_role_remap[i] = static_cast<std::uint8_t>(base_role_byte + i);
  }
  return spec;
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

auto gallic_tunic_fill_role_colors(const HumanoidPalette& palette,
                                   QVector3D* out,
                                   std::size_t max) -> std::uint32_t {
  if (max < k_allied_tunic_role_count) {
    return 0;
  }
  out[k_tunic_slot] = palette.cloth;
  out[k_accent_slot] =
      saturate_color(palette.cloth * 0.18F + QVector3D(0.05F, 0.12F, 0.10F));
  out[k_belt_slot] = saturate_color(palette.leather_dark * 0.90F);
  out[k_ornament_slot] = QVector3D(0.88F, 0.70F, 0.30F);
  return k_allied_tunic_role_count;
}

auto iberian_tunic_fill_role_colors(const HumanoidPalette& palette,
                                    QVector3D* out,
                                    std::size_t max) -> std::uint32_t {
  if (max < k_allied_tunic_role_count) {
    return 0;
  }
  out[k_tunic_slot] = palette.cloth;
  out[k_accent_slot] = QVector3D(0.66F, 0.08F, 0.09F);
  out[k_belt_slot] = saturate_color(palette.leather_dark * 0.85F);
  out[k_ornament_slot] = QVector3D(0.80F, 0.62F, 0.32F);
  return k_allied_tunic_role_count;
}

auto gallic_tunic_make_static_attachment(std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  return tunic_static_attachment(gallic_tunic_archetype(), base_role_byte);
}

auto iberian_tunic_make_static_attachment(std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  return tunic_static_attachment(iberian_tunic_archetype(), base_role_byte);
}

auto iberian_hem_band_make_static_attachment(bool left, std::uint8_t crimson_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  HumanoidBone const hip = left ? HumanoidBone::HipL : HumanoidBone::HipR;
  static const RenderArchetype left_band = hem_band_archetype(true);
  static const RenderArchetype right_band = hem_band_archetype(false);
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = left ? &left_band : &right_band,
      .socket_bone_index = static_cast<std::uint16_t>(hip),
  });
  spec.palette_role_remap[0] = crimson_role_byte;
  return spec;
}

} // namespace Render::GL
