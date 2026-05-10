#include "elephant_manifest.h"

#include "../creature/bpat/bpat_format.h"
#include "../creature/quadruped/skeleton_factory.h"
#include "../gl/primitives.h"
#include "dimensions.h"
#include "elephant_gait.h"
#include "elephant_spec.h"

#include <QMatrix4x4>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <span>
#include <vector>

namespace Render::Elephant {

namespace {

constexpr std::uint8_t k_role_skin = 1;
constexpr std::uint8_t k_role_skin_trunk = 5;
constexpr std::uint8_t k_role_tusk = 6;
constexpr std::uint8_t k_role_eye = 7;
constexpr int k_elephant_material_id = 6;
constexpr float k_elephant_visual_scale = 1.85F;
constexpr float k_elephant_body_length_scale = 0.56F;
constexpr float k_elephant_body_width_scale = 0.66F;
constexpr float k_elephant_body_height_scale = 1.02F;
constexpr float k_elephant_head_scale = 1.55F;
constexpr float k_elephant_torso_width_scale = 1.20F;
constexpr float k_elephant_torso_height_scale = 1.10F;
constexpr int k_elephant_leg_radial_segments = 6;
constexpr float k_elephant_whole_thigh_upper_trim = 0.22F;
constexpr float k_elephant_front_thigh_head_y = 0.10F;
constexpr float k_elephant_rear_thigh_head_y = 0.06F;
constexpr float k_elephant_front_thigh_tail_y = 0.03F;
constexpr float k_elephant_rear_thigh_tail_y = 0.04F;
constexpr float k_elephant_front_thigh_head_z = 0.02F;
constexpr float k_elephant_rear_thigh_head_z = 0.07F;
constexpr float k_elephant_front_thigh_tail_z = 0.01F;
constexpr float k_elephant_rear_thigh_tail_z = 0.04F;
constexpr float k_elephant_thigh_thickness_scale = 2.0F;
constexpr float k_elephant_hoof_thickness_scale = 0.8F;

struct ElephantClipSpec {
  Render::Creature::BakeClipDescriptor desc;
  bool is_moving;
  Render::GL::ElephantGait gait;
  bool is_fighting{false};
  bool is_death{false};
  bool is_dead_hold{false};
  float bob_scale{0.0F};
};

const std::array<ElephantClipSpec, 6> k_elephant_clips{{
    {{"idle", 24U, 24.0F, true},
     false,
     Render::GL::ElephantGait{2.0F, 0.0F, 0.0F, 0.02F, 0.01F},
     false,
     false,
     false,
     0.0F},
    {{"walk", 24U, 24.0F, true},
     true,
     Render::GL::ElephantGait{1.2F, 0.25F, 0.0F, 0.30F, 0.10F},
     false,
     false,
     false,
     0.62F},
    {{"run", 16U, 24.0F, true},
     true,
     Render::GL::ElephantGait{0.6F, 0.5F, 0.5F, 0.70F, 0.25F},
     false,
     false,
     false,
     0.75F},
    {{"fight", 24U, 24.0F, true},
     false,
     Render::GL::ElephantGait{1.15F, 0.0F, 0.0F, 0.30F, 0.06F},
     true,
     false,
     false,
     0.0F},
    {{"die", 24U, 24.0F, false},
     false,
     Render::GL::ElephantGait{1.15F, 0.0F, 0.0F, 0.30F, 0.06F},
     false,
     true,
     false,
     0.0F},
    {{"dead", 1U, 1.0F, true},
     false,
     Render::GL::ElephantGait{1.15F, 0.0F, 0.0F, 0.30F, 0.06F},
     false,
     true,
     true,
     0.0F},
}};

const std::array<Render::Creature::BakeClipDescriptor, k_elephant_clips.size()>
    k_elephant_clip_descs{{
        k_elephant_clips[0].desc,
        k_elephant_clips[1].desc,
        k_elephant_clips[2].desc,
        k_elephant_clips[3].desc,
        k_elephant_clips[4].desc,
        k_elephant_clips[5].desc,
    }};

auto build_elephant_whole_nodes()
    -> std::vector<Render::Creature::Quadruped::MeshNode> {
  using namespace Render::Creature;
  using namespace Render::Creature::Quadruped;

  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  float const bw =
      dims.body_width * k_elephant_body_width_scale * k_elephant_visual_scale;
  float const bh =
      dims.body_height * k_elephant_body_height_scale * k_elephant_visual_scale;
  float const bl =
      dims.body_length * k_elephant_body_length_scale * k_elephant_visual_scale;
  float const hw = dims.head_width * k_elephant_visual_scale;
  float const hh = dims.head_height * k_elephant_visual_scale;
  float const hl = dims.head_length * k_elephant_visual_scale;

  std::vector<MeshNode> nodes;
  nodes.reserve(24);

  BarrelNode body;
  body.rings = {
      {-bl * 0.70F, bh * 0.00F, bw * 0.34F * k_elephant_torso_width_scale,
       bh * 0.12F * k_elephant_torso_height_scale,
       bh * 0.12F * k_elephant_torso_height_scale},
      {-bl * 0.56F, bh * 0.04F, bw * 0.70F * k_elephant_torso_width_scale,
       bh * 0.32F * k_elephant_torso_height_scale,
       bh * 0.28F * k_elephant_torso_height_scale},
      {-bl * 0.36F, bh * 0.04F, bw * 0.88F * k_elephant_torso_width_scale,
       bh * 0.38F * k_elephant_torso_height_scale,
       bh * 0.40F * k_elephant_torso_height_scale},
      {-bl * 0.14F, bh * 0.02F, bw * 0.96F * k_elephant_torso_width_scale,
       bh * 0.42F * k_elephant_torso_height_scale,
       bh * 0.46F * k_elephant_torso_height_scale},
      {bl * 0.10F, bh * 0.02F, bw * 0.96F * k_elephant_torso_width_scale,
       bh * 0.42F * k_elephant_torso_height_scale,
       bh * 0.46F * k_elephant_torso_height_scale},
      {bl * 0.32F, bh * 0.06F, bw * 0.86F * k_elephant_torso_width_scale,
       bh * 0.40F * k_elephant_torso_height_scale,
       bh * 0.36F * k_elephant_torso_height_scale},
      {bl * 0.50F, bh * 0.10F, bw * 0.74F * k_elephant_torso_width_scale,
       bh * 0.38F * k_elephant_torso_height_scale,
       bh * 0.28F * k_elephant_torso_height_scale},
      {bl * 0.62F, bh * 0.14F, bw * 0.50F * k_elephant_torso_width_scale,
       bh * 0.24F * k_elephant_torso_height_scale,
       bh * 0.14F * k_elephant_torso_height_scale},
      {bl * 0.70F, bh * 0.13F, bw * 0.28F * k_elephant_torso_width_scale,
       bh * 0.10F * k_elephant_torso_height_scale,
       bh * 0.04F * k_elephant_torso_height_scale},
  };
  nodes.push_back({"elephant.body", static_cast<BoneIndex>(ElephantBone::Body),
                   k_role_skin, k_lod_all, 0, body});

  EllipsoidNode head;
  head.center = QVector3D(0.0F, bh * 0.28F, bl * 0.72F);
  head.radii = QVector3D(hw * 0.41F * k_elephant_head_scale,
                         hh * 0.51F * k_elephant_head_scale,
                         hl * 0.48F * k_elephant_head_scale);
  head.ring_count = 5U;
  head.ring_vertices = 8U;
  nodes.push_back({"elephant.head", static_cast<BoneIndex>(ElephantBone::Head),
                   k_role_skin, k_lod_all, 0, head});

  float const head_front_z = head.center.z() + head.radii.z();
  float const trunk_base_z = bl * 1.16F;
  float const eye_radius_z = hl * 0.035F;
  float const head_eye_z = head_front_z - 3.5F * eye_radius_z;

  EllipsoidNode trunk_bridge;
  trunk_bridge.center = QVector3D(0.0F, bh * 0.02F, head_front_z - hl * 0.045F);
  trunk_bridge.radii = QVector3D(hw * 0.24F, hh * 0.22F, hl * 0.14F);
  trunk_bridge.ring_count = 4U;
  trunk_bridge.ring_vertices = 6U;
  nodes.push_back({"elephant.trunk.bridge",
                   static_cast<BoneIndex>(ElephantBone::Head), k_role_skin,
                   k_lod_all, 0, trunk_bridge});

  auto add_eye = [&](float side) {
    EllipsoidNode eye;
    eye.center = QVector3D(side * hw * 0.34F, bh * 0.24F, head_eye_z);
    eye.radii = QVector3D(hw * 0.055F, hh * 0.055F, eye_radius_z);
    eye.ring_count = 4U;
    eye.ring_vertices = 6U;
    nodes.push_back({"elephant.eye", static_cast<BoneIndex>(ElephantBone::Head),
                     k_role_eye, k_lod_all, 0, eye});
  };
  add_eye(1.0F);
  add_eye(-1.0F);

  auto add_leg = [&](std::string_view thigh_name, std::string_view calf_name,
                     std::string_view hoof_name, float x, float z,
                     BoneIndex shoulder_bone, BoneIndex knee_bone,
                     BoneIndex foot_bone, bool front) {
    float const top_y = -bh * 0.30F;
    float const sole_y = -dims.leg_length * k_elephant_visual_scale * 2.04F;
    float const leg_drop = sole_y - top_y;
    float const knee_y = top_y + leg_drop * (front ? 0.36F : 0.34F);
    float const hoof_height = dims.foot_radius * k_elephant_visual_scale *
                              0.86F * k_elephant_hoof_thickness_scale;
    float const hoof_top_y = sole_y + hoof_height;
    float const calf_end_y =
        hoof_top_y + dims.leg_radius * k_elephant_visual_scale * 0.08F;
    float const bend_z = (front ? 1.0F : -1.0F) * dims.body_length *
                         k_elephant_visual_scale * 0.16F;
    float const ankle_z = z + (front ? 1.0F : -1.0F) * dims.body_length *
                                  k_elephant_visual_scale * 0.06F;
    float const foot_scale = dims.foot_radius * k_elephant_visual_scale;
    float const leg_scale =
        dims.leg_radius * k_elephant_visual_scale * (front ? 2.15F : 2.02F);
    float const hoof_radius_y = hoof_height * 0.5F;
    float const knee_x = x;
    float const ankle_x = x;

    QVector3D const shoulder_local(x, top_y, z);
    QVector3D const knee_local(knee_x, knee_y, z + bend_z);
    QVector3D const calf_end(ankle_x, calf_end_y, ankle_z);

    QVector3D thigh_axis = knee_local - shoulder_local;
    if (thigh_axis.lengthSquared() > 1.0e-6F) {
      thigh_axis.normalize();
    }
    QVector3D calf_axis = calf_end - knee_local;
    if (calf_axis.lengthSquared() > 1.0e-6F) {
      calf_axis.normalize();
    }
    float const knee_gap = leg_scale * 0.34F;
    float const ankle_gap = foot_scale * 0.24F;
    float const thigh_trim_y = (knee_local.y() - shoulder_local.y()) *
                               k_elephant_whole_thigh_upper_trim;

    TubeNode thigh;
    thigh.start = shoulder_local + QVector3D(0.0F, thigh_trim_y, 0.0F);
    thigh.end = knee_local - thigh_axis * knee_gap;
    thigh.start_radius = leg_scale * 0.44F * k_elephant_thigh_thickness_scale;
    thigh.end_radius = leg_scale * 0.20F * k_elephant_thigh_thickness_scale;
    thigh.segment_count = 4U;
    thigh.ring_vertices = 6U;
    nodes.push_back(
        {thigh_name, shoulder_bone, k_role_skin, k_lod_all, 0, thigh});

    TubeNode calf;
    calf.start = knee_local + calf_axis * knee_gap;
    calf.end = calf_end + calf_axis * ankle_gap;
    calf.start_radius = leg_scale * 0.16F;
    calf.end_radius = leg_scale * 0.10F;
    calf.segment_count = 4U;
    calf.ring_vertices = 6U;
    nodes.push_back({calf_name, knee_bone, k_role_skin, k_lod_all, 0, calf});

    EllipsoidNode hoof;
    hoof.center = QVector3D(ankle_x, hoof_top_y - hoof_radius_y, ankle_z);
    hoof.radii = QVector3D(
        foot_scale * (front ? 1.22F : 1.14F) * k_elephant_hoof_thickness_scale,
        hoof_radius_y,
        foot_scale * (front ? 1.42F : 1.28F) * k_elephant_hoof_thickness_scale);
    hoof.ring_count = 4U;
    hoof.ring_vertices = 6U;
    nodes.push_back({hoof_name, foot_bone, k_role_skin, k_lod_all, 0, hoof});
  };
  add_leg("elephant.leg.fl.thigh", "elephant.leg.fl.calf",
          "elephant.leg.fl.hoof", bw * 0.40F, bl * 0.42F,
          static_cast<BoneIndex>(ElephantBone::ShoulderFL),
          static_cast<BoneIndex>(ElephantBone::KneeFL),
          static_cast<BoneIndex>(ElephantBone::FootFL), true);
  add_leg("elephant.leg.fr.thigh", "elephant.leg.fr.calf",
          "elephant.leg.fr.hoof", -bw * 0.40F, bl * 0.42F,
          static_cast<BoneIndex>(ElephantBone::ShoulderFR),
          static_cast<BoneIndex>(ElephantBone::KneeFR),
          static_cast<BoneIndex>(ElephantBone::FootFR), true);
  add_leg("elephant.leg.bl.thigh", "elephant.leg.bl.calf",
          "elephant.leg.bl.hoof", bw * 0.38F, -bl * 0.44F,
          static_cast<BoneIndex>(ElephantBone::ShoulderBL),
          static_cast<BoneIndex>(ElephantBone::KneeBL),
          static_cast<BoneIndex>(ElephantBone::FootBL), false);
  add_leg("elephant.leg.br.thigh", "elephant.leg.br.calf",
          "elephant.leg.br.hoof", -bw * 0.38F, -bl * 0.44F,
          static_cast<BoneIndex>(ElephantBone::ShoulderBR),
          static_cast<BoneIndex>(ElephantBone::KneeBR),
          static_cast<BoneIndex>(ElephantBone::FootBR), false);

  SnoutNode trunk;
  trunk.start = QVector3D(0.0F, bh * 0.06F, trunk_base_z);
  trunk.end = QVector3D(0.0F, -bh * 1.02F, bl * 1.04F);
  trunk.base_radius = dims.trunk_base_radius * k_elephant_visual_scale * 0.38F;
  trunk.tip_radius = dims.trunk_tip_radius * k_elephant_visual_scale * 0.25F;
  trunk.sag = -bh * 0.16F;
  trunk.segment_count = 9U;
  trunk.ring_vertices = 6U;
  nodes.push_back({"elephant.trunk",
                   static_cast<BoneIndex>(ElephantBone::TrunkTip),
                   k_role_skin_trunk, k_lod_all, 0, trunk});

  auto add_tusk = [&](float side) {
    ConeNode tusk;
    tusk.base_center = QVector3D(side * hw * 0.32F, bh * 0.02F, bl * 0.96F);
    tusk.tip = QVector3D(side * hw * 0.42F, -bh * 0.34F,
                         bl * 1.10F + dims.tusk_length *
                                          k_elephant_visual_scale * 0.24F);
    tusk.base_radius = dims.tusk_radius * k_elephant_visual_scale * 1.0F;
    tusk.ring_vertices = 6U;
    nodes.push_back({"elephant.tusk",
                     static_cast<BoneIndex>(ElephantBone::Head), k_role_tusk,
                     k_lod_all, 0, tusk});
  };
  add_tusk(1.0F);
  add_tusk(-1.0F);

  auto make_ear = [&](float side) {
    constexpr float k_elephant_ear_scale = 1.0F / 1.5F;
    FlatFanNode ear;
    QVector3D const ear_root(side * (hw * ((0.48F + 0.34F + 0.40F) / 3.0F)),
                             bh * ((0.82F + 0.08F + 0.58F) / 3.0F),
                             bl * ((0.56F + 0.62F + 0.58F) / 3.0F));
    auto scale_from_root = [&](const QVector3D &point) {
      return ear_root + (point - ear_root) * k_elephant_ear_scale;
    };
    ear.outline = {
        scale_from_root({side * (hw * 0.48F), bh * 0.82F, bl * 0.56F}),
        scale_from_root({side * (hw * 1.34F), bh * 0.70F, bl * 0.48F}),
        scale_from_root({side * (hw * 1.72F), bh * 0.26F, bl * 0.44F}),
        scale_from_root({side * (hw * 1.54F), -bh * 0.24F, bl * 0.44F}),
        scale_from_root({side * (hw * 1.04F), -bh * 0.48F, bl * 0.50F}),
        scale_from_root({side * (hw * 0.46F), -bh * 0.34F, bl * 0.60F}),
        scale_from_root({side * (hw * 0.34F), bh * 0.08F, bl * 0.62F}),
        scale_from_root({side * (hw * 0.40F), bh * 0.58F, bl * 0.58F}),
    };
    ear.thickness_axis = QVector3D(0.0F, 0.0F, 1.0F);
    ear.thickness = dims.ear_thickness * k_elephant_visual_scale * 1.15F *
                    k_elephant_ear_scale;
    nodes.push_back({"elephant.ear", static_cast<BoneIndex>(ElephantBone::Head),
                     k_role_skin, k_lod_all, 0, ear});
  };
  make_ear(1.0F);
  make_ear(-1.0F);

  TubeNode tail;
  tail.start = QVector3D(0.0F, bh * 0.11F, -bl * 0.50F);
  tail.end = QVector3D(0.0F, -bh * 0.48F, -bl * 1.00F);
  tail.start_radius = dims.tail_length * k_elephant_visual_scale * 0.03F;
  tail.end_radius = dims.tail_length * k_elephant_visual_scale * 0.04F;
  tail.segment_count = 4U;
  tail.ring_vertices = 6U;
  tail.sag = -bh * 0.05F;
  nodes.push_back({"elephant.tail", static_cast<BoneIndex>(ElephantBone::Body),
                   k_role_skin, k_lod_all, 0, tail});

  return nodes;
}

const auto k_elephant_whole_nodes = build_elephant_whole_nodes();
const std::array<std::string_view, 1> k_elephant_full_excluded_prefixes{
    "elephant.leg."};

struct ElephantLegRingProfile {
  float y{};
  float x_shift{};
  float z_shift{};
  float x_outer{};
  float x_inner{};
  float z_front{};
  float z_back{};
};

auto build_elephant_leg_span_mesh(std::span<const ElephantLegRingProfile> rings)
    -> std::unique_ptr<Render::GL::Mesh> {
  constexpr int k_ring_vertices = 8;
  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(rings.size() * k_ring_vertices + 2U);
  indices.reserve((rings.size() - 1U) * k_ring_vertices * 6U +
                  k_ring_vertices * 6U);

  auto append_vertex = [&](float x, float y, float z, float u, float v) {
    QVector3D normal(x, 0.0F, z);
    if (normal.lengthSquared() < 1.0e-6F) {
      normal = QVector3D(0.0F, 1.0F, 0.0F);
    } else {
      normal.normalize();
    }
    vertices.push_back(
        {{x, y, z}, {normal.x(), normal.y(), normal.z()}, {u, v}});
  };

  for (ElephantLegRingProfile const &ring : rings) {
    append_vertex(ring.x_shift + ring.x_outer * 0.60F, ring.y,
                  ring.z_shift + ring.z_front * 0.88F, 0.00F, ring.y + 0.5F);
    append_vertex(ring.x_shift, ring.y, ring.z_shift + ring.z_front, 0.14F,
                  ring.y + 0.5F);
    append_vertex(ring.x_shift - ring.x_inner * 0.60F, ring.y,
                  ring.z_shift + ring.z_front * 0.88F, 0.28F, ring.y + 0.5F);
    append_vertex(ring.x_shift - ring.x_inner, ring.y, ring.z_shift, 0.42F,
                  ring.y + 0.5F);
    append_vertex(ring.x_shift - ring.x_inner * 0.60F, ring.y,
                  ring.z_shift - ring.z_back * 0.84F, 0.56F, ring.y + 0.5F);
    append_vertex(ring.x_shift, ring.y, ring.z_shift - ring.z_back, 0.70F,
                  ring.y + 0.5F);
    append_vertex(ring.x_shift + ring.x_outer * 0.60F, ring.y,
                  ring.z_shift - ring.z_back * 0.84F, 0.84F, ring.y + 0.5F);
    append_vertex(ring.x_shift + ring.x_outer, ring.y, ring.z_shift, 1.00F,
                  ring.y + 0.5F);
  }

  for (std::size_t ring = 0; ring + 1U < rings.size(); ++ring) {
    unsigned int const base = static_cast<unsigned int>(ring * k_ring_vertices);
    unsigned int const next =
        static_cast<unsigned int>((ring + 1U) * k_ring_vertices);
    for (int i = 0; i < k_ring_vertices; ++i) {
      unsigned int const a = base + static_cast<unsigned int>(i);
      unsigned int const b =
          base + static_cast<unsigned int>((i + 1) % k_ring_vertices);
      unsigned int const c =
          next + static_cast<unsigned int>((i + 1) % k_ring_vertices);
      unsigned int const d = next + static_cast<unsigned int>(i);
      indices.insert(indices.end(), {a, b, c, c, d, a});
    }
  }

  auto add_cap = [&](std::size_t ring, bool top) {
    ElephantLegRingProfile const &profile = rings[ring];
    unsigned int const center = static_cast<unsigned int>(vertices.size());
    vertices.push_back({{profile.x_shift, profile.y, profile.z_shift},
                        {0.0F, top ? -1.0F : 1.0F, 0.0F},
                        {0.5F, 0.5F}});
    unsigned int const base = static_cast<unsigned int>(ring * k_ring_vertices);
    for (int i = 0; i < k_ring_vertices; ++i) {
      unsigned int const a = base + static_cast<unsigned int>(i);
      unsigned int const b =
          base + static_cast<unsigned int>((i + 1) % k_ring_vertices);
      if (top) {
        indices.insert(indices.end(), {center, b, a});
      } else {
        indices.insert(indices.end(), {center, a, b});
      }
    }
  };
  add_cap(0U, true);
  add_cap(rings.size() - 1U, false);

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto elephant_front_thigh_mesh() noexcept -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> mesh = [] {
    auto const rings = std::array<ElephantLegRingProfile, 4>{{
        {-0.50F, 0.00F, 0.08F, 0.82F, 0.76F, 0.96F, 0.58F},
        {-0.18F, 0.00F, 0.05F, 0.68F, 0.62F, 0.80F, 0.50F},
        {0.16F, 0.00F, 0.02F, 0.46F, 0.40F, 0.54F, 0.34F},
        {0.50F, 0.00F, 0.00F, 0.20F, 0.18F, 0.24F, 0.16F},
    }};
    return build_elephant_leg_span_mesh(rings);
  }();
  return mesh.get();
}

auto elephant_rear_thigh_mesh() noexcept -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> mesh = [] {
    auto const rings = std::array<ElephantLegRingProfile, 4>{{
        {-0.50F, 0.00F, 0.12F, 0.86F, 0.80F, 0.86F, 0.52F},
        {-0.18F, 0.00F, 0.08F, 0.70F, 0.64F, 0.72F, 0.46F},
        {0.16F, 0.00F, 0.03F, 0.48F, 0.42F, 0.48F, 0.30F},
        {0.50F, 0.00F, 0.00F, 0.22F, 0.18F, 0.22F, 0.14F},
    }};
    return build_elephant_leg_span_mesh(rings);
  }();
  return mesh.get();
}

auto elephant_leg_segment_mesh() noexcept -> Render::GL::Mesh * {
  static Render::GL::Mesh *const mesh =
      Render::GL::get_unit_cylinder(k_elephant_leg_radial_segments);
  return mesh;
}

auto build_elephant_full_leg_overlays()
    -> std::array<Render::Creature::PrimitiveInstance, 12> {
  using Render::Creature::PrimitiveInstance;
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  std::array<PrimitiveInstance, 12> out{};
  std::size_t idx = 0U;

  auto add_leg = [&](std::string_view thigh_name, std::string_view calf_name,
                     std::string_view hoof_name,
                     Render::Creature::BoneIndex shoulder_bone,
                     Render::Creature::BoneIndex knee_bone,
                     Render::Creature::BoneIndex foot_bone, bool front) {
    PrimitiveInstance &thigh = out[idx++];
    thigh.debug_name = thigh_name;
    thigh.shape = Render::Creature::PrimitiveShape::BoneSpanMesh;
    thigh.params.anchor_bone = shoulder_bone;
    thigh.params.tail_bone = knee_bone;
    thigh.params.head_offset =
        QVector3D(0.0F,
                  dims.leg_length * (front ? k_elephant_front_thigh_head_y
                                           : k_elephant_rear_thigh_head_y),
                  dims.body_length * (front ? k_elephant_front_thigh_head_z
                                            : k_elephant_rear_thigh_head_z));
    thigh.params.tail_offset =
        QVector3D(0.0F,
                  dims.leg_length * (front ? k_elephant_front_thigh_tail_y
                                           : k_elephant_rear_thigh_tail_y),
                  dims.body_length * (front ? k_elephant_front_thigh_tail_z
                                            : k_elephant_rear_thigh_tail_z));
    thigh.params.radius = dims.leg_radius * (front ? 1.95F : 1.90F) *
                          k_elephant_thigh_thickness_scale;
    thigh.params.depth_radius = dims.leg_radius * (front ? 2.25F : 2.10F) *
                                k_elephant_thigh_thickness_scale;
    thigh.custom_mesh =
        front ? elephant_front_thigh_mesh() : elephant_rear_thigh_mesh();
    thigh.color_role = k_role_skin;
    thigh.material_id = k_elephant_material_id;
    thigh.lod_mask = Render::Creature::k_lod_full;

    PrimitiveInstance &calf = out[idx++];
    calf.debug_name = calf_name;
    calf.shape = Render::Creature::PrimitiveShape::Cylinder;
    calf.params.anchor_bone = knee_bone;
    calf.params.tail_bone = foot_bone;
    calf.params.head_offset = QVector3D(0.0F, dims.leg_length * 0.10F, 0.0F);
    calf.params.tail_offset =
        QVector3D(0.0F, dims.foot_radius * (front ? 0.06F : 0.04F), 0.0F);
    calf.params.radius = dims.leg_radius * (front ? 0.90F : 0.85F);
    calf.custom_mesh = elephant_leg_segment_mesh();
    calf.color_role = k_role_skin;
    calf.material_id = k_elephant_material_id;
    calf.lod_mask = Render::Creature::k_lod_full;

    PrimitiveInstance &hoof = out[idx++];
    hoof.debug_name = hoof_name;
    hoof.shape = Render::Creature::PrimitiveShape::Box;
    hoof.params.anchor_bone = foot_bone;
    hoof.params.head_offset = QVector3D(
        0.0F,
        dims.foot_radius * (0.56F / 3.0F) * k_elephant_hoof_thickness_scale,
        (front ? 1.0F : -1.0F) * dims.foot_radius * (0.10F / 3.0F) *
            k_elephant_hoof_thickness_scale);
    hoof.params.half_extents = QVector3D(
        dims.foot_radius * (front ? 0.88F / 3.0F : 0.82F / 3.0F) *
            k_elephant_hoof_thickness_scale,
        dims.foot_radius * (0.56F / 3.0F) * k_elephant_hoof_thickness_scale,
        dims.foot_radius * (front ? 1.18F / 3.0F : 1.02F / 3.0F) *
            k_elephant_hoof_thickness_scale);
    hoof.color_role = k_role_skin;
    hoof.material_id = k_elephant_material_id;
    hoof.lod_mask = Render::Creature::k_lod_full;
  };

  add_leg("elephant.leg.fl.thigh", "elephant.leg.fl.calf",
          "elephant.leg.fl.hoof",
          static_cast<Render::Creature::BoneIndex>(ElephantBone::ShoulderFL),
          static_cast<Render::Creature::BoneIndex>(ElephantBone::KneeFL),
          static_cast<Render::Creature::BoneIndex>(ElephantBone::FootFL), true);
  add_leg("elephant.leg.fr.thigh", "elephant.leg.fr.calf",
          "elephant.leg.fr.hoof",
          static_cast<Render::Creature::BoneIndex>(ElephantBone::ShoulderFR),
          static_cast<Render::Creature::BoneIndex>(ElephantBone::KneeFR),
          static_cast<Render::Creature::BoneIndex>(ElephantBone::FootFR), true);
  add_leg(
      "elephant.leg.bl.thigh", "elephant.leg.bl.calf", "elephant.leg.bl.hoof",
      static_cast<Render::Creature::BoneIndex>(ElephantBone::ShoulderBL),
      static_cast<Render::Creature::BoneIndex>(ElephantBone::KneeBL),
      static_cast<Render::Creature::BoneIndex>(ElephantBone::FootBL), false);
  add_leg(
      "elephant.leg.br.thigh", "elephant.leg.br.calf", "elephant.leg.br.hoof",
      static_cast<Render::Creature::BoneIndex>(ElephantBone::ShoulderBR),
      static_cast<Render::Creature::BoneIndex>(ElephantBone::KneeBR),
      static_cast<Render::Creature::BoneIndex>(ElephantBone::FootBR), false);
  return out;
}

const auto k_elephant_full_leg_overlays = build_elephant_full_leg_overlays();

auto elephant_topology_storage() noexcept
    -> const Render::Creature::Quadruped::TopologyStorage & {
  static const auto storage = [] {
    Render::Creature::Quadruped::TopologyOptions options;
    options.include_neck_top = false;
    options.include_appendage_tip = true;
    options.appendage_tip_name = "TrunkTip";
    return Render::Creature::Quadruped::make_topology(options);
  }();
  return storage;
}

auto elephant_topology_ref() noexcept
    -> const Render::Creature::SkeletonTopology & {
  static const auto topology = elephant_topology_storage().topology();
  return topology;
}

void bake_elephant_manifest_clip_palettes(
    std::size_t clip_index, std::uint32_t frame_index,
    std::vector<QMatrix4x4> &out_palettes) {
  auto const &clip = k_elephant_clips[clip_index];
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  float const phase =
      static_cast<float>(frame_index) /
      static_cast<float>(std::max<std::uint32_t>(clip.desc.frame_count, 1U));

  Render::Elephant::ElephantPoseMotion motion{};
  motion.phase = phase;
  motion.is_moving = clip.is_moving;
  motion.is_fighting = clip.is_fighting;
  motion.anim_time = phase * clip.gait.cycle_time;
  if (!clip.is_fighting) {
    float const amp =
        clip.is_moving ? dims.move_bob_amplitude : dims.idle_bob_amplitude;
    float const scale = clip.is_moving ? clip.bob_scale : 0.8F;
    motion.bob =
        std::sin(phase * 2.0F * std::numbers::pi_v<float>) * amp * scale;
  }

  Render::Elephant::ElephantSpecPose pose{};
  Render::Elephant::make_elephant_spec_pose_animated(dims, clip.gait, motion,
                                                     pose);

  Render::Elephant::BonePalette palette{};
  Render::Elephant::evaluate_elephant_skeleton(pose, palette);
  if (clip.is_death) {
    float death_phase =
        clip.is_dead_hold ? 1.0F : std::clamp(phase, 0.0F, 1.0F);
    QMatrix4x4 death_transform;
    death_transform.translate(0.0F, -0.42F * death_phase, -0.12F * death_phase);
    death_transform.rotate(72.0F * death_phase, 1.0F, 0.0F, 0.0F);
    death_transform.rotate(10.0F * death_phase, 0.0F, 0.0F, 1.0F);
    for (auto &bone : palette) {
      bone = death_transform * bone;
    }
  }
  out_palettes.insert(out_palettes.end(), palette.begin(), palette.end());
}

} // namespace

auto elephant_manifest() noexcept -> const Render::Creature::SpeciesManifest & {
  static const Render::Creature::SpeciesManifest manifest = [] {
    Render::Creature::SpeciesManifest m;
    m.species_name = "elephant";
    m.species_id = Render::Creature::Bpat::k_species_elephant;
    m.bpat_file_name = "elephant.bpat";
    m.minimal_snapshot_file_name = "elephant_minimal.bpsm";
    m.topology = &elephant_topology_ref();
    m.lod_full.primitive_name = "elephant.full.body";
    m.lod_full.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(ElephantBone::Root);
    m.lod_full.mesh_skinning = Render::Creature::MeshSkinning::ElephantWhole;
    m.lod_full.color_role = k_role_skin;
    m.lod_full.material_id = k_elephant_material_id;
    m.lod_full.lod_mask = Render::Creature::k_lod_full;
    m.lod_full.mesh_nodes =
        std::span<const Render::Creature::Quadruped::MeshNode>(
            k_elephant_whole_nodes);
    m.lod_full.excluded_node_name_prefixes = k_elephant_full_excluded_prefixes;
    m.lod_full.overlay_primitives = k_elephant_full_leg_overlays;
    m.lod_minimal.primitive_name = "elephant.minimal.whole";
    m.lod_minimal.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(ElephantBone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::ElephantWhole;
    m.lod_minimal.color_role = k_role_skin;
    m.lod_minimal.material_id = k_elephant_material_id;
    m.lod_minimal.lod_mask = Render::Creature::k_lod_minimal;
    m.lod_minimal.mesh_nodes =
        std::span<const Render::Creature::Quadruped::MeshNode>(
            k_elephant_whole_nodes);
    m.clips = std::span<const Render::Creature::BakeClipDescriptor>(
        k_elephant_clip_descs);
    m.bind_palette = &elephant_bind_palette;
    m.creature_spec = &elephant_creature_spec;
    m.bake_clip_palette = &bake_elephant_manifest_clip_palettes;
    return m;
  }();
  return manifest;
}

} // namespace Render::Elephant
