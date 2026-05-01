#include "horse_manifest.h"

#include "../creature/bpat/bpat_format.h"
#include "../gl/primitives.h"
#include "../horse/dimensions.h"
#include "horse_gait.h"
#include "horse_layout.h"
#include "horse_spec.h"

#include <QMatrix4x4>

#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <span>
#include <vector>

namespace Render::Horse {

namespace {

constexpr std::uint8_t k_role_coat = 1;
constexpr std::uint8_t k_role_hoof = 4;
constexpr std::uint8_t k_role_eye = 7;
constexpr int k_horse_material_id = 6;
constexpr float k_horse_torso_width_scale = 2.68F;
constexpr float k_horse_torso_section_length_scale = 0.80F;
constexpr float k_horse_neck_width_scale = 0.95F;
constexpr float k_horse_neck_height_scale = 0.87F;
constexpr float k_horse_head_length_scale = 1.00F;
constexpr float k_horse_leg_length_scale = 1.08F;
constexpr float k_horse_leg_thickness_scale = 2.35F;
constexpr int k_horse_leg_radial_segments = 6;

struct HorseClipSpec {
  Render::Creature::BakeClipDescriptor desc;
  Render::GL::GaitType gait;
  bool is_moving;
  bool is_fighting{false};
  float bob_scale{0.0F};
};

const std::array<HorseClipSpec, 6> k_horse_clips{{
    {{"idle", 24U, 24.0F, true},
     Render::GL::GaitType::IDLE,
     false,
     false,
     0.0F},
    {{"walk", 24U, 24.0F, true},
     Render::GL::GaitType::WALK,
     true,
     false,
     0.50F},
    {{"trot", 16U, 24.0F, true},
     Render::GL::GaitType::TROT,
     true,
     false,
     0.85F},
    {{"canter", 16U, 24.0F, true},
     Render::GL::GaitType::CANTER,
     true,
     false,
     1.00F},
    {{"gallop", 12U, 24.0F, true},
     Render::GL::GaitType::GALLOP,
     true,
     false,
     1.12F},
    {{"fight", 24U, 24.0F, true},
     Render::GL::GaitType::IDLE,
     false,
     true,
     0.0F},
}};

const std::array<Render::Creature::BakeClipDescriptor, k_horse_clips.size()>
    k_horse_clip_descs{{
        k_horse_clips[0].desc,
        k_horse_clips[1].desc,
        k_horse_clips[2].desc,
        k_horse_clips[3].desc,
        k_horse_clips[4].desc,
        k_horse_clips[5].desc,
    }};

struct TorsoSectionRing {
  float z{};
  float y{};
  float half_width{};
  float top{};
  float bottom{};
  float shoulder_ratio{0.72F};
  float belly_ratio{0.74F};
};

struct MuzzleRingProfile {
  float z{};
  float y{};
  float half_width{};
  float top{};
  float bottom{};
};

template <std::size_t N>
void scale_torso_section_length(std::array<TorsoSectionRing, N> &rings) {
  float const center_z = (rings.front().z + rings.back().z) * 0.5F;
  for (TorsoSectionRing &ring : rings) {
    ring.z =
        center_z + (ring.z - center_z) * k_horse_torso_section_length_scale;
  }
}

auto build_horse_torso_section_mesh(std::span<const TorsoSectionRing> rings,
                                    bool cap_rear, bool cap_front)
    -> Render::Creature::Quadruped::CustomMeshNode {
  using Render::GL::Vertex;

  auto append_vertex = [](std::vector<Vertex> &vertices, QVector3D const &p,
                          QVector3D n) {
    if (n.lengthSquared() <= 1.0e-8F) {
      n = QVector3D(0.0F, 1.0F, 0.0F);
    } else {
      n.normalize();
    }
    vertices.push_back(
        {{p.x(), p.y(), p.z()}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});
  };

  Render::Creature::Quadruped::CustomMeshNode node;
  node.vertices.reserve(rings.size() * 8U + 2U);
  node.indices.reserve((rings.size() - 1U) * 48U + (cap_rear ? 24U : 0U) +
                       (cap_front ? 24U : 0U));

  for (TorsoSectionRing const &ring : rings) {
    float const shoulder_half_width = ring.half_width * ring.shoulder_ratio;
    float const belly_half_width = ring.half_width * ring.belly_ratio;
    std::array<QVector3D, 8> const points{{
        {0.0F, ring.y + ring.top, ring.z},
        {shoulder_half_width, ring.y + ring.top * 0.60F, ring.z},
        {ring.half_width, ring.y + ring.top * 0.08F, ring.z},
        {belly_half_width, ring.y - ring.bottom * 0.22F, ring.z},
        {0.0F, ring.y - ring.bottom * 0.44F, ring.z},
        {-belly_half_width, ring.y - ring.bottom * 0.22F, ring.z},
        {-ring.half_width, ring.y + ring.top * 0.08F, ring.z},
        {-shoulder_half_width, ring.y + ring.top * 0.60F, ring.z},
    }};
    for (QVector3D const &point : points) {
      append_vertex(node.vertices, point,
                    QVector3D(point.x(), point.y() - ring.y, 0.0F));
    }
  }

  constexpr unsigned int ring_vertices = 8U;
  for (unsigned int ring = 0U; ring + 1U < rings.size(); ++ring) {
    unsigned int const base = ring * ring_vertices;
    unsigned int const next = (ring + 1U) * ring_vertices;
    for (unsigned int i = 0U; i < ring_vertices; ++i) {
      unsigned int const a = base + i;
      unsigned int const b = base + ((i + 1U) % ring_vertices);
      unsigned int const c = next + ((i + 1U) % ring_vertices);
      unsigned int const d = next + i;
      node.indices.insert(node.indices.end(), {a, b, c, c, d, a});
    }
  }

  auto add_cap = [&](unsigned int ring_index, bool front) {
    TorsoSectionRing const &profile = rings[ring_index];
    unsigned int const center = static_cast<unsigned int>(node.vertices.size());
    append_vertex(node.vertices,
                  QVector3D(0.0F,
                            profile.y + (profile.top - profile.bottom) * 0.16F,
                            profile.z),
                  QVector3D(0.0F, 0.0F, front ? 1.0F : -1.0F));
    unsigned int const base = ring_index * ring_vertices;
    for (unsigned int i = 0U; i < ring_vertices; ++i) {
      unsigned int const a = base + i;
      unsigned int const b = base + ((i + 1U) % ring_vertices);
      if (front) {
        node.indices.insert(node.indices.end(), {center, a, b});
      } else {
        node.indices.insert(node.indices.end(), {center, b, a});
      }
    }
  };

  if (cap_rear) {
    add_cap(0U, false);
  }
  if (cap_front) {
    add_cap(static_cast<unsigned int>(rings.size() - 1U), true);
  }

  return node;
}

auto build_horse_muzzle_mesh(std::span<const MuzzleRingProfile> rings)
    -> Render::Creature::Quadruped::CustomMeshNode {
  using Render::GL::Vertex;

  auto append_vertex = [](std::vector<Vertex> &vertices, QVector3D const &p,
                          QVector3D n) {
    if (n.lengthSquared() <= 1.0e-8F) {
      n = QVector3D(0.0F, 1.0F, 0.0F);
    } else {
      n.normalize();
    }
    vertices.push_back(
        {{p.x(), p.y(), p.z()}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}});
  };

  Render::Creature::Quadruped::CustomMeshNode node;
  constexpr std::size_t k_ring_vertices = 9U;
  node.vertices.reserve(rings.size() * k_ring_vertices + 2U);
  node.indices.reserve((rings.size() - 1U) * k_ring_vertices * 6U + 54U);

  for (MuzzleRingProfile const &ring : rings) {
    float const upper_width = ring.half_width * 0.56F;
    float const jaw_width = ring.half_width * 0.62F;
    float const chin_width = ring.half_width * 0.10F;
    std::array<QVector3D, k_ring_vertices> const points{{
        {0.0F, ring.y + ring.top, ring.z},
        {upper_width, ring.y + ring.top * 0.58F, ring.z},
        {ring.half_width, ring.y + ring.top * 0.06F, ring.z},
        {jaw_width, ring.y - ring.bottom * 0.18F, ring.z},
        {chin_width, ring.y - ring.bottom, ring.z},
        {-chin_width, ring.y - ring.bottom, ring.z},
        {-jaw_width, ring.y - ring.bottom * 0.18F, ring.z},
        {-ring.half_width, ring.y + ring.top * 0.06F, ring.z},
        {-upper_width, ring.y + ring.top * 0.58F, ring.z},
    }};
    for (QVector3D const &point : points) {
      append_vertex(node.vertices, point,
                    QVector3D(point.x(), point.y() - ring.y, 0.0F));
    }
  }

  auto append_cap = [&](std::size_t ring_index, bool front_facing) {
    std::size_t const base = ring_index * k_ring_vertices;
    MuzzleRingProfile const &profile = rings[ring_index];
    QVector3D const center(0.0F,
                           profile.y + (profile.top - profile.bottom) *
                                           (front_facing ? 0.34F : 0.20F),
                           profile.z);
    append_vertex(node.vertices, center,
                  front_facing ? QVector3D(0.0F, 0.0F, 1.0F)
                               : QVector3D(0.0F, 0.0F, -1.0F));
    unsigned int const center_index =
        static_cast<unsigned int>(node.vertices.size() - 1U);
    for (std::size_t i = 0; i < k_ring_vertices; ++i) {
      unsigned int const a = static_cast<unsigned int>(base + i);
      unsigned int const b =
          static_cast<unsigned int>(base + ((i + 1U) % k_ring_vertices));
      if (front_facing) {
        node.indices.insert(node.indices.end(), {center_index, a, b});
      } else {
        node.indices.insert(node.indices.end(), {center_index, b, a});
      }
    }
  };

  for (std::size_t ring_index = 0; ring_index + 1U < rings.size();
       ++ring_index) {
    std::size_t const base = ring_index * k_ring_vertices;
    std::size_t const next = base + k_ring_vertices;
    for (std::size_t i = 0; i < k_ring_vertices; ++i) {
      unsigned int const a = static_cast<unsigned int>(base + i);
      unsigned int const b =
          static_cast<unsigned int>(base + ((i + 1U) % k_ring_vertices));
      unsigned int const c =
          static_cast<unsigned int>(next + ((i + 1U) % k_ring_vertices));
      unsigned int const d = static_cast<unsigned int>(next + i);
      node.indices.insert(node.indices.end(), {a, b, c, c, d, a});
    }
  }

  append_cap(0U, false);
  append_cap(rings.size() - 1U, true);
  return node;
}

auto build_horse_whole_nodes()
    -> std::vector<Render::Creature::Quadruped::MeshNode> {
  using namespace Render::Creature;
  using namespace Render::Creature::Quadruped;

  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  float const bw = horse_body_visual_width(dims);
  float const bh = horse_body_visual_height(dims);
  float const bl = horse_body_visual_length(dims);
  float const hw = horse_head_visual_width(dims);
  float const hh = horse_head_visual_height(dims);
  float const hl = horse_head_visual_length(dims);
  float const torso_lift = horse_torso_lift(dims);

  std::vector<MeshNode> nodes;
  nodes.reserve(20);

  std::array<TorsoSectionRing, 5> rear_rings{{
      {-bl * 0.54F, bh * 0.44F + torso_lift,
       bw * 0.24F * k_horse_torso_width_scale,
       bh * 0.2970F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       bh * 0.1700F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       0.76F, 0.54F},
      {-bl * 0.44F, bh * 0.45F + torso_lift,
       bw * 0.316F * k_horse_torso_width_scale,
       bh * 0.3080F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       bh * 0.2700F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       0.80F, 0.62F},
      {-bl * 0.32F, bh * 0.46F + torso_lift,
       bw * 0.344F * k_horse_torso_width_scale,
       bh * 0.3320F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       bh * 0.2900F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       0.80F, 0.62F},
      {-bl * 0.19F, bh * 0.43F + torso_lift,
       bw * 0.320F * k_horse_torso_width_scale,
       bh * 0.2960F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       bh * 0.2500F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       0.78F, 0.58F},
      {-bl * 0.060F, bh * 0.42F + torso_lift,
       bw * 0.304F * k_horse_torso_width_scale,
       bh * 0.3905F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       bh * 0.2350F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       0.76F, 0.56F},
  }};
  scale_torso_section_length(rear_rings);
  nodes.push_back({"horse.body.rear", static_cast<BoneIndex>(HorseBone::Body),
                   k_role_coat, kLodAll, 0,
                   build_horse_torso_section_mesh(rear_rings, true, false)});

  std::array<TorsoSectionRing, 3> mid_rings{{
      {-bl * 0.22F, bh * 0.40F + torso_lift,
       bw * 0.336F * k_horse_torso_width_scale,
       bh * 0.3920F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       bh * 0.3200F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       0.78F, 0.66F},
      {bl * 0.02F, bh * 0.42F + torso_lift,
       bw * 0.348F * k_horse_torso_width_scale,
       bh * 0.4040F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       bh * 0.3160F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       0.80F, 0.68F},
      {bl * 0.26F, bh * 0.46F + torso_lift,
       bw * 0.336F * k_horse_torso_width_scale,
       bh * 0.3720F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       bh * 0.3000F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       0.78F, 0.64F},
  }};
  scale_torso_section_length(mid_rings);
  nodes.push_back({"horse.body.mid", static_cast<BoneIndex>(HorseBone::Body),
                   k_role_coat, kLodAll, 0,
                   build_horse_torso_section_mesh(mid_rings, false, false)});

  std::array<TorsoSectionRing, 3> front_rings{{
      {bl * 0.10F, bh * 0.42F + torso_lift,
       bw * 0.312F * k_horse_torso_width_scale,
       bh * 0.4000F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       bh * 0.3400F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       0.80F, 0.62F},
      {bl * 0.34F, bh * 0.48F + torso_lift,
       bw * 0.284F * k_horse_torso_width_scale,
       bh * 0.4600F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       bh * 0.3200F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       0.82F, 0.58F},
      {bl * 0.58F, bh * 0.56F + torso_lift,
       bw * 0.24F * k_horse_torso_width_scale,
       bh * 0.4200F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       bh * 0.2400F * k_horse_torso_height_scale *
           k_horse_torso_mesh_height_scale,
       0.84F, 0.48F},
  }};
  scale_torso_section_length(front_rings);
  nodes.push_back({"horse.body.front", static_cast<BoneIndex>(HorseBone::Body),
                   k_role_coat, kLodAll, 0,
                   build_horse_torso_section_mesh(front_rings, false, true)});

  TubeNode neck;
  neck.start = horse_neck_base_local(dims);
  neck.end = horse_neck_top_local(dims);
  neck.start_radius = 0.40F * (bw * 0.34F * k_horse_neck_width_scale +
                               bh * 0.34F * k_horse_neck_height_scale);
  neck.end_radius = 0.40F * (bw * 0.20F * k_horse_neck_width_scale +
                             bh * 0.19F * k_horse_neck_height_scale);
  neck.segment_count = 5U;
  neck.ring_vertices = 8U;
  nodes.push_back({"horse.neck", static_cast<BoneIndex>(HorseBone::NeckTop),
                   k_role_coat, kLodAll, 0, neck});

  float const head_back_z = neck.end.z();
  float const head_base_y = neck.end.y();
  float const head_mid_z = head_back_z + hl * 0.40F * k_horse_head_length_scale;
  float const head_front_z =
      head_back_z + hl * 0.82F * k_horse_head_length_scale;

  std::array<MuzzleRingProfile, 5> const head_rings{{
      {head_back_z, head_base_y + hh * 0.06F, hw * 0.56F, hh * 0.28F,
       hh * 0.12F},
      {head_back_z + hl * 0.18F, head_base_y + hh * 0.16F, hw * 0.78F,
       hh * 0.68F, hh * 0.20F},
      {head_mid_z, head_base_y + hh * 0.12F, hw * 0.64F, hh * 0.54F,
       hh * 0.14F},
      {head_back_z + hl * 0.72F, head_base_y + hh * 0.08F, hw * 0.30F,
       hh * 0.22F, hh * 0.06F},
      {head_front_z + hl * 0.08F, head_base_y + hh * 0.04F, hw * 0.12F,
       hh * 0.10F, hh * 0.03F},
  }};
  nodes.push_back({"horse.head.upper", static_cast<BoneIndex>(HorseBone::Head),
                   k_role_coat, kLodAll, 0,
                   build_horse_muzzle_mesh(head_rings)});

  auto add_eye = [&](float side) {
    EllipsoidNode eye;
    eye.center = QVector3D(-side * hw * 0.56F, head_base_y + hh * 0.48F,
                           head_back_z + hl * 0.28F);
    eye.radii = QVector3D(hw * 0.055F, hh * 0.090F, hl * 0.080F);
    eye.ring_count = 4U;
    eye.ring_vertices = 6U;
    nodes.push_back({"horse.eye", static_cast<BoneIndex>(HorseBone::Head),
                     k_role_eye, kLodAll, 0, eye});
  };
  add_eye(1.0F);
  add_eye(-1.0F);

  auto add_leg = [&](float x, float z, BoneIndex foot_bone) {
    ColumnLegNode leg;
    leg.top_center = QVector3D(x, bh * (z < 0.0F ? 0.46F : 0.50F), z);
    leg.bottom_y = -dims.leg_length * k_horse_visual_scale * 0.84F *
                   k_horse_leg_length_scale;
    leg.top_radius_x = dims.body_width * k_horse_visual_scale *
                       (z < 0.0F ? 0.28F : 0.25F) *
                       k_horse_leg_thickness_scale * 2.0F;
    leg.top_radius_z = leg.top_radius_x * 0.86F;
    leg.shaft_taper = 0.50F;
    leg.foot_radius_scale = 1.45F;
    nodes.push_back({"horse.leg", foot_bone, k_role_coat, kLodAll, 0, leg});
  };
  add_leg(bw * 0.20F, bl * 0.55F, static_cast<BoneIndex>(HorseBone::FootFL));
  add_leg(-bw * 0.20F, bl * 0.55F, static_cast<BoneIndex>(HorseBone::FootFR));
  add_leg(bw * 0.28F, -bl * 0.34F, static_cast<BoneIndex>(HorseBone::FootBL));
  add_leg(-bw * 0.28F, -bl * 0.34F, static_cast<BoneIndex>(HorseBone::FootBR));

  FlatFanNode ear_l;
  ear_l.outline = {
      {hw * 0.176F, head_base_y + hh * 0.28F, head_back_z + hl * 0.02F},
      {hw * 0.352F, head_base_y + hh * 0.76F, head_back_z - hl * 0.08F},
      {hw * 0.064F, head_base_y + hh * 0.42F, head_back_z - hl * 0.24F},
  };
  ear_l.thickness_axis = QVector3D(0.0F, 0.0F, 1.0F);
  ear_l.thickness = hw * 0.052F;
  nodes.push_back({"horse.ear.l", static_cast<BoneIndex>(HorseBone::Head),
                   k_role_coat, kLodAll, 0, ear_l});

  FlatFanNode ear_r = ear_l;
  for (QVector3D &p : ear_r.outline) {
    p.setX(-p.x());
  }
  nodes.push_back({"horse.ear.r", static_cast<BoneIndex>(HorseBone::Head),
                   k_role_coat, kLodAll, 0, ear_r});

  ConeNode tail_dock;
  tail_dock.base_center = horse_tail_base_local(dims);
  tail_dock.tip = horse_tail_tip_local(dims);
  tail_dock.base_radius = bw * 0.048F;
  tail_dock.ring_vertices = 5U;
  nodes.push_back({"horse.tail.dock", static_cast<BoneIndex>(HorseBone::Body),
                   k_role_coat, kLodAll, 0, tail_dock});

  FlatFanNode tail_hair;
  tail_hair.outline = {
      horse_tail_tip_local(dims),
      {0.0F, bh * 1.30F, -bl * 0.72F},
      {0.0F, bh * 1.06F, -bl * 0.80F},
      {0.0F, bh * 1.26F, -bl * 0.70F},
  };
  tail_hair.thickness_axis = QVector3D(1.0F, 0.0F, 0.0F);
  tail_hair.thickness = bw * 0.064F;
  nodes.push_back({"horse.tail.hair", static_cast<BoneIndex>(HorseBone::Body),
                   k_role_coat, kLodAll, 0, tail_hair});

  return nodes;
}

const auto k_horse_whole_nodes = build_horse_whole_nodes();
const std::array<std::string_view, 1> k_horse_full_excluded_prefixes{
    "horse.leg"};

struct LegRingProfile {
  float y{};
  float x_shift{};
  float z_shift{};
  float x_outer{};
  float x_inner{};
  float z_front{};
  float z_back{};
};

auto build_horse_leg_span_mesh(std::span<const LegRingProfile> rings)
    -> std::unique_ptr<Render::GL::Mesh> {
  constexpr int k_ring_vertices = 8;
  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(rings.size() * k_ring_vertices + 2U);
  indices.reserve((rings.size() - 1U) * k_ring_vertices * 6U +
                  k_ring_vertices * 6U);

  auto append_vertex = [&](float x, float y, float z, float u, float v) {
    QVector3D normal(x, 0.0F, z);
    if (normal.lengthSquared() < 1e-6F) {
      normal = QVector3D(0.0F, 1.0F, 0.0F);
    } else {
      normal.normalize();
    }
    vertices.push_back(
        {{x, y, z}, {normal.x(), normal.y(), normal.z()}, {u, v}});
  };

  for (LegRingProfile const &ring : rings) {
    append_vertex(ring.x_shift + ring.x_outer * 0.62F, ring.y,
                  ring.z_shift + ring.z_front * 0.88F, 0.00F, ring.y + 0.5F);
    append_vertex(ring.x_shift, ring.y, ring.z_shift + ring.z_front, 0.14F,
                  ring.y + 0.5F);
    append_vertex(ring.x_shift - ring.x_inner * 0.62F, ring.y,
                  ring.z_shift + ring.z_front * 0.88F, 0.28F, ring.y + 0.5F);
    append_vertex(ring.x_shift - ring.x_inner, ring.y, ring.z_shift, 0.42F,
                  ring.y + 0.5F);
    append_vertex(ring.x_shift - ring.x_inner * 0.60F, ring.y,
                  ring.z_shift - ring.z_back * 0.86F, 0.56F, ring.y + 0.5F);
    append_vertex(ring.x_shift, ring.y, ring.z_shift - ring.z_back, 0.70F,
                  ring.y + 0.5F);
    append_vertex(ring.x_shift + ring.x_outer * 0.60F, ring.y,
                  ring.z_shift - ring.z_back * 0.86F, 0.84F, ring.y + 0.5F);
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
    LegRingProfile const &profile = rings[ring];
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

auto horse_front_thigh_mesh() noexcept -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> mesh =
      build_horse_leg_span_mesh(std::array<LegRingProfile, 4>{{
          {-0.50F, 0.00F, 0.02F, 0.60F, 0.54F, 0.32F, 0.22F},
          {-0.18F, 0.00F, 0.01F, 0.46F, 0.40F, 0.28F, 0.18F},
          {0.16F, 0.00F, 0.00F, 0.28F, 0.24F, 0.20F, 0.14F},
          {0.50F, 0.00F, 0.00F, 0.11F, 0.10F, 0.12F, 0.08F},
      }});
  return mesh.get();
}

auto horse_rear_thigh_mesh() noexcept -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> mesh =
      build_horse_leg_span_mesh(std::array<LegRingProfile, 4>{{
          {-0.50F, 0.00F, 0.05F, 0.68F, 0.60F, 0.40F, 0.22F},
          {-0.18F, 0.00F, 0.03F, 0.50F, 0.44F, 0.30F, 0.18F},
          {0.16F, 0.00F, 0.01F, 0.30F, 0.26F, 0.22F, 0.12F},
          {0.50F, 0.00F, 0.00F, 0.12F, 0.11F, 0.12F, 0.08F},
      }});
  return mesh.get();
}

auto horse_front_calf_mesh() noexcept -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> mesh =
      build_horse_leg_span_mesh(std::array<LegRingProfile, 4>{{
          {-0.50F, 0.00F, 0.02F, 0.24F, 0.22F, 0.28F, 0.24F},
          {-0.16F, 0.00F, 0.01F, 0.21F, 0.20F, 0.25F, 0.22F},
          {0.18F, 0.00F, 0.00F, 0.18F, 0.17F, 0.22F, 0.20F},
          {0.50F, 0.00F, -0.01F, 0.16F, 0.15F, 0.19F, 0.18F},
      }});
  return mesh.get();
}

auto horse_rear_calf_mesh() noexcept -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> mesh =
      build_horse_leg_span_mesh(std::array<LegRingProfile, 4>{{
          {-0.50F, 0.00F, -0.02F, 0.25F, 0.23F, 0.25F, 0.31F},
          {-0.16F, 0.00F, -0.01F, 0.22F, 0.20F, 0.23F, 0.27F},
          {0.16F, 0.00F, 0.00F, 0.18F, 0.17F, 0.20F, 0.23F},
          {0.50F, 0.00F, 0.00F, 0.16F, 0.15F, 0.18F, 0.20F},
      }});
  return mesh.get();
}

auto horse_hoof_mesh() noexcept -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> mesh = [] {
    std::vector<Render::GL::Vertex> vertices{
        {{-0.72F, -0.55F, -0.60F}, {0.0F, -1.0F, 0.0F}, {0.0F, 0.0F}},
        {{0.72F, -0.55F, -0.60F}, {0.0F, -1.0F, 0.0F}, {1.0F, 0.0F}},
        {{0.88F, -0.55F, 0.70F}, {0.0F, -1.0F, 0.0F}, {1.0F, 1.0F}},
        {{-0.88F, -0.55F, 0.70F}, {0.0F, -1.0F, 0.0F}, {0.0F, 1.0F}},
        {{-0.52F, 0.45F, -0.48F}, {-0.5F, 0.5F, -0.2F}, {0.0F, 0.0F}},
        {{0.52F, 0.45F, -0.48F}, {0.5F, 0.5F, -0.2F}, {1.0F, 0.0F}},
        {{0.64F, 0.45F, 0.42F}, {0.5F, 0.5F, 0.2F}, {1.0F, 1.0F}},
        {{-0.64F, 0.45F, 0.42F}, {-0.5F, 0.5F, 0.2F}, {0.0F, 1.0F}},
    };
    std::vector<unsigned int> indices{0, 1, 2, 2, 3, 0, 4, 7, 6, 6, 5, 4,
                                      0, 4, 5, 5, 1, 0, 1, 5, 6, 6, 2, 1,
                                      2, 6, 7, 7, 3, 2, 3, 7, 4, 4, 0, 3};
    return std::make_unique<Render::GL::Mesh>(vertices, indices);
  }();
  return mesh.get();
}

auto horse_hoof_center_y(bool front,
                         Render::GL::HorseDimensions const &dims) -> float {
  return dims.hoof_height * (front ? 0.33F : 0.31F);
}

auto horse_hoof_half_extents(
    bool front, Render::GL::HorseDimensions const &dims) -> QVector3D {
  return QVector3D(dims.body_width * (front ? 0.046F : 0.044F) * 2.0F,
                   dims.hoof_height * 0.36F,
                   dims.body_width * (front ? 0.078F : 0.072F) * 2.0F);
}

auto build_horse_full_leg_overlays()
    -> std::array<Render::Creature::PrimitiveInstance, 12> {
  using Render::Creature::PrimitiveInstance;
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
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
        QVector3D(0.0F, dims.body_height * (front ? 0.86F : 1.02F),
                  dims.body_length * (front ? 0.070F : -0.060F));
    thigh.params.tail_offset =
        QVector3D(0.0F, dims.leg_length * (front ? 0.08F : 0.10F),
                  dims.body_length * (front ? 0.040F : 0.100F));
    thigh.params.radius = dims.body_width * (front ? 0.690F : 0.780F) * 2.0F;
    thigh.params.depth_radius =
        dims.body_width * (front ? 1.050F : 1.200F) * 2.0F;
    thigh.custom_mesh =
        front ? horse_front_thigh_mesh() : horse_rear_thigh_mesh();
    thigh.color_role = k_role_coat;
    thigh.material_id = k_horse_material_id;
    thigh.lod_mask = Render::Creature::kLodFull;

    PrimitiveInstance &calf = out[idx++];
    calf.debug_name = calf_name;
    calf.shape = Render::Creature::PrimitiveShape::BoneSpanMesh;
    calf.params.anchor_bone = knee_bone;
    calf.params.tail_bone = foot_bone;
    calf.params.head_offset =
        QVector3D(0.0F, dims.leg_length * 0.04F,
                  dims.body_length * (front ? 0.018F : 0.020F));
    calf.params.tail_offset =
        QVector3D(0.0F, dims.hoof_height * 0.02F,
                  dims.body_length * (front ? 0.010F : 0.020F));
    calf.params.radius = dims.body_width * (front ? 0.290F : 0.310F) * 2.0F;
    calf.params.depth_radius =
        dims.body_width * (front ? 0.410F : 0.450F) * 2.0F;
    calf.custom_mesh = front ? horse_front_calf_mesh() : horse_rear_calf_mesh();
    calf.color_role = k_role_coat;
    calf.material_id = k_horse_material_id;
    calf.lod_mask = Render::Creature::kLodFull;

    PrimitiveInstance &hoof = out[idx++];
    hoof.debug_name = hoof_name;
    hoof.shape = Render::Creature::PrimitiveShape::Mesh;
    hoof.params.anchor_bone = foot_bone;
    hoof.params.head_offset =
        QVector3D(0.0F, horse_hoof_center_y(front, dims),
                  (front ? 1.0F : -1.0F) * dims.body_width * 0.010F);
    hoof.params.half_extents = horse_hoof_half_extents(front, dims);
    hoof.custom_mesh = horse_hoof_mesh();
    hoof.color_role = k_role_hoof;
    hoof.material_id = k_horse_material_id;
    hoof.lod_mask = Render::Creature::kLodFull;
  };

  add_leg("horse.leg.fl.thigh", "horse.leg.fl.calf", "horse.leg.fl.hoof",
          static_cast<Render::Creature::BoneIndex>(HorseBone::ShoulderFL),
          static_cast<Render::Creature::BoneIndex>(HorseBone::KneeFL),
          static_cast<Render::Creature::BoneIndex>(HorseBone::FootFL), true);
  add_leg("horse.leg.fr.thigh", "horse.leg.fr.calf", "horse.leg.fr.hoof",
          static_cast<Render::Creature::BoneIndex>(HorseBone::ShoulderFR),
          static_cast<Render::Creature::BoneIndex>(HorseBone::KneeFR),
          static_cast<Render::Creature::BoneIndex>(HorseBone::FootFR), true);
  add_leg("horse.leg.bl.thigh", "horse.leg.bl.calf", "horse.leg.bl.hoof",
          static_cast<Render::Creature::BoneIndex>(HorseBone::ShoulderBL),
          static_cast<Render::Creature::BoneIndex>(HorseBone::KneeBL),
          static_cast<Render::Creature::BoneIndex>(HorseBone::FootBL), false);
  add_leg("horse.leg.br.thigh", "horse.leg.br.calf", "horse.leg.br.hoof",
          static_cast<Render::Creature::BoneIndex>(HorseBone::ShoulderBR),
          static_cast<Render::Creature::BoneIndex>(HorseBone::KneeBR),
          static_cast<Render::Creature::BoneIndex>(HorseBone::FootBR), false);
  return out;
}

const auto k_horse_full_leg_overlays = build_horse_full_leg_overlays();

void bake_horse_manifest_clip_palettes(std::size_t clip_index,
                                       std::uint32_t frame_index,
                                       std::vector<QMatrix4x4> &out_palettes) {
  auto const &clip = k_horse_clips[clip_index];
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  float const phase =
      static_cast<float>(frame_index) /
      static_cast<float>(std::max<std::uint32_t>(clip.desc.frame_count, 1U));

  Render::GL::HorseGait base{};
  Render::GL::HorseGait const gait = Render::GL::gait_for_type(clip.gait, base);

  Render::Horse::HorsePoseMotion motion{};
  motion.phase = phase;
  motion.is_moving = clip.is_moving;
  motion.is_fighting = clip.is_fighting;
  if (!clip.is_fighting) {
    float const amp =
        clip.is_moving ? dims.move_bob_amplitude : dims.idle_bob_amplitude;
    float const scale = clip.is_moving ? clip.bob_scale : 0.8F;
    motion.bob =
        std::sin(phase * 2.0F * std::numbers::pi_v<float>) * amp * scale;
  }

  Render::Horse::HorseSpecPose pose{};
  Render::Horse::make_horse_spec_pose_animated(dims, gait, motion, pose);

  Render::Horse::BonePalette palette{};
  Render::Horse::evaluate_horse_skeleton(pose, palette);
  out_palettes.insert(out_palettes.end(), palette.begin(), palette.end());
}

} // namespace

auto horse_manifest() noexcept -> const Render::Creature::SpeciesManifest & {
  static const Render::Creature::SpeciesManifest manifest = [] {
    Render::Creature::SpeciesManifest m;
    m.species_name = "horse";
    m.species_id = Render::Creature::Bpat::kSpeciesHorse;
    m.bpat_file_name = "horse.bpat";
    m.minimal_snapshot_file_name = "horse_minimal.bpsm";
    m.topology = &horse_topology();
    m.lod_full.primitive_name = "horse.full.body";
    m.lod_full.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(HorseBone::Root);
    m.lod_full.mesh_skinning = Render::Creature::MeshSkinning::HorseWhole;
    m.lod_full.color_role = k_role_coat;
    m.lod_full.material_id = k_horse_material_id;
    m.lod_full.lod_mask = Render::Creature::kLodFull;
    m.lod_full.mesh_nodes =
        std::span<const Render::Creature::Quadruped::MeshNode>(
            k_horse_whole_nodes);
    m.lod_full.excluded_node_name_prefixes = k_horse_full_excluded_prefixes;
    m.lod_full.overlay_primitives = k_horse_full_leg_overlays;
    m.lod_minimal.primitive_name = "horse.minimal.whole";
    m.lod_minimal.anchor_bone =
        static_cast<Render::Creature::BoneIndex>(HorseBone::Root);
    m.lod_minimal.mesh_skinning = Render::Creature::MeshSkinning::HorseWhole;
    m.lod_minimal.color_role = k_role_coat;
    m.lod_minimal.material_id = k_horse_material_id;
    m.lod_minimal.lod_mask = Render::Creature::kLodMinimal;
    m.lod_minimal.mesh_nodes =
        std::span<const Render::Creature::Quadruped::MeshNode>(
            k_horse_whole_nodes);
    m.clips = std::span<const Render::Creature::BakeClipDescriptor>(
        k_horse_clip_descs);
    m.bind_palette = &horse_bind_palette;
    m.creature_spec = &horse_creature_spec;
    m.bake_clip_palette = &bake_horse_manifest_clip_palettes;
    return m;
  }();
  return manifest;
}

} // namespace Render::Horse
