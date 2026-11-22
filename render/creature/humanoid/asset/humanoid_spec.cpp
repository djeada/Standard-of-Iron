#include "render/creature/humanoid/asset/humanoid_spec.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <mutex>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "animation/rig/humanoid_proportions.h"
#include "render/creature/gpu/rigged_mesh_cache.h"
#include "render/creature/humanoid/asset/humanoid_beard_mesh.h"
#include "render/creature/humanoid/asset/humanoid_body_primitives.h"
#include "render/creature/humanoid/asset/humanoid_face_mesh.h"
#include "render/creature/humanoid/asset/humanoid_skeleton_primitives.h"
#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/humanoid/runtime/body_frame_resolver.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/skeleton_evaluator.h"
#include "render/creature/spec.h"
#include "render/frame/bone_palette_arena.h"
#include "render/frame/submitter.h"
#include "render/geom/garment_shell.h"
#include "render/geom/mesh.h"
#include "render/geom/primitives.h"

namespace Render::Humanoid {

using namespace humanoid_spec_detail;

namespace {

void fill_humanoid_role_colors_impl(
    const Render::GL::HumanoidVariant& v,
    std::array<QVector3D, k_humanoid_role_count>& out) noexcept {
  out[Cloth - 1] = v.palette.cloth;
  out[Skin - 1] = v.palette.skin;
  out[Leather - 1] = v.palette.leather;
  out[LeatherDark - 1] = v.palette.leather_dark;
  out[Wood - 1] = v.palette.wood;
  out[Metal - 1] = v.palette.metal;
  out[ClothDark - 1] = v.palette.cloth * 0.48F + QVector3D(0.27F, 0.23F, 0.20F) * 0.36F;
  auto const& hair = v.facial_hair;
  QVector3D const grey(0.72F, 0.71F, 0.69F);
  out[Hair - 1] = (hair.color * (1.0F - hair.greyness)) + (grey * hair.greyness);
}

constexpr int k_smooth_limb_slices = 32;

auto smooth_limb_mesh(float tail_radius,
                      float head_slope,
                      float tail_slope,
                      bool cap_head,
                      bool cap_tail,
                      unsigned int slices) -> std::unique_ptr<Render::GL::Mesh> {
  constexpr unsigned int k_stacks = 4;
  unsigned int const row = slices + 1;
  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve((k_stacks + 3) * row + 2);
  indices.reserve((k_stacks * 2 + 2) * slices * 3);

  float const a = 2.0F - 2.0F * tail_radius + head_slope + tail_slope;
  float const b = -3.0F + 3.0F * tail_radius - 2.0F * head_slope - tail_slope;
  for (unsigned int stack = 0; stack <= k_stacks; ++stack) {
    float const t = static_cast<float>(stack) / k_stacks;
    float const radius = ((a * t + b) * t + head_slope) * t + 1.0F;
    float const slope = (3.0F * a * t + 2.0F * b) * t + head_slope;
    for (unsigned int slice = 0; slice <= slices; ++slice) {
      float const u = static_cast<float>(slice) / slices;
      float const angle = slice == slices ? 0.0F : 2.0F * std::numbers::pi_v<float> * u;
      float const x = std::cos(angle);
      float const z = std::sin(angle);
      QVector3D const normal = QVector3D(x, -slope, z).normalized();
      vertices.push_back({{radius * x, t - 0.5F, radius * z},
                          {normal.x(), normal.y(), normal.z()},
                          {u, t}});
    }
  }
  for (unsigned int stack = 0; stack < k_stacks; ++stack) {
    for (unsigned int slice = 0; slice < slices; ++slice) {
      auto const a0 = stack * row + slice;
      indices.insert(indices.end(),
                     {a0, a0 + row, a0 + 1, a0 + 1, a0 + row, a0 + row + 1});
    }
  }

  auto append_cap = [&](bool tail) {
    auto const center = static_cast<unsigned int>(vertices.size());
    float const y = tail ? 0.5F : -0.5F;
    float const ny = tail ? 1.0F : -1.0F;
    vertices.push_back({{0.0F, y, 0.0F}, {0.0F, ny, 0.0F}, {0.5F, 0.5F}});
    for (unsigned int slice = 0; slice <= slices; ++slice) {
      auto const& side = vertices[(tail ? k_stacks * row : 0) + slice];
      vertices.push_back({side.position, {0.0F, ny, 0.0F}, side.tex_coord});
    }
    for (unsigned int slice = 0; slice < slices; ++slice) {
      auto const first = center + slice + 1;
      indices.insert(indices.end(),
                     {center, tail ? first + 1 : first, tail ? first : first + 1});
    }
  };
  if (cap_head) {
    append_cap(false);
  }
  if (cap_tail) {
    append_cap(true);
  }

  for (std::size_t i = 0; i < indices.size(); i += 3) {
    std::swap(indices[i + 1], indices[i + 2]);
  }
  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto make_tunic_body(unsigned int radial,
                     unsigned int subdivisions) -> std::unique_ptr<Render::GL::Mesh> {
  float constexpr height = HP::NECK_BASE_Y - HP::WAIST_Y;
  auto shell = Render::GL::make_garment_shell(
      {
          {-0.025F, 0.150F, 0.092F},
          {height * 0.18F, 0.134F, 0.083F},
          {height * 0.43F, 0.148F, 0.093F, 0.004F},
          {height * 0.67F, 0.173F, 0.104F, 0.006F},
          {height * 0.81F, 0.181F, 0.091F},
          {height * 0.89F, 0.148F, 0.073F},
          {height, HP::NECK_RADIUS, HP::NECK_RADIUS * 0.93F},
      },
      radial,
      subdivisions,
      true);
  auto vertices = shell->get_vertices();
  for (auto& vertex : vertices) {
    float const h = vertex.position[1] / height;
    float const pelvis = std::clamp(1.0F - h / 0.32F, 0.0F, 1.0F);
    float const chest = std::clamp((h - 0.20F) / 0.40F, 0.0F, 1.0F);
    vertex.bone_indices = {bone(HumanoidBone::Pelvis),
                           bone(HumanoidBone::Spine),
                           bone(HumanoidBone::Chest),
                           0};
    vertex.bone_weights = {pelvis, 1.0F - pelvis - chest, chest, 0.0F};
  }
  return std::make_unique<Render::GL::Mesh>(vertices, shell->get_indices());
}

auto tunic_hem_mesh() -> Render::GL::Mesh* {
  static auto mesh = []() {
    auto shell = Render::GL::make_garment_shell({
        {-0.225F, 0.242F, 0.163F},
        {-0.10F, 0.178F, 0.111F},
        {-0.045F, 0.150F, 0.085F},
        {0.02F, 0.140F, 0.075F},
        {0.12F, 0.108F, 0.055F},
    });
    auto vertices = shell->get_vertices();
    for (auto& vertex : vertices) {
      float const y = vertex.position[1];
      float t = std::clamp(-y / 0.20F, 0.0F, 1.0F);
      t = t * t * (3.0F - 2.0F * t);
      float const legs = t * 0.45F;
      float const right = std::clamp(0.5F + vertex.position[0] / 0.08F, 0.0F, 1.0F);
      float const spine = std::clamp(y / 0.12F, 0.0F, 1.0F) * 0.65F;
      vertex.bone_indices = {static_cast<std::uint8_t>(HumanoidBone::Pelvis),
                             static_cast<std::uint8_t>(HumanoidBone::Spine),
                             static_cast<std::uint8_t>(HumanoidBone::HipL),
                             static_cast<std::uint8_t>(HumanoidBone::HipR)};
      vertex.bone_weights = {
          1.0F - legs - spine, spine, legs * (1.0F - right), legs * right};
    }
    return std::make_unique<Render::GL::Mesh>(vertices, shell->get_indices());
  }();
  return mesh.get();
}

void fit_tunic_hem(Creature::PrimitiveInstance& part) {
  part.shape = Creature::PrimitiveShape::Mesh;
  part.params.head_offset = QVector3D();
  part.params.half_extents = QVector3D(1.0F, 1.0F, 1.0F);
  part.custom_mesh = tunic_hem_mesh();
  part.mesh_skinning = Creature::MeshSkinning::Authored;
}

auto face_primitive(Render::GL::Mesh* mesh) -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_full_face";
  p.shape = Creature::PrimitiveShape::Mesh;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.half_extents = QVector3D(1.0F, 1.0F, 1.0F);
  p.custom_mesh = mesh;
  p.mesh_skinning = Creature::MeshSkinning::Authored;
  p.color_role = Skin;
  p.lod_mask = Creature::k_lod_full;
  return p;
}

struct SmoothBodyParts {
  std::array<Creature::PrimitiveInstance, k_full_parts.size() + 1> parts{};
  std::vector<std::unique_ptr<Render::GL::Mesh>> meshes;

  SmoothBodyParts() {
    std::copy(k_full_parts.begin(), k_full_parts.end(), parts.begin());
    meshes.push_back(make_tunic_body(28, 3));
    parts.front().custom_mesh = meshes.back().get();
    constexpr auto cranium = make_full_cranium();
    constexpr auto jaw = make_full_jaw();
    meshes.push_back(build_humanoid_face_mesh(cranium.params.head_offset,
                                              cranium.params.half_extents * 2.0F,
                                              jaw.params.head_offset,
                                              jaw.params.half_extents * 2.0F));
    parts.back() = face_primitive(meshes.back().get());
    for (auto& part : parts) {
      if (part.params.anchor_bone == bone(HumanoidBone::Pelvis) &&
          std::string_view(part.debug_name) == "humanoid_full_pelvis_block") {
        fit_tunic_hem(part);
      }
    }
    for (auto& proximal : parts) {
      if (proximal.shape != Creature::PrimitiveShape::TaperedCylinder ||
          proximal.params.anchor_bone != proximal.params.tail_bone) {
        continue;
      }
      for (auto& distal : parts) {
        if (distal.shape != Creature::PrimitiveShape::TaperedCylinder ||
            distal.params.anchor_bone != proximal.params.anchor_bone ||
            distal.params.anchor_bone == distal.params.tail_bone) {
          continue;
        }
        auto const anchor = static_cast<HumanoidBone>(proximal.params.anchor_bone);
        float const length =
            anchor == HumanoidBone::UpperArmL || anchor == HumanoidBone::UpperArmR
                ? HP::UPPER_ARM_LEN
            : anchor == HumanoidBone::ForearmL || anchor == HumanoidBone::ForearmR
                ? HP::FORE_ARM_LEN
            : anchor == HumanoidBone::HipL || anchor == HumanoidBone::HipR
                ? HP::UPPER_LEG_LEN
                : HP::LOWER_LEG_LEN;
        float const first_length = proximal.params.tail_offset.y();
        float const second_length = length - first_length;
        float const first_slope =
            (proximal.params.tail_radius - proximal.params.radius) / first_length;
        float const second_slope =
            (distal.params.tail_radius - distal.params.radius) / second_length;

        float const join_slope =
            first_slope * second_slope > 0.0F
                ? 2.0F * first_slope * second_slope / (first_slope + second_slope)
                : 0.0F;
        auto const slices = static_cast<unsigned int>(
            Render::GL::bake_radial_segments(std::max({proximal.params.radius,
                                                       proximal.params.tail_radius,
                                                       distal.params.radius,
                                                       distal.params.tail_radius}),
                                             k_smooth_limb_slices));
        auto attach = [&](Creature::PrimitiveInstance& part,
                          float span,
                          float start_slope,
                          float end_slope,
                          bool first) {
          float const radius = part.params.radius;
          meshes.push_back(smooth_limb_mesh(part.params.tail_radius / radius,
                                            start_slope * span / radius,
                                            end_slope * span / radius,
                                            first,
                                            !first,
                                            slices));
          part.custom_mesh = meshes.back().get();
        };
        attach(proximal, first_length, first_slope, join_slope, true);
        attach(distal, second_length, join_slope, second_slope, false);
        break;
      }
    }
  }
};

auto smooth_minimal_parts() {
  auto parts = k_minimal_parts;
  static const auto tunic = make_tunic_body(12, 1);
  parts.front().custom_mesh = tunic.get();

  constexpr int k_radial_segments = 12;
  constexpr int k_latitude_segments = 8;
  for (auto& part : parts) {
    if (std::string_view(part.debug_name) == "humanoid_full_pelvis_block") {
      fit_tunic_hem(part);
      continue;
    }
    switch (part.shape) {
    case Creature::PrimitiveShape::OrientedSphere:
      part.custom_mesh =
          Render::GL::get_unit_sphere(k_latitude_segments, k_radial_segments);
      break;
    case Creature::PrimitiveShape::Cylinder:
    case Creature::PrimitiveShape::OrientedCylinder:
      part.custom_mesh = Render::GL::get_unit_cylinder(k_radial_segments);
      break;
    case Creature::PrimitiveShape::TaperedCylinder:
      part.custom_mesh = Render::GL::get_unit_tapered_cylinder(
          1.0F, part.params.tail_radius / part.params.radius, k_radial_segments);
      break;
    default:
      break;
    }
  }
  return parts;
}

} // namespace

void apply_skeleton_proportion_pose(Render::GL::HumanoidPose& io_pose) noexcept {
  QVector3D const cervical_axis = io_pose.head_pos - io_pose.neck_base;

  io_pose.head_pos = io_pose.neck_base + cervical_axis * 0.70F;
}

auto humanoid_creature_spec() noexcept -> const Creature::CreatureSpec& {
  static const SmoothBodyParts full;
  static const auto minimal = smooth_minimal_parts();
  static const Creature::CreatureSpec spec = [] {
    Creature::CreatureSpec s;
    s.species_name = "humanoid";
    s.topology = humanoid_topology();
    s.lod_minimal = Creature::PartGraph{
        std::span<const Creature::PrimitiveInstance>(minimal),
    };
    s.lod_full = Creature::PartGraph{
        std::span<const Creature::PrimitiveInstance>(full.parts),
    };

    return s;
  }();
  return spec;
}

namespace {

struct BodyVariantSpec {
  std::string species_name;
  std::unique_ptr<Render::GL::Mesh> full_beard;
  std::unique_ptr<Render::GL::Mesh> minimal_beard;
  std::vector<Creature::PrimitiveInstance> full_parts;
  std::vector<Creature::PrimitiveInstance> minimal_parts;
  Creature::CreatureSpec spec;
};

auto beard_primitive(Render::GL::Mesh* mesh,
                     std::uint8_t lod_mask) -> Creature::PrimitiveInstance {
  Creature::PrimitiveInstance p{};
  p.debug_name = "humanoid_beard";
  p.shape = Creature::PrimitiveShape::Mesh;
  p.params.anchor_bone = bone(HumanoidBone::Head);
  p.params.half_extents = QVector3D(k_beard_head_silhouette_radius,
                                    k_beard_head_silhouette_radius,
                                    k_beard_head_silhouette_radius);
  p.custom_mesh = mesh;
  p.mesh_skinning = Creature::MeshSkinning::Authored;
  p.color_role = Hair;
  p.lod_mask = lod_mask;
  return p;
}

auto build_body_variant_spec(HumanoidBodyVariant variant)
    -> std::unique_ptr<BodyVariantSpec> {
  auto out = std::make_unique<BodyVariantSpec>();
  auto const& base = humanoid_creature_spec();
  out->species_name =
      std::string(base.species_name) + std::string(body_variant_name_suffix(variant));
  out->full_beard = build_humanoid_beard_mesh(variant, Creature::CreatureLOD::Full);
  out->minimal_beard =
      build_humanoid_beard_mesh(variant, Creature::CreatureLOD::Minimal);
  out->full_parts.assign(base.lod_full.primitives.begin(),
                         base.lod_full.primitives.end());
  out->minimal_parts.assign(base.lod_minimal.primitives.begin(),
                            base.lod_minimal.primitives.end());
  if (out->full_beard != nullptr) {
    out->full_parts.push_back(
        beard_primitive(out->full_beard.get(), Creature::k_lod_full));
  }
  if (out->minimal_beard != nullptr) {
    out->minimal_parts.push_back(
        beard_primitive(out->minimal_beard.get(), Creature::k_lod_minimal));
  }
  out->spec = base;
  out->spec.species_name = out->species_name;
  out->spec.body_variant = static_cast<std::uint8_t>(variant);
  out->spec.lod_full = Creature::PartGraph{std::span<const Creature::PrimitiveInstance>(
      out->full_parts.data(), out->full_parts.size())};
  out->spec.lod_minimal =
      Creature::PartGraph{std::span<const Creature::PrimitiveInstance>(
          out->minimal_parts.data(), out->minimal_parts.size())};
  return out;
}

} // namespace

auto humanoid_creature_spec_for_body_variant(std::uint8_t body_variant) noexcept
    -> const Creature::CreatureSpec* {
  if (body_variant == 0U || body_variant >= k_humanoid_body_variant_count) {
    return &humanoid_creature_spec();
  }
  static const auto variants = [] {
    std::array<std::unique_ptr<BodyVariantSpec>, k_humanoid_body_variant_count> built{};
    for (std::size_t i = 1; i < built.size(); ++i) {
      built[i] = build_body_variant_spec(static_cast<HumanoidBodyVariant>(i));
    }
    return built;
  }();
  return &variants[body_variant]->spec;
}

auto skeleton_humanoid_creature_spec() noexcept -> const Creature::CreatureSpec& {
  static const Creature::CreatureSpec spec = [] {
    Creature::CreatureSpec s;
    s.species_name = "skeleton_humanoid";
    s.topology = humanoid_topology();
    s.lod_minimal = Creature::PartGraph{
        std::span<const Creature::PrimitiveInstance>(k_skeleton_minimal_parts.data(),
                                                     k_skeleton_minimal_parts.size()),
    };
    s.lod_full = Creature::PartGraph{
        std::span<const Creature::PrimitiveInstance>(k_skeleton_full_parts.data(),
                                                     k_skeleton_full_parts.size()),
    };
    return s;
  }();
  return spec;
}

} // namespace Render::Humanoid

namespace Render::Humanoid {

namespace {

auto build_humanoid_bind_palette() -> std::array<QMatrix4x4, k_bone_count> {
  Render::GL::VariationParams variation{};
  variation.height_scale = 1.0F;
  variation.bulk_scale = 1.0F;
  variation.stance_width = 1.0F;
  variation.arm_swing_amp = 1.0F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;

  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidRendererBase::compute_locomotion_pose(
      0, 0.0F, false, variation, pose);

  std::array<QMatrix4x4, k_bone_count> palette{};
  BonePalette tmp{};
  evaluate_skeleton(pose, QVector3D(1.0F, 0.0F, 0.0F), tmp);
  for (std::size_t i = 0; i < k_bone_count; ++i) {
    palette[i] = tmp[i];
  }
  return palette;
}

} // namespace

auto humanoid_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const std::array<QMatrix4x4, k_bone_count> palette =
      build_humanoid_bind_palette();
  return {palette.data(), palette.size()};
}

auto humanoid_inverse_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const std::array<QMatrix4x4, k_bone_count> inverse = []() {
    auto const bind = humanoid_bind_palette();
    std::array<QMatrix4x4, k_bone_count> result{};
    for (std::size_t i = 0; i < k_bone_count && i < bind.size(); ++i) {
      result[i] = bind[i].inverted();
    }
    return result;
  }();
  return {inverse.data(), inverse.size()};
}

auto humanoid_bind_body_frames() noexcept -> const Render::GL::BodyFrames& {
  static const Render::GL::BodyFrames frames = []() -> Render::GL::BodyFrames {
    Render::GL::VariationParams variation{};
    variation.height_scale = 1.0F;
    variation.bulk_scale = 1.0F;
    variation.stance_width = 1.0F;
    variation.arm_swing_amp = 1.0F;
    variation.walk_speed_mult = 1.0F;
    variation.posture_slump = 0.0F;
    variation.shoulder_tilt = 0.0F;

    Render::GL::HumanoidPose pose{};
    Render::GL::HumanoidRendererBase::compute_locomotion_pose(
        0, 0.0F, false, variation, pose);

    HumanoidBodyMetrics metrics{};
    compute_humanoid_body_metrics(pose, QVector3D(1.0F, 1.0F, 1.0F), 1.0F, metrics);
    compute_humanoid_head_frame(pose, metrics);
    compute_humanoid_body_frames(pose, metrics);
    return pose.body_frames;
  }();
  return frames;
}

void fill_humanoid_role_colors(
    const Render::GL::HumanoidVariant& variant,
    std::array<QVector3D, k_humanoid_role_count>& out_roles) noexcept {
  fill_humanoid_role_colors_impl(variant, out_roles);
}

} // namespace Render::Humanoid
