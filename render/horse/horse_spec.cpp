#include "horse_spec.h"

#include "../creature/part_graph.h"
#include "../creature/skeleton.h"
#include "../geom/transforms.h"
#include "../gl/mesh.h"
#include "../gl/primitives.h"
#include "../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <span>
#include <vector>

namespace Render::Horse {

namespace {

using Render::Creature::BoneDef;
using Render::Creature::BoneIndex;
using Render::Creature::CreatureLOD;
using Render::Creature::kLodMinimal;
using Render::Creature::PartGraph;
using Render::Creature::PrimitiveInstance;
using Render::Creature::PrimitiveParams;
using Render::Creature::PrimitiveShape;
using Render::Creature::SkeletonTopology;

constexpr std::array<BoneDef, kHorseBoneCount> kHorseBones = {{
    {"Root", Render::Creature::kInvalidBone},
    {"Body", static_cast<BoneIndex>(HorseBone::Root)},
    {"ShoulderFL", static_cast<BoneIndex>(HorseBone::Root)},
    {"KneeFL", static_cast<BoneIndex>(HorseBone::ShoulderFL)},
    {"FootFL", static_cast<BoneIndex>(HorseBone::KneeFL)},
    {"ShoulderFR", static_cast<BoneIndex>(HorseBone::Root)},
    {"KneeFR", static_cast<BoneIndex>(HorseBone::ShoulderFR)},
    {"FootFR", static_cast<BoneIndex>(HorseBone::KneeFR)},
    {"ShoulderBL", static_cast<BoneIndex>(HorseBone::Root)},
    {"KneeBL", static_cast<BoneIndex>(HorseBone::ShoulderBL)},
    {"FootBL", static_cast<BoneIndex>(HorseBone::KneeBL)},
    {"ShoulderBR", static_cast<BoneIndex>(HorseBone::Root)},
    {"KneeBR", static_cast<BoneIndex>(HorseBone::ShoulderBR)},
    {"FootBR", static_cast<BoneIndex>(HorseBone::KneeBR)},
    {"NeckTop", static_cast<BoneIndex>(HorseBone::Root)},
    {"Head", static_cast<BoneIndex>(HorseBone::NeckTop)},
}};

constexpr std::array<Render::Creature::SocketDef, 0> kHorseSockets{};

constexpr SkeletonTopology kHorseTopology{
    std::span<const BoneDef>(kHorseBones),
    std::span<const Render::Creature::SocketDef>(kHorseSockets),
};

constexpr std::uint8_t kRoleCoat = 1;
constexpr std::uint8_t kRoleCoatDark = 2;
constexpr std::uint8_t kRoleCoatLeg = 3;
constexpr std::uint8_t kRoleHoof = 4;
constexpr std::uint8_t kRoleMane = 5;
constexpr std::uint8_t kRoleTail = 6;
constexpr std::uint8_t kRoleMuzzle = 7;
constexpr std::uint8_t kRoleEye = 8;
constexpr std::size_t kHorseRoleCount = 8;

constexpr float kHorseBodyVisualWidthScale = 1.38F;
constexpr float kHorseBodyVisualHeightScale = 1.06F;

[[nodiscard]] auto
translation_matrix(const QVector3D &origin) noexcept -> QMatrix4x4 {
  QMatrix4x4 m;
  m.setToIdentity();
  m.setColumn(3, QVector4D(origin, 1.0F));
  return m;
}

[[nodiscard]] auto
solve_bent_leg_joint(const QVector3D &shoulder, const QVector3D &foot,
                     float upper_len, float lower_len,
                     const QVector3D &bend_hint) noexcept -> QVector3D {
  QVector3D hip_to_foot = foot - shoulder;
  float const dist_sq = QVector3D::dotProduct(hip_to_foot, hip_to_foot);
  if (dist_sq <= 1.0e-6F) {
    return shoulder + QVector3D(0.0F, -upper_len, 0.0F);
  }

  float const total_len = std::max(upper_len + lower_len, 1.0e-4F);
  float const min_len =
      std::max(std::abs(upper_len - lower_len) + 1.0e-4F, total_len * 0.10F);
  float const dist =
      std::clamp(std::sqrt(dist_sq), min_len, total_len - 1.0e-4F);
  QVector3D const dir = hip_to_foot / std::sqrt(dist_sq);

  QVector3D bend_axis = bend_hint - dir * QVector3D::dotProduct(bend_hint, dir);
  if (bend_axis.lengthSquared() <= 1.0e-6F) {
    bend_axis = QVector3D(0.0F, 0.0F, 1.0F) -
                dir * QVector3D::dotProduct(QVector3D(0.0F, 0.0F, 1.0F), dir);
  }
  if (bend_axis.lengthSquared() <= 1.0e-6F) {
    bend_axis = QVector3D(1.0F, 0.0F, 0.0F);
  }
  bend_axis.normalize();

  float const along =
      (upper_len * upper_len - lower_len * lower_len + dist * dist) /
      (2.0F * dist);
  float const height =
      std::sqrt(std::max(upper_len * upper_len - along * along, 0.0F));
  return shoulder + dir * along + bend_axis * height;
}

struct primitive_instance_desc {
  std::string_view debug_name{};
  PrimitiveShape shape{PrimitiveShape::Sphere};
  BoneIndex anchor_bone{0};
  BoneIndex tail_bone{0};
  QVector3D head_offset{};
  QVector3D tail_offset{};
  QVector3D half_extents{};
  float radius{0.0F};
  std::uint8_t color_role{0};
  int material_id{0};
  std::uint32_t lod_mask{0};
  Render::GL::Mesh *custom_mesh{nullptr};
};

struct torso_ring {
  float z{0.0F};
  float y{0.0F};
  float half_width{0.0F};
  float top{0.0F};
  float bottom{0.0F};
};

auto make_torso_vertex(const QVector3D &pos, const QVector3D &normal, float u,
                       float v) -> Render::GL::Vertex {
  QVector3D n = normal;
  if (n.lengthSquared() < 1.0e-8F) {
    n = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    n.normalize();
  }
  return {{pos.x(), pos.y(), pos.z()}, {n.x(), n.y(), n.z()}, {u, v}};
}

void append_flat_triangle(std::vector<Render::GL::Vertex> &vertices,
                          std::vector<unsigned int> &indices,
                          const QVector3D &a, const QVector3D &b,
                          const QVector3D &c) {
  QVector3D normal = QVector3D::crossProduct(b - a, c - a);
  if (normal.lengthSquared() < 1.0e-8F) {
    normal = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    normal.normalize();
  }

  unsigned int const base = static_cast<unsigned int>(vertices.size());
  vertices.push_back(make_torso_vertex(a, normal, 0.0F, 0.0F));
  vertices.push_back(make_torso_vertex(b, normal, 1.0F, 0.0F));
  vertices.push_back(make_torso_vertex(c, normal, 0.5F, 1.0F));
  indices.insert(indices.end(), {base, base + 1U, base + 2U});
}

void append_flat_quad(std::vector<Render::GL::Vertex> &vertices,
                      std::vector<unsigned int> &indices, const QVector3D &a,
                      const QVector3D &b, const QVector3D &c,
                      const QVector3D &d) {
  QVector3D normal = QVector3D::crossProduct(b - a, c - a);
  if (normal.lengthSquared() < 1.0e-8F) {
    normal = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    normal.normalize();
  }

  unsigned int const base = static_cast<unsigned int>(vertices.size());
  vertices.push_back(make_torso_vertex(a, normal, 0.0F, 0.0F));
  vertices.push_back(make_torso_vertex(b, normal, 1.0F, 0.0F));
  vertices.push_back(make_torso_vertex(c, normal, 1.0F, 1.0F));
  vertices.push_back(make_torso_vertex(d, normal, 0.0F, 1.0F));
  indices.insert(indices.end(),
                 {base, base + 1U, base + 2U, base + 2U, base + 3U, base});
}

template <std::size_t RingCount, std::size_t RingVertexCount>
auto build_closed_ring_mesh(
    const std::array<std::array<QVector3D, RingVertexCount>, RingCount> &rings)
    -> std::unique_ptr<Render::GL::Mesh> {
  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve((RingCount - 1U) * RingVertexCount * 4U + 64U);
  indices.reserve((RingCount - 1U) * RingVertexCount * 6U + 96U);

  for (std::size_t r = 0; r + 1U < RingCount; ++r) {
    for (std::size_t p = 0; p < RingVertexCount; ++p) {
      std::size_t const next = (p + 1U) % RingVertexCount;
      append_flat_quad(vertices, indices, rings[r][p], rings[r][next],
                       rings[r + 1U][next], rings[r + 1U][p]);
    }
  }

  auto add_cap = [&](std::size_t row, bool reverse) {
    QVector3D center;
    for (std::size_t p = 0; p < RingVertexCount; ++p) {
      center += rings[row][p];
    }
    center /= static_cast<float>(RingVertexCount);
    for (std::size_t p = 0; p < RingVertexCount; ++p) {
      std::size_t const next = (p + 1U) % RingVertexCount;
      if (reverse) {
        append_flat_triangle(vertices, indices, center, rings[row][next],
                             rings[row][p]);
      } else {
        append_flat_triangle(vertices, indices, center, rings[row][p],
                             rings[row][next]);
      }
    }
  };

  add_cap(0U, true);
  add_cap(RingCount - 1U, false);
  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

template <std::size_t RingCount, std::size_t RingVertexCount>
void append_closed_ring_mesh(
    std::vector<Render::GL::Vertex> &vertices,
    std::vector<unsigned int> &indices,
    const std::array<std::array<QVector3D, RingVertexCount>, RingCount>
        &rings) {
  for (std::size_t r = 0; r + 1U < RingCount; ++r) {
    for (std::size_t p = 0; p < RingVertexCount; ++p) {
      std::size_t const next = (p + 1U) % RingVertexCount;
      append_flat_quad(vertices, indices, rings[r][p], rings[r][next],
                       rings[r + 1U][next], rings[r + 1U][p]);
    }
  }

  auto add_cap = [&](std::size_t row, bool reverse) {
    QVector3D center;
    for (std::size_t p = 0; p < RingVertexCount; ++p) {
      center += rings[row][p];
    }
    center /= static_cast<float>(RingVertexCount);
    for (std::size_t p = 0; p < RingVertexCount; ++p) {
      std::size_t const next = (p + 1U) % RingVertexCount;
      if (reverse) {
        append_flat_triangle(vertices, indices, center, rings[row][next],
                             rings[row][p]);
      } else {
        append_flat_triangle(vertices, indices, center, rings[row][p],
                             rings[row][next]);
      }
    }
  };

  add_cap(0U, true);
  add_cap(RingCount - 1U, false);
}

auto create_faceted_horse_torso_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  float const bw = dims.body_width * kHorseBodyVisualWidthScale;
  float const bh = dims.body_height * kHorseBodyVisualHeightScale;
  float const bl = dims.body_length;

  std::array<torso_ring, 7> const rings{{
      {-0.62F, 0.10F, 0.16F, 0.14F, 0.12F},
      {-0.46F, 0.16F, 0.28F, 0.26F, 0.18F},
      {-0.22F, 0.10F, 0.38F, 0.30F, 0.24F},
      {0.04F, 0.04F, 0.40F, 0.40F, 0.28F},
      {0.28F, 0.12F, 0.34F, 0.46F, 0.22F},
      {0.50F, 0.18F, 0.24F, 0.42F, 0.16F},
      {0.66F, 0.08F, 0.16F, 0.22F, 0.12F},
  }};

  std::array<std::array<QVector3D, 10>, 7> ring_points{};
  for (std::size_t r = 0; r < rings.size(); ++r) {
    torso_ring const &ring = rings[r];
    float const z = ring.z * bl;
    float const yc = ring.y * bh;
    float const hw = ring.half_width * bw;
    float const top = yc + ring.top * bh;
    float const bot = yc - ring.bottom * bh;
    float const shoulder_y = yc + ring.top * bh * 0.82F;
    float const side_high_y = yc + ring.top * bh * 0.36F;
    float const side_low_y = yc - ring.bottom * bh * 0.18F;
    float const belly_y = yc - ring.bottom * bh * 0.78F;
    float const shoulder = hw * 0.44F;
    float const side_high = hw * 0.86F;
    float const side_low = hw;
    float const belly = hw * 0.38F;
    ring_points[r] = {{
        {0.0F, top, z},
        {shoulder, shoulder_y, z},
        {side_high, side_high_y, z},
        {side_low, side_low_y, z},
        {belly, belly_y, z},
        {0.0F, bot, z},
        {-belly, belly_y, z},
        {-side_low, side_low_y, z},
        {-side_high, side_high_y, z},
        {-shoulder, shoulder_y, z},
    }};
  }

  return build_closed_ring_mesh(ring_points);
}

auto get_faceted_horse_torso_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_faceted_horse_torso_mesh();
  return mesh.get();
}

auto create_whole_horse_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  float const bw = dims.body_width * kHorseBodyVisualWidthScale;
  float const bh = dims.body_height * kHorseBodyVisualHeightScale;
  float const bl = dims.body_length;
  float const hw = dims.head_width;
  float const hh = dims.head_height;
  float const hl = dims.head_length;
  float const torso_width_scale = 2.0F;
  float const torso_height_scale = 2.0F;
  float const torso_lift = bh * 0.16F;
  float const neck_width_scale = 2.0F;
  float const neck_height_scale = 2.0F;
  float const head_length_scale = 1.0F / 3.0F;
  float const leg_length_scale = 2.0F;
  float const leg_thickness_scale = 3.0F;

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(460);
  indices.reserve(760);

  auto make_ring = [](QVector3D c, float rx,
                      float ry) -> std::array<QVector3D, 8> {
    return {{
        {c.x(), c.y() + ry, c.z()},
        {c.x() + rx * 0.62F, c.y() + ry * 0.68F, c.z()},
        {c.x() + rx, c.y(), c.z()},
        {c.x() + rx * 0.66F, c.y() - ry * 0.72F, c.z()},
        {c.x(), c.y() - ry, c.z()},
        {c.x() - rx * 0.66F, c.y() - ry * 0.72F, c.z()},
        {c.x() - rx, c.y(), c.z()},
        {c.x() - rx * 0.62F, c.y() + ry * 0.68F, c.z()},
    }};
  };

  std::array<std::array<QVector3D, 8>, 7> body{{
      make_ring({0.0F, bh * 0.10F + torso_lift, -bl * 0.56F},
                bw * 0.18F * torso_width_scale,
                bh * 0.18F * torso_height_scale),
      make_ring({0.0F, bh * 0.18F + torso_lift, -bl * 0.40F},
                bw * 0.28F * torso_width_scale,
                bh * 0.32F * torso_height_scale),
      make_ring({0.0F, bh * 0.10F + torso_lift, -bl * 0.16F},
                bw * 0.42F * torso_width_scale,
                bh * 0.34F * torso_height_scale),
      make_ring({0.0F, bh * 0.04F + torso_lift, bl * 0.08F},
                bw * 0.44F * torso_width_scale,
                bh * 0.38F * torso_height_scale),
      make_ring({0.0F, bh * 0.16F + torso_lift, bl * 0.32F},
                bw * 0.36F * torso_width_scale,
                bh * 0.42F * torso_height_scale),
      make_ring({0.0F, bh * 0.24F + torso_lift, bl * 0.52F},
                bw * 0.24F * torso_width_scale,
                bh * 0.34F * torso_height_scale),
      make_ring({0.0F, bh * 0.12F + torso_lift, bl * 0.66F},
                bw * 0.14F * torso_width_scale,
                bh * 0.18F * torso_height_scale),
  }};
  append_closed_ring_mesh(vertices, indices, body);

  std::array<std::array<QVector3D, 8>, 4> neck{{
      make_ring({0.0F, bh * 0.36F, bl * 0.48F}, bw * 0.16F * neck_width_scale,
                bh * 0.18F * neck_height_scale),
      make_ring({0.0F, bh * 0.54F, bl * 0.66F}, bw * 0.14F * neck_width_scale,
                bh * 0.18F * neck_height_scale),
      make_ring({0.0F, bh * 0.66F, bl * 0.86F}, bw * 0.12F * neck_width_scale,
                bh * 0.16F * neck_height_scale),
      make_ring({0.0F, bh * 0.60F, bl * 1.02F}, bw * 0.10F * neck_width_scale,
                bh * 0.12F * neck_height_scale),
  }};
  append_closed_ring_mesh(vertices, indices, neck);

  std::array<std::array<QVector3D, 8>, 5> head{{
      make_ring({0.0F, bh * 0.60F, bl * 0.98F}, hw * 0.22F, hh * 0.18F),
      make_ring({0.0F, bh * 0.58F,
                 bl * (0.98F + (1.12F - 0.98F) * head_length_scale)},
                hw * 0.28F, hh * 0.20F),
      make_ring({0.0F, bh * 0.48F,
                 bl * (0.98F + (1.30F - 0.98F) * head_length_scale)},
                hw * 0.20F, hh * 0.18F),
      make_ring({0.0F, bh * 0.40F,
                 bl * (0.98F + (1.48F - 0.98F) * head_length_scale)},
                hw * 0.12F, hh * 0.12F),
      make_ring({0.0F, bh * 0.36F,
                 bl * (0.98F + (1.58F - 0.98F) * head_length_scale)},
                hw * 0.07F, hh * 0.08F),
  }};
  append_closed_ring_mesh(vertices, indices, head);

  auto make_leg_ring = [](QVector3D c, float rx,
                          float rz) -> std::array<QVector3D, 6> {
    return {{
        {c.x(), c.y(), c.z() + rz},
        {c.x() + rx, c.y(), c.z() + rz * 0.50F},
        {c.x() + rx * 0.82F, c.y(), c.z() - rz * 0.50F},
        {c.x(), c.y(), c.z() - rz},
        {c.x() - rx * 0.82F, c.y(), c.z() - rz * 0.50F},
        {c.x() - rx, c.y(), c.z() + rz * 0.50F},
    }};
  };

  auto append_leg = [&](float x, float z, bool rear) {
    float const top_y = -bh * 0.10F;
    float const bottom_y = -dims.leg_length * 0.82F * leg_length_scale;
    float const upper =
        dims.body_width * (rear ? 0.17F : 0.16F) * leg_thickness_scale;
    float const lower = dims.body_width * 0.075F * leg_thickness_scale;
    std::array<std::array<QVector3D, 6>, 5> leg{{
        make_leg_ring({x, top_y, z}, upper, upper * 0.78F),
        make_leg_ring({x * 0.96F, (top_y + bottom_y) * 0.36F,
                       z + (rear ? bl * 0.04F : -bl * 0.02F)},
                      upper * 0.72F, upper * 0.58F),
        make_leg_ring({x * 0.94F, (top_y + bottom_y) * 0.62F, z}, lower,
                      lower * 0.72F),
        make_leg_ring({x * 0.92F, bottom_y + dims.hoof_height * 1.3F, z},
                      lower * 1.15F, lower * 0.82F),
        make_leg_ring({x * 0.92F, bottom_y, z + bl * 0.025F},
                      dims.body_width * 0.16F * leg_thickness_scale,
                      dims.body_width * 0.13F * leg_thickness_scale),
    }};
    append_closed_ring_mesh(vertices, indices, leg);
  };
  append_leg(bw * 0.34F, bl * 0.38F, false);
  append_leg(-bw * 0.34F, bl * 0.38F, false);
  append_leg(bw * 0.30F, -bl * 0.36F, true);
  append_leg(-bw * 0.30F, -bl * 0.36F, true);

  auto append_ear = [&](float side) {
    QVector3D const base(side * hw * 0.12F, bh * 0.64F, bl * 1.02F);
    QVector3D const tip(side * hw * 0.14F, bh * 0.84F, bl * 0.96F);
    QVector3D const rear(side * hw * 0.04F, bh * 0.62F, bl * 0.92F);
    append_flat_triangle(vertices, indices, base, tip, rear);
    append_flat_triangle(vertices, indices,
                         base + QVector3D(side * hw * 0.06F, 0, 0),
                         rear + QVector3D(side * hw * 0.06F, 0, 0), tip);
  };
  append_ear(1.0F);
  append_ear(-1.0F);

  std::array<std::array<QVector3D, 6>, 4> tail{{
      make_leg_ring({0.0F, bh * 0.20F, -bl * 0.60F}, bw * 0.08F, bw * 0.06F),
      make_leg_ring({0.0F, bh * 0.02F, -bl * 0.72F}, bw * 0.07F, bw * 0.06F),
      make_leg_ring({0.0F, -bh * 0.18F, -bl * 0.78F}, bw * 0.05F, bw * 0.05F),
      make_leg_ring({0.0F, -bh * 0.34F, -bl * 0.80F}, bw * 0.03F, bw * 0.035F),
  }};
  append_closed_ring_mesh(vertices, indices, tail);

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto get_whole_horse_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_whole_horse_mesh();
  return mesh.get();
}

auto create_faceted_horse_head_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  float const hw = dims.head_width;
  float const hh = dims.head_height;
  float const hl = dims.head_length;

  std::array<torso_ring, 6> const sections{{
      {-0.46F, 0.10F, 0.18F, 0.14F, 0.10F},
      {-0.28F, 0.14F, 0.26F, 0.18F, 0.12F},
      {-0.06F, 0.08F, 0.24F, 0.14F, 0.16F},
      {0.18F, 0.00F, 0.16F, 0.08F, 0.16F},
      {0.38F, -0.06F, 0.10F, 0.04F, 0.12F},
      {0.52F, -0.10F, 0.06F, 0.02F, 0.08F},
  }};

  std::array<std::array<QVector3D, 8>, 6> section_points{};
  for (std::size_t s = 0; s < sections.size(); ++s) {
    torso_ring const &section = sections[s];
    float const z = section.z * hl;
    float const yc = section.y * hh;
    float const half_width = section.half_width * hw;
    float const top = yc + section.top * hh;
    float const bot = yc - section.bottom * hh;
    float const crown_y = yc + section.top * hh * 0.52F;
    float const jaw_y = yc - section.bottom * hh * 0.36F;
    float const crown_w = half_width * 0.54F;
    float const side_w = half_width;
    float const jaw_w = half_width * 0.66F;

    section_points[s] = {{
        {0.0F, top, z},
        {crown_w, crown_y, z},
        {side_w, yc, z},
        {jaw_w, jaw_y, z},
        {0.0F, bot, z},
        {-jaw_w, jaw_y, z},
        {-side_w, yc, z},
        {-crown_w, crown_y, z},
    }};
  }

  return build_closed_ring_mesh(section_points);
}

auto get_faceted_horse_head_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_faceted_horse_head_mesh();
  return mesh.get();
}

auto create_unit_horse_minimal_leg_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  std::array<std::array<QVector3D, 6>, 5> const rings{{
      {{{0.0F, 0.50F, 0.14F},
        {0.18F, 0.50F, 0.10F},
        {0.18F, 0.50F, -0.08F},
        {0.0F, 0.50F, -0.10F},
        {-0.18F, 0.50F, -0.08F},
        {-0.18F, 0.50F, 0.10F}}},
      {{{0.0F, 0.18F, 0.12F},
        {0.16F, 0.18F, 0.08F},
        {0.16F, 0.18F, -0.07F},
        {0.0F, 0.18F, -0.08F},
        {-0.16F, 0.18F, -0.07F},
        {-0.16F, 0.18F, 0.08F}}},
      {{{0.0F, -0.12F, 0.08F},
        {0.10F, -0.12F, 0.06F},
        {0.10F, -0.12F, -0.05F},
        {0.0F, -0.12F, -0.06F},
        {-0.10F, -0.12F, -0.05F},
        {-0.10F, -0.12F, 0.06F}}},
      {{{0.0F, -0.42F, 0.12F},
        {0.14F, -0.42F, 0.10F},
        {0.13F, -0.42F, -0.05F},
        {0.0F, -0.42F, -0.06F},
        {-0.13F, -0.42F, -0.05F},
        {-0.14F, -0.42F, 0.10F}}},
      {{{0.0F, -0.58F, 0.18F},
        {0.18F, -0.58F, 0.14F},
        {0.16F, -0.58F, -0.06F},
        {0.0F, -0.58F, -0.08F},
        {-0.16F, -0.58F, -0.06F},
        {-0.18F, -0.58F, 0.14F}}},
  }};
  return build_closed_ring_mesh(rings);
}

auto get_unit_horse_minimal_leg_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_unit_horse_minimal_leg_mesh();
  return mesh.get();
}

auto create_unit_horse_tail_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  std::array<std::array<QVector3D, 6>, 4> const rings{{
      {{{0.0F, 0.50F, 0.10F},
        {0.12F, 0.50F, 0.06F},
        {0.10F, 0.50F, -0.06F},
        {0.0F, 0.50F, -0.08F},
        {-0.10F, 0.50F, -0.06F},
        {-0.12F, 0.50F, 0.06F}}},
      {{{0.0F, 0.18F, 0.14F},
        {0.10F, 0.18F, 0.10F},
        {0.08F, 0.18F, 0.00F},
        {0.0F, 0.18F, -0.02F},
        {-0.08F, 0.18F, 0.00F},
        {-0.10F, 0.18F, 0.10F}}},
      {{{0.0F, -0.14F, 0.18F},
        {0.07F, -0.14F, 0.14F},
        {0.05F, -0.14F, 0.06F},
        {0.0F, -0.14F, 0.02F},
        {-0.05F, -0.14F, 0.06F},
        {-0.07F, -0.14F, 0.14F}}},
      {{{0.0F, -0.48F, 0.22F},
        {0.03F, -0.48F, 0.18F},
        {0.02F, -0.48F, 0.12F},
        {0.0F, -0.48F, 0.08F},
        {-0.02F, -0.48F, 0.12F},
        {-0.03F, -0.48F, 0.18F}}},
  }};
  return build_closed_ring_mesh(rings);
}

auto get_unit_horse_tail_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_unit_horse_tail_mesh();
  return mesh.get();
}

auto create_unit_horse_hoof_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  std::vector<Render::GL::Vertex> vertices;
  vertices.reserve(8);

  auto add = [&](QVector3D pos, QVector3D normal, float u, float v) {
    vertices.push_back(make_torso_vertex(pos, normal, u, v));
  };

  add({-0.92F, 0.52F, -0.95F}, {-0.8F, 0.5F, -0.4F}, 0.0F, 1.0F);
  add({0.92F, 0.52F, -0.95F}, {0.8F, 0.5F, -0.4F}, 1.0F, 1.0F);
  add({-0.72F, 0.24F, 0.86F}, {-0.7F, 0.3F, 0.7F}, 0.0F, 0.8F);
  add({0.72F, 0.24F, 0.86F}, {0.7F, 0.3F, 0.7F}, 1.0F, 0.8F);
  add({-1.0F, -0.58F, -0.80F}, {-0.8F, -0.7F, -0.2F}, 0.0F, 0.0F);
  add({1.0F, -0.58F, -0.80F}, {0.8F, -0.7F, -0.2F}, 1.0F, 0.0F);
  add({-0.82F, -0.58F, 0.94F}, {-0.7F, -0.5F, 0.7F}, 0.0F, 0.2F);
  add({0.82F, -0.58F, 0.94F}, {0.7F, -0.5F, 0.7F}, 1.0F, 0.2F);

  std::vector<unsigned int> indices{
      0, 1, 3, 3, 2, 0, 4, 6, 7, 7, 5, 4, 0, 2, 6, 6, 4, 0,
      1, 5, 7, 7, 3, 1, 0, 4, 5, 5, 1, 0, 2, 3, 7, 7, 6, 2,
  };

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto get_unit_horse_hoof_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_unit_horse_hoof_mesh();
  return mesh.get();
}

auto create_unit_horse_segment_mesh(int sides)
    -> std::unique_ptr<Render::GL::Mesh> {
  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(static_cast<std::size_t>(sides) * 18U);
  indices.reserve(static_cast<std::size_t>(sides) * 18U);

  std::vector<QVector3D> top;
  std::vector<QVector3D> bottom;
  top.reserve(sides);
  bottom.reserve(sides);
  for (int i = 0; i < sides; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(sides);
    float const ang = t * (2.0F * std::numbers::pi_v<float>);
    top.push_back(QVector3D(std::cos(ang), 0.5F, std::sin(ang)));
    bottom.push_back(QVector3D(std::cos(ang), -0.5F, std::sin(ang)));
  }

  for (int i = 0; i < sides; ++i) {
    int const next = (i + 1) % sides;
    append_flat_quad(vertices, indices, bottom[i], bottom[next], top[next],
                     top[i]);
  }

  QVector3D const top_center(0.0F, 0.5F, 0.0F);
  QVector3D const bottom_center(0.0F, -0.5F, 0.0F);
  for (int i = 0; i < sides; ++i) {
    int const next = (i + 1) % sides;
    append_flat_triangle(vertices, indices, top_center, top[i], top[next]);
    append_flat_triangle(vertices, indices, bottom_center, bottom[next],
                         bottom[i]);
  }

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto get_unit_horse_segment_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_unit_horse_segment_mesh(6);
  return mesh.get();
}

auto create_unit_horse_cone_mesh(int sides)
    -> std::unique_ptr<Render::GL::Mesh> {
  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(static_cast<std::size_t>(sides) * 9U);
  indices.reserve(static_cast<std::size_t>(sides) * 9U);

  QVector3D const apex(0.0F, 0.5F, 0.0F);
  std::vector<QVector3D> base;
  base.reserve(sides);
  for (int i = 0; i < sides; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(sides);
    float const ang = t * (2.0F * std::numbers::pi_v<float>);
    base.push_back(QVector3D(std::cos(ang), -0.5F, std::sin(ang)));
  }

  for (int i = 0; i < sides; ++i) {
    int const next = (i + 1) % sides;
    append_flat_triangle(vertices, indices, apex, base[i], base[next]);
  }

  QVector3D const base_center(0.0F, -0.5F, 0.0F);
  for (int i = 0; i < sides; ++i) {
    int const next = (i + 1) % sides;
    append_flat_triangle(vertices, indices, base_center, base[next], base[i]);
  }

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto get_unit_horse_cone_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_unit_horse_cone_mesh(5);
  return mesh.get();
}

[[nodiscard]] auto make_primitive_instance(
    const primitive_instance_desc &desc) noexcept -> PrimitiveInstance {
  PrimitiveInstance primitive{};
  primitive.debug_name = desc.debug_name;
  primitive.shape = desc.shape;
  primitive.params.anchor_bone = desc.anchor_bone;
  primitive.params.tail_bone = desc.tail_bone;
  primitive.params.head_offset = desc.head_offset;
  primitive.params.tail_offset = desc.tail_offset;
  primitive.params.half_extents = desc.half_extents;
  primitive.params.radius = desc.radius;
  primitive.custom_mesh = desc.custom_mesh;
  primitive.color_role = desc.color_role;
  primitive.material_id = desc.material_id;
  primitive.lod_mask = desc.lod_mask;
  return primitive;
}

struct horse_pose_profile {
  QVector3D body_half_scale{0.68F, 0.34F, 0.42F};
  QVector3D neck_base_scale{0.0F, 0.38F, 0.40F};
  float neck_rise_scale{1.00F};
  float neck_length_scale{0.94F};
  float neck_radius_scale{0.20F};
  QVector3D head_center_scale{0.0F, -0.04F, 0.68F};
  QVector3D head_half_scale{0.19F, 0.13F, 0.34F};
  QVector3D front_anchor_scale{0.0F, -0.13F, 0.42F};
  QVector3D rear_anchor_scale{0.0F, -0.20F, -0.36F};
  float front_bias_scale{0.08F};
  float rear_bias_scale{-0.12F};
  float front_shoulder_out_scale{0.96F};
  float rear_shoulder_out_scale{0.74F};
  float front_vertical_bias_scale{-0.30F};
  float rear_vertical_bias_scale{-0.44F};
  float front_longitudinal_bias_scale{0.08F};
  float rear_longitudinal_bias_scale{-0.11F};
  float front_leg_length_scale{1.14F};
  float rear_leg_length_scale{1.12F};
  float leg_radius_scale{0.22F};
  float front_upper_radius_scale{1.70F};
  float rear_upper_radius_scale{1.85F};
  float front_lower_radius_scale{0.78F};
  float rear_lower_radius_scale{0.76F};
  float front_upper_joint_height_scale{0.66F};
  float rear_upper_joint_height_scale{0.60F};
  float front_joint_z_scale{0.12F};
  float rear_joint_z_scale{-0.48F};

  QVector3D body_offset_scale{0.0F, 0.03F, -0.02F};
  QVector3D body_half_extents_scale{0.50F, 0.22F, 0.44F};
  QVector3D chest_offset_scale{0.0F, 0.03F, 0.50F};
  QVector3D chest_half_extents_scale{0.36F, 0.32F, 0.14F};
  QVector3D belly_offset_scale{0.0F, -0.18F, -0.04F};
  QVector3D belly_half_extents_scale{0.36F, 0.08F, 0.34F};
  QVector3D rump_offset_scale{0.0F, 0.07F, -0.42F};
  QVector3D rump_half_extents_scale{0.46F, 0.28F, 0.24F};

  QVector3D withers_offset_scale{0.0F, 0.58F, 0.18F};
  QVector3D withers_tail_offset_scale{0.0F, 0.34F, 0.18F};
  float withers_radius_scale{0.12F};
  QVector3D cranium_offset_scale{0.0F, 0.05F, -0.10F};
  QVector3D cranium_half_extents_scale{0.24F, 0.18F, 0.33F};
  QVector3D muzzle_head_scale{0.0F, -0.03F, 0.04F};
  QVector3D muzzle_tail_scale{0.0F, -0.20F, 0.88F};
  float muzzle_radius_scale{0.09F};
  QVector3D jaw_offset_scale{0.0F, -0.16F, 0.15F};
  QVector3D jaw_half_extents_scale{0.08F, 0.04F, 0.17F};
  QVector3D ear_head_scale{0.10F, 0.12F, -0.28F};
  QVector3D ear_tail_scale{0.11F, 0.34F, -0.36F};
  float ear_radius_scale{0.05F};
  QVector3D tail_head_scale{0.0F, 0.16F, -0.42F};
  QVector3D tail_tail_scale{0.0F, -0.36F, -0.56F};
  float tail_radius_scale{0.20F};
  QVector3D hoof_scale{0.30F, 0.88F, 0.50F};
};

[[nodiscard]] auto make_horse_pose_profile() noexcept -> horse_pose_profile {
  return {};
}

} // namespace

auto horse_topology() noexcept -> const SkeletonTopology & {
  return kHorseTopology;
}

void evaluate_horse_skeleton(const HorseSpecPose &pose,
                             BonePalette &out_palette) noexcept {
  out_palette[static_cast<std::size_t>(HorseBone::Root)] =
      translation_matrix(pose.barrel_center);

  QMatrix4x4 body;
  body.setColumn(0, QVector4D(pose.body_ellipsoid_x, 0.0F, 0.0F, 0.0F));
  body.setColumn(1, QVector4D(0.0F, pose.body_ellipsoid_y, 0.0F, 0.0F));
  body.setColumn(2, QVector4D(0.0F, 0.0F, pose.body_ellipsoid_z, 0.0F));
  body.setColumn(3, QVector4D(pose.barrel_center, 1.0F));
  out_palette[static_cast<std::size_t>(HorseBone::Body)] = body;

  out_palette[static_cast<std::size_t>(HorseBone::ShoulderFL)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_fl);
  out_palette[static_cast<std::size_t>(HorseBone::ShoulderFR)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_fr);
  out_palette[static_cast<std::size_t>(HorseBone::ShoulderBL)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_bl);
  out_palette[static_cast<std::size_t>(HorseBone::ShoulderBR)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_br);

  out_palette[static_cast<std::size_t>(HorseBone::KneeFL)] =
      translation_matrix(pose.knee_fl);
  out_palette[static_cast<std::size_t>(HorseBone::KneeFR)] =
      translation_matrix(pose.knee_fr);
  out_palette[static_cast<std::size_t>(HorseBone::KneeBL)] =
      translation_matrix(pose.knee_bl);
  out_palette[static_cast<std::size_t>(HorseBone::KneeBR)] =
      translation_matrix(pose.knee_br);

  out_palette[static_cast<std::size_t>(HorseBone::FootFL)] =
      translation_matrix(pose.foot_fl);
  out_palette[static_cast<std::size_t>(HorseBone::FootFR)] =
      translation_matrix(pose.foot_fr);
  out_palette[static_cast<std::size_t>(HorseBone::FootBL)] =
      translation_matrix(pose.foot_bl);
  out_palette[static_cast<std::size_t>(HorseBone::FootBR)] =
      translation_matrix(pose.foot_br);

  out_palette[static_cast<std::size_t>(HorseBone::NeckTop)] =
      translation_matrix(pose.neck_top);
  out_palette[static_cast<std::size_t>(HorseBone::Head)] =
      translation_matrix(pose.head_center);
}

void fill_horse_role_colors(const Render::GL::HorseVariant &variant,
                            std::array<QVector3D, 8> &out_roles) noexcept {
  QVector3D const coat = variant.coat_color;
  out_roles[0] = coat;
  out_roles[1] = coat * 0.62F + QVector3D(0.080F, 0.068F, 0.050F);
  out_roles[2] = coat * 0.70F + QVector3D(0.060F, 0.050F, 0.040F);
  out_roles[3] = variant.hoof_color;
  out_roles[4] = variant.mane_color;
  out_roles[5] = variant.tail_color;
  out_roles[6] = variant.muzzle_color;
  out_roles[7] = QVector3D(0.04F, 0.03F, 0.03F);
}

void make_horse_spec_pose(const Render::GL::HorseDimensions &dims, float bob,
                          HorseSpecPose &out_pose) noexcept {
  QVector3D const center(0.0F, dims.barrel_center_y + bob, 0.0F);
  out_pose.barrel_center = center;

  out_pose.body_ellipsoid_x = dims.body_width * 1.52F;
  out_pose.body_ellipsoid_y = dims.body_height * 0.56F + dims.neck_rise * 0.05F;
  out_pose.body_ellipsoid_z =
      dims.body_length * 1.02F + dims.head_length * 0.12F;

  float const front_shoulder_dx = dims.body_width * 0.92F;
  float const rear_hip_dx = dims.body_width * 0.62F;
  float const front_shoulder_dy = -dims.body_height * 0.10F;
  float const rear_hip_dy = -dims.body_height * 0.20F;
  float const front_dz = dims.body_length * 0.48F;
  float const rear_dz = -dims.body_length * 0.42F;

  out_pose.shoulder_offset_fl =
      QVector3D(front_shoulder_dx, front_shoulder_dy, front_dz);
  out_pose.shoulder_offset_fr =
      QVector3D(-front_shoulder_dx, front_shoulder_dy, front_dz);
  out_pose.shoulder_offset_bl = QVector3D(rear_hip_dx, rear_hip_dy, rear_dz);
  out_pose.shoulder_offset_br = QVector3D(-rear_hip_dx, rear_hip_dy, rear_dz);

  float const front_drop = -dims.leg_length * 0.80F;
  float const rear_drop = -dims.leg_length * 0.79F;
  out_pose.foot_fl = center + out_pose.shoulder_offset_fl +
                     QVector3D(0.0F, front_drop, dims.body_length * 0.03F);
  out_pose.foot_fr = center + out_pose.shoulder_offset_fr +
                     QVector3D(0.0F, front_drop, dims.body_length * 0.03F);
  out_pose.foot_bl = center + out_pose.shoulder_offset_bl +
                     QVector3D(0.0F, rear_drop, -dims.body_length * 0.10F);
  out_pose.foot_br = center + out_pose.shoulder_offset_br +
                     QVector3D(0.0F, rear_drop, -dims.body_length * 0.10F);

  out_pose.shoulder_offset_pose_fl = out_pose.shoulder_offset_fl;
  out_pose.shoulder_offset_pose_fr = out_pose.shoulder_offset_fr;
  out_pose.shoulder_offset_pose_bl = out_pose.shoulder_offset_bl;
  out_pose.shoulder_offset_pose_br = out_pose.shoulder_offset_br;

  QVector3D const shoulder_fl = center + out_pose.shoulder_offset_pose_fl;
  QVector3D const shoulder_fr = center + out_pose.shoulder_offset_pose_fr;
  QVector3D const shoulder_bl = center + out_pose.shoulder_offset_pose_bl;
  QVector3D const shoulder_br = center + out_pose.shoulder_offset_pose_br;
  float const front_upper_len = dims.leg_length * 0.50F;
  float const front_lower_len = dims.leg_length * 0.46F;
  float const rear_upper_len = dims.leg_length * 0.56F;
  float const rear_lower_len = dims.leg_length * 0.50F;
  QVector3D const front_bend_hint(0.0F, 0.0F, dims.body_length * 0.28F);
  QVector3D const rear_bend_hint(0.0F, 0.0F, -dims.body_length * 0.40F);
  out_pose.knee_fl =
      solve_bent_leg_joint(shoulder_fl, out_pose.foot_fl, front_upper_len,
                           front_lower_len, front_bend_hint);
  out_pose.knee_fr =
      solve_bent_leg_joint(shoulder_fr, out_pose.foot_fr, front_upper_len,
                           front_lower_len, front_bend_hint);
  out_pose.knee_bl =
      solve_bent_leg_joint(shoulder_bl, out_pose.foot_bl, rear_upper_len,
                           rear_lower_len, rear_bend_hint);
  out_pose.knee_br =
      solve_bent_leg_joint(shoulder_br, out_pose.foot_br, rear_upper_len,
                           rear_lower_len, rear_bend_hint);

  out_pose.leg_radius = dims.body_width * 0.108F;
}

namespace {

struct pose_leg_sample {
  QVector3D shoulder;
  QVector3D foot;
};

auto compute_pose_leg(
    const Render::GL::HorseDimensions &dims, const Render::GL::HorseGait &gait,
    const horse_pose_profile &profile, float phase_base, float phase_offset,
    float lateral_sign, bool is_rear, float forward_bias, bool is_moving,
    const QVector3D &anchor_offset) noexcept -> pose_leg_sample {
  auto ease_in_out = [](float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
  };
  float const leg_phase = std::fmod(phase_base + phase_offset, 1.0F);
  float stride = 0.0F;
  float lift = 0.0F;
  float shoulder_compress = 0.0F;
  if (is_moving) {
    float const stance_fraction =
        gait.cycle_time > 0.9F ? 0.60F
                               : (gait.cycle_time > 0.5F ? 0.44F : 0.34F);
    float const stride_extent = gait.stride_swing * 0.56F;
    if (leg_phase < stance_fraction) {
      float const t = ease_in_out(leg_phase / stance_fraction);
      stride = forward_bias + stride_extent * (0.40F - 1.00F * t);
      shoulder_compress =
          std::sin(t * std::numbers::pi_v<float>) * gait.stride_lift * 0.08F;
    } else {
      float const t =
          ease_in_out((leg_phase - stance_fraction) / (1.0F - stance_fraction));
      stride = forward_bias - stride_extent * 0.60F + stride_extent * 1.00F * t;
      lift = std::sin(t * std::numbers::pi_v<float>) * gait.stride_lift * 0.84F;
    }
  }

  float const shoulder_out =
      dims.body_width * (is_rear ? profile.rear_shoulder_out_scale
                                 : profile.front_shoulder_out_scale);

  float const vertical_bias =
      dims.body_height * (is_rear ? profile.rear_vertical_bias_scale
                                  : profile.front_vertical_bias_scale);
  float const longitudinal_bias =
      dims.body_length * (is_rear ? profile.rear_longitudinal_bias_scale
                                  : profile.front_longitudinal_bias_scale);
  QVector3D const shoulder =
      anchor_offset +
      QVector3D(lateral_sign * shoulder_out,
                vertical_bias + lift * 0.05F - shoulder_compress,
                stride + longitudinal_bias);

  float const leg_length =
      dims.leg_length * (is_rear ? profile.rear_leg_length_scale
                                 : profile.front_leg_length_scale);
  QVector3D const foot =
      shoulder + QVector3D(0.0F, -leg_length + lift,
                           dims.body_length * (is_rear ? -0.02F : 0.01F));

  return {shoulder, foot};
}

} // namespace

void make_horse_spec_pose_animated(const Render::GL::HorseDimensions &dims,
                                   const Render::GL::HorseGait &gait,
                                   HorsePoseMotion motion,
                                   HorseSpecPose &out_pose) noexcept {

  make_horse_spec_pose(dims, motion.bob, out_pose);
  horse_pose_profile const profile = make_horse_pose_profile();

  QVector3D const center = out_pose.barrel_center;

  out_pose.body_half =
      QVector3D(dims.body_width * profile.body_half_scale.x(),
                dims.body_height * profile.body_half_scale.y(),
                dims.body_length * profile.body_half_scale.z());

  float const head_height_scale = 1.0F + gait.head_height_jitter;
  out_pose.neck_base =
      center + QVector3D(dims.body_width * profile.neck_base_scale.x(),
                         dims.body_height * profile.neck_base_scale.y() *
                             head_height_scale,
                         dims.body_length * profile.neck_base_scale.z());
  out_pose.neck_top =
      out_pose.neck_base +
      QVector3D(0.0F,
                dims.neck_rise * profile.neck_rise_scale * head_height_scale,
                dims.neck_length * profile.neck_length_scale);
  out_pose.neck_radius = dims.body_width * profile.neck_radius_scale;

  out_pose.head_center =
      out_pose.neck_top +
      QVector3D(dims.head_width * profile.head_center_scale.x(),
                dims.head_height * profile.head_center_scale.y(),
                dims.head_length * profile.head_center_scale.z());
  out_pose.head_half =
      QVector3D(dims.head_width * profile.head_half_scale.x(),
                dims.head_height * profile.head_half_scale.y(),
                dims.head_length * profile.head_half_scale.z());

  QVector3D const front_anchor =
      QVector3D(dims.body_width * profile.front_anchor_scale.x(),
                dims.body_height * profile.front_anchor_scale.y(),
                dims.body_length * profile.front_anchor_scale.z());
  QVector3D const rear_anchor =
      QVector3D(dims.body_width * profile.rear_anchor_scale.x(),
                dims.body_height * profile.rear_anchor_scale.y(),
                dims.body_length * profile.rear_anchor_scale.z());

  float const front_bias = dims.body_length * profile.front_bias_scale;
  float const rear_bias = dims.body_length * profile.rear_bias_scale;

  Render::GL::HorseGait jittered = gait;
  jittered.stride_swing *= (1.0F + gait.stride_jitter);
  float const front_lat =
      gait.lateral_lead_front == 0.0F ? 0.44F : gait.lateral_lead_front;
  float const rear_lat =
      gait.lateral_lead_rear == 0.0F ? 0.50F : gait.lateral_lead_rear;

  auto fl = compute_pose_leg(
      dims, jittered, profile, motion.phase, gait.front_leg_phase, 1.0F, false,
      front_bias, motion.is_moving,
      front_anchor + QVector3D(dims.body_width * 0.10F, 0.0F, 0.0F));
  auto fr = compute_pose_leg(
      dims, jittered, profile, motion.phase, gait.front_leg_phase + front_lat,
      -1.0F, false, front_bias, motion.is_moving,
      front_anchor + QVector3D(-dims.body_width * 0.10F, 0.0F, 0.0F));
  auto bl = compute_pose_leg(
      dims, jittered, profile, motion.phase, gait.rear_leg_phase, 1.0F, true,
      rear_bias, motion.is_moving,
      rear_anchor + QVector3D(-dims.body_width * 0.10F, 0.0F, 0.0F));
  auto br = compute_pose_leg(
      dims, jittered, profile, motion.phase, gait.rear_leg_phase + rear_lat,
      -1.0F, true, rear_bias, motion.is_moving,
      rear_anchor + QVector3D(dims.body_width * 0.10F, 0.0F, 0.0F));

  out_pose.shoulder_offset_pose_fl = fl.shoulder;
  out_pose.shoulder_offset_pose_fr = fr.shoulder;
  out_pose.shoulder_offset_pose_bl = bl.shoulder;
  out_pose.shoulder_offset_pose_br = br.shoulder;

  out_pose.foot_fl = center + fl.foot;
  out_pose.foot_fr = center + fr.foot;
  out_pose.foot_bl = center + bl.foot;
  out_pose.foot_br = center + br.foot;

  QVector3D const shoulder_fl = center + out_pose.shoulder_offset_pose_fl;
  QVector3D const shoulder_fr = center + out_pose.shoulder_offset_pose_fr;
  QVector3D const shoulder_bl = center + out_pose.shoulder_offset_pose_bl;
  QVector3D const shoulder_br = center + out_pose.shoulder_offset_pose_br;
  float const front_upper_len = dims.leg_length * 0.50F;
  float const front_lower_len = dims.leg_length * 0.46F;
  float const rear_upper_len = dims.leg_length * 0.56F;
  float const rear_lower_len = dims.leg_length * 0.50F;
  float const gait_bend =
      motion.is_moving ? 1.0F + std::min(gait.stride_lift * 4.0F, 0.65F) : 1.0F;
  QVector3D const front_bend_hint(0.0F, 0.0F,
                                  dims.body_length * 0.26F * gait_bend);
  QVector3D const rear_bend_hint(0.0F, 0.0F,
                                 -dims.body_length * 0.40F * gait_bend);
  out_pose.knee_fl =
      solve_bent_leg_joint(shoulder_fl, out_pose.foot_fl, front_upper_len,
                           front_lower_len, front_bend_hint);
  out_pose.knee_fr =
      solve_bent_leg_joint(shoulder_fr, out_pose.foot_fr, front_upper_len,
                           front_lower_len, front_bend_hint);
  out_pose.knee_bl =
      solve_bent_leg_joint(shoulder_bl, out_pose.foot_bl, rear_upper_len,
                           rear_lower_len, rear_bend_hint);
  out_pose.knee_br =
      solve_bent_leg_joint(shoulder_br, out_pose.foot_br, rear_upper_len,
                           rear_lower_len, rear_bend_hint);

  out_pose.pose_leg_radius = dims.body_width * profile.leg_radius_scale;

  out_pose.hoof_scale = QVector3D(dims.body_width * profile.hoof_scale.x(),
                                  dims.hoof_height * profile.hoof_scale.y(),
                                  dims.body_width * profile.hoof_scale.z());
}

namespace {

auto build_minimal_primitives(const HorseSpecPose &pose,
                              std::array<PrimitiveInstance, 1> &out) noexcept
    -> std::size_t {
  (void)pose;
  {
    PrimitiveInstance &p = out[0];
    p.debug_name = "horse.minimal.whole";
    p.shape = PrimitiveShape::Mesh;
    p.custom_mesh = get_whole_horse_mesh();
    p.mesh_skinning = Render::Creature::MeshSkinning::HorseWhole;
    p.params.anchor_bone = static_cast<BoneIndex>(HorseBone::Root);
    p.params.half_extents = QVector3D(1.0F, 1.0F, 1.0F);
    p.color_role = kRoleCoat;
    p.material_id = 6;
    p.lod_mask = kLodMinimal;
  }
  return 1;
}

} // namespace

} // namespace Render::Horse

#include "../bone_palette_arena.h"
#include "../rigged_mesh_cache.h"
#include "../scene_renderer.h"

namespace Render::Horse {

namespace {

auto build_baseline_pose() noexcept -> HorseSpecPose {
  Render::GL::HorseDimensions const dims =
      Render::GL::make_horse_dimensions(0U);
  Render::GL::HorseGait gait{};
  HorseSpecPose pose{};
  make_horse_spec_pose_animated(dims, gait, HorsePoseMotion{}, pose);
  return pose;
}

auto baseline_pose() noexcept -> const HorseSpecPose & {
  static const HorseSpecPose pose = build_baseline_pose();
  return pose;
}

constexpr std::size_t kHorseMinimalPartCount = 1;

auto build_static_minimal_parts() noexcept
    -> std::array<PrimitiveInstance, kHorseMinimalPartCount> {
  std::array<PrimitiveInstance, kHorseMinimalPartCount> out{};
  build_minimal_primitives(baseline_pose(), out);
  return out;
}

auto static_minimal_parts() noexcept
    -> const std::array<PrimitiveInstance, kHorseMinimalPartCount> & {
  static const auto parts = build_static_minimal_parts();
  return parts;
}

constexpr std::size_t kHorseFullPartCount = 1;

auto build_static_full_parts() noexcept
    -> std::array<PrimitiveInstance, kHorseFullPartCount> {
  (void)baseline_pose();
  std::array<PrimitiveInstance, kHorseFullPartCount> out{};

  using Render::Creature::kLodFull;

  auto root = static_cast<BoneIndex>(HorseBone::Root);

  PrimitiveInstance &whole = out[0];
  whole.debug_name = "horse.full.whole";
  whole.shape = PrimitiveShape::Mesh;
  whole.params.anchor_bone = root;
  whole.params.head_offset = QVector3D(0.0F, 0.0F, 0.0F);
  whole.params.half_extents = QVector3D(1.0F, 1.0F, 1.0F);
  whole.custom_mesh = get_whole_horse_mesh();
  whole.mesh_skinning = Render::Creature::MeshSkinning::HorseWhole;
  whole.color_role = kRoleCoat;
  whole.material_id = 6;
  whole.lod_mask = kLodFull;
  return out;
}

auto static_full_parts() noexcept
    -> const std::array<PrimitiveInstance, kHorseFullPartCount> & {
  static const auto parts = build_static_full_parts();
  return parts;
}

auto build_horse_bind_palette() noexcept
    -> std::array<QMatrix4x4, kHorseBoneCount> {
  std::array<QMatrix4x4, kHorseBoneCount> out{};
  BonePalette tmp{};
  evaluate_horse_skeleton(baseline_pose(), tmp);
  for (std::size_t i = 0; i < kHorseBoneCount; ++i) {
    out[i] = tmp[i];
  }
  return out;
}

} // namespace

auto horse_creature_spec() noexcept -> const Render::Creature::CreatureSpec & {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "horse";
    s.topology = horse_topology();
    s.lod_minimal = PartGraph{
        std::span<const PrimitiveInstance>(static_minimal_parts().data(),
                                           static_minimal_parts().size()),
    };
    s.lod_full = PartGraph{
        std::span<const PrimitiveInstance>(static_full_parts().data(),
                                           static_full_parts().size()),
    };
    return s;
  }();
  return spec;
}

auto compute_horse_bone_palette(const HorseSpecPose &pose,
                                std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t {
  if (out_bones.size() < kHorseBoneCount) {
    return 0U;
  }
  BonePalette tmp{};
  evaluate_horse_skeleton(pose, tmp);
  for (std::size_t i = 0; i < kHorseBoneCount; ++i) {
    out_bones[i] = tmp[i];
  }
  return static_cast<std::uint32_t>(kHorseBoneCount);
}

auto horse_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const std::array<QMatrix4x4, kHorseBoneCount> palette =
      build_horse_bind_palette();
  return std::span<const QMatrix4x4>(palette.data(), palette.size());
}

} // namespace Render::Horse
