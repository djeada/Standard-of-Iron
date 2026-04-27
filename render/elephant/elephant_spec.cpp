#include "elephant_spec.h"

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
#include <span>
#include <vector>

namespace Render::Elephant {

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

constexpr std::array<BoneDef, kElephantBoneCount> kElephantBones = {{
    {"Root", Render::Creature::kInvalidBone},
    {"Body", static_cast<BoneIndex>(ElephantBone::Root)},
    {"ShoulderFL", static_cast<BoneIndex>(ElephantBone::Root)},
    {"KneeFL", static_cast<BoneIndex>(ElephantBone::ShoulderFL)},
    {"FootFL", static_cast<BoneIndex>(ElephantBone::KneeFL)},
    {"ShoulderFR", static_cast<BoneIndex>(ElephantBone::Root)},
    {"KneeFR", static_cast<BoneIndex>(ElephantBone::ShoulderFR)},
    {"FootFR", static_cast<BoneIndex>(ElephantBone::KneeFR)},
    {"ShoulderBL", static_cast<BoneIndex>(ElephantBone::Root)},
    {"KneeBL", static_cast<BoneIndex>(ElephantBone::ShoulderBL)},
    {"FootBL", static_cast<BoneIndex>(ElephantBone::KneeBL)},
    {"ShoulderBR", static_cast<BoneIndex>(ElephantBone::Root)},
    {"KneeBR", static_cast<BoneIndex>(ElephantBone::ShoulderBR)},
    {"FootBR", static_cast<BoneIndex>(ElephantBone::KneeBR)},
    {"Head", static_cast<BoneIndex>(ElephantBone::Root)},
    {"TrunkTip", static_cast<BoneIndex>(ElephantBone::Head)},
}};

constexpr std::array<Render::Creature::SocketDef, 0> kElephantSockets{};

constexpr SkeletonTopology kElephantTopology{
    std::span<const BoneDef>(kElephantBones),
    std::span<const Render::Creature::SocketDef>(kElephantSockets),
};

constexpr std::uint8_t kRoleSkin = 1;
constexpr std::uint8_t kRoleSkinShadow = 2;
constexpr std::uint8_t kRoleSkinLeg = 3;
constexpr std::uint8_t kRoleSkinFootPad = 4;
constexpr std::uint8_t kRoleSkinTrunk = 5;
constexpr std::uint8_t kRoleEarInner = 6;
constexpr std::uint8_t kRoleTusk = 7;
constexpr std::uint8_t kRoleEye = 8;
constexpr std::uint8_t kRoleToenail = 9;
constexpr std::uint8_t kRoleTailTip = 10;

constexpr float k_pi = 3.14159265358979323846F;
constexpr float k_two_pi = 2.0F * k_pi;

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
  QVector3D shoulder_to_foot = foot - shoulder;
  float const dist_sq =
      QVector3D::dotProduct(shoulder_to_foot, shoulder_to_foot);
  if (dist_sq <= 1.0e-6F) {
    return shoulder + QVector3D(0.0F, -upper_len, 0.0F);
  }

  float const total_len = std::max(upper_len + lower_len, 1.0e-4F);
  float const min_len =
      std::max(std::abs(upper_len - lower_len) + 1.0e-4F, total_len * 0.10F);
  float const dist =
      std::clamp(std::sqrt(dist_sq), min_len, total_len - 1.0e-4F);
  QVector3D const dir = shoulder_to_foot / std::sqrt(dist_sq);

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

[[nodiscard]] auto darken(const QVector3D &c, float s) noexcept -> QVector3D {
  return c * s;
}

struct torso_ring {
  float z{0.0F};
  float y{0.0F};
  float half_width{0.0F};
  float top{0.0F};
  float bottom{0.0F};
};

auto make_faceted_vertex(const QVector3D &pos, const QVector3D &normal, float u,
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
  vertices.push_back(make_faceted_vertex(a, normal, 0.0F, 0.0F));
  vertices.push_back(make_faceted_vertex(b, normal, 1.0F, 0.0F));
  vertices.push_back(make_faceted_vertex(c, normal, 0.5F, 1.0F));
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
  vertices.push_back(make_faceted_vertex(a, normal, 0.0F, 0.0F));
  vertices.push_back(make_faceted_vertex(b, normal, 1.0F, 0.0F));
  vertices.push_back(make_faceted_vertex(c, normal, 1.0F, 1.0F));
  vertices.push_back(make_faceted_vertex(d, normal, 0.0F, 1.0F));
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

template <std::size_t VertexCount>
void append_closed_prism(std::vector<Render::GL::Vertex> &vertices,
                         std::vector<unsigned int> &indices,
                         const std::array<QVector3D, VertexCount> &front,
                         const std::array<QVector3D, VertexCount> &back) {
  for (std::size_t i = 0; i < VertexCount; ++i) {
    std::size_t const next = (i + 1U) % VertexCount;
    append_flat_quad(vertices, indices, front[i], front[next], back[next],
                     back[i]);
  }

  QVector3D front_center;
  QVector3D back_center;
  for (std::size_t i = 0; i < VertexCount; ++i) {
    front_center += front[i];
    back_center += back[i];
  }
  front_center /= static_cast<float>(VertexCount);
  back_center /= static_cast<float>(VertexCount);

  for (std::size_t i = 0; i < VertexCount; ++i) {
    std::size_t const next = (i + 1U) % VertexCount;
    append_flat_triangle(vertices, indices, front_center, front[i],
                         front[next]);
    append_flat_triangle(vertices, indices, back_center, back[next], back[i]);
  }
}

auto create_faceted_elephant_torso_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  float const bw = dims.body_width;
  float const bh = dims.body_height;
  float const bl = dims.body_length;

  std::array<torso_ring, 8> const rings{{
      {-0.58F, 0.00F, 0.18F, 0.16F, 0.14F},
      {-0.46F, 0.04F, 0.34F, 0.30F, 0.20F},
      {-0.24F, 0.10F, 0.50F, 0.42F, 0.28F},
      {0.00F, 0.12F, 0.58F, 0.48F, 0.34F},
      {0.22F, 0.14F, 0.60F, 0.52F, 0.36F},
      {0.40F, 0.10F, 0.54F, 0.46F, 0.30F},
      {0.54F, 0.02F, 0.42F, 0.34F, 0.24F},
      {0.66F, -0.06F, 0.24F, 0.22F, 0.18F},
  }};

  std::array<std::array<QVector3D, 10>, 8> ring_points{};
  for (std::size_t r = 0; r < rings.size(); ++r) {
    torso_ring const &ring = rings[r];
    float const z = ring.z * bl;
    float const yc = ring.y * bh;
    float const hw = ring.half_width * bw;
    float const top = yc + ring.top * bh;
    float const bot = yc - ring.bottom * bh;
    float const shoulder_y = yc + ring.top * bh * 0.78F;
    float const side_high_y = yc + ring.top * bh * 0.24F;
    float const side_low_y = yc - ring.bottom * bh * 0.18F;
    float const belly_y = yc - ring.bottom * bh * 0.76F;
    float const shoulder = hw * 0.58F;
    float const side_high = hw * 0.90F;
    float const side_low = hw * 1.02F;
    float const belly = hw * 0.66F;

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

auto get_faceted_elephant_torso_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_faceted_elephant_torso_mesh();
  return mesh.get();
}

auto create_whole_elephant_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  float const bw = dims.body_width;
  float const bh = dims.body_height;
  float const bl = dims.body_length;
  float const hl = dims.head_length;
  float const hw = dims.head_width;
  float const hh = dims.head_height;
  float const torso_width_scale = 2.0F;
  float const torso_height_scale = 2.0F;
  float const torso_lift = bh * 0.18F;
  float const trunk_thickness_scale = 0.50F;

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(560);
  indices.reserve(900);

  auto make_ring = [](QVector3D c, float rx,
                      float ry) -> std::array<QVector3D, 8> {
    return {{
        {c.x(), c.y() + ry, c.z()},
        {c.x() + rx * 0.70F, c.y() + ry * 0.62F, c.z()},
        {c.x() + rx, c.y(), c.z()},
        {c.x() + rx * 0.72F, c.y() - ry * 0.70F, c.z()},
        {c.x(), c.y() - ry, c.z()},
        {c.x() - rx * 0.72F, c.y() - ry * 0.70F, c.z()},
        {c.x() - rx, c.y(), c.z()},
        {c.x() - rx * 0.70F, c.y() + ry * 0.62F, c.z()},
    }};
  };

  std::array<std::array<QVector3D, 8>, 8> body{{
      make_ring({0.0F, -bh * 0.02F + torso_lift, -bl * 0.58F},
                bw * 0.20F * torso_width_scale,
                bh * 0.20F * torso_height_scale),
      make_ring({0.0F, bh * 0.04F + torso_lift, -bl * 0.44F},
                bw * 0.44F * torso_width_scale,
                bh * 0.34F * torso_height_scale),
      make_ring({0.0F, bh * 0.10F + torso_lift, -bl * 0.22F},
                bw * 0.58F * torso_width_scale,
                bh * 0.46F * torso_height_scale),
      make_ring({0.0F, bh * 0.12F + torso_lift, 0.02F},
                bw * 0.64F * torso_width_scale,
                bh * 0.50F * torso_height_scale),
      make_ring({0.0F, bh * 0.14F + torso_lift, bl * 0.22F},
                bw * 0.62F * torso_width_scale,
                bh * 0.52F * torso_height_scale),
      make_ring({0.0F, bh * 0.08F + torso_lift, bl * 0.42F},
                bw * 0.52F * torso_width_scale,
                bh * 0.42F * torso_height_scale),
      make_ring({0.0F, -bh * 0.02F + torso_lift, bl * 0.58F},
                bw * 0.36F * torso_width_scale,
                bh * 0.30F * torso_height_scale),
      make_ring({0.0F, -bh * 0.08F + torso_lift, bl * 0.68F},
                bw * 0.20F * torso_width_scale,
                bh * 0.18F * torso_height_scale),
  }};
  append_closed_ring_mesh(vertices, indices, body);

  std::array<std::array<QVector3D, 8>, 5> head{{
      make_ring({0.0F, bh * 0.30F, bl * 0.62F}, hw * 0.22F, hh * 0.18F),
      make_ring({0.0F, bh * 0.42F, bl * 0.78F}, hw * 0.44F, hh * 0.34F),
      make_ring({0.0F, bh * 0.42F, bl * 0.98F}, hw * 0.50F, hh * 0.38F),
      make_ring({0.0F, bh * 0.28F, bl * 1.13F}, hw * 0.34F, hh * 0.26F),
      make_ring({0.0F, bh * 0.14F, bl * 1.23F}, hw * 0.16F, hh * 0.14F),
  }};
  append_closed_ring_mesh(vertices, indices, head);

  auto make_cylinder_ring = [](QVector3D c, float rx,
                               float rz) -> std::array<QVector3D, 6> {
    return {{
        {c.x(), c.y(), c.z() + rz},
        {c.x() + rx, c.y(), c.z() + rz * 0.45F},
        {c.x() + rx * 0.88F, c.y(), c.z() - rz * 0.52F},
        {c.x(), c.y(), c.z() - rz},
        {c.x() - rx * 0.88F, c.y(), c.z() - rz * 0.52F},
        {c.x() - rx, c.y(), c.z() + rz * 0.45F},
    }};
  };

  auto append_leg = [&](float x, float z, bool front) {
    float const top_y = -bh * 0.22F;
    float const bottom_y = -dims.leg_length * 0.82F;
    float const r = dims.leg_radius * (front ? 1.20F : 1.12F);
    std::array<std::array<QVector3D, 6>, 5> leg{{
        make_cylinder_ring({x, top_y, z}, r * 1.18F, r * 0.92F),
        make_cylinder_ring({x * 0.98F, (top_y + bottom_y) * 0.38F, z},
                           r * 1.02F, r * 0.82F),
        make_cylinder_ring({x * 0.96F, (top_y + bottom_y) * 0.62F, z},
                           r * 0.86F, r * 0.72F),
        make_cylinder_ring({x * 0.94F, bottom_y + dims.foot_radius * 0.20F, z},
                           r * 1.10F, r * 0.88F),
        make_cylinder_ring({x * 0.94F, bottom_y, z + dims.foot_radius * 0.14F},
                           r * 1.42F, r * 1.22F),
    }};
    append_closed_ring_mesh(vertices, indices, leg);
  };
  append_leg(bw * 0.42F, bl * 0.34F, true);
  append_leg(-bw * 0.42F, bl * 0.34F, true);
  append_leg(bw * 0.38F, -bl * 0.34F, false);
  append_leg(-bw * 0.38F, -bl * 0.34F, false);

  std::array<std::array<QVector3D, 6>, 6> trunk{{
      make_cylinder_ring({0.0F, bh * 0.12F, bl * 1.18F},
                         dims.trunk_base_radius * 1.20F * trunk_thickness_scale,
                         dims.trunk_base_radius * 0.92F *
                             trunk_thickness_scale),
      make_cylinder_ring({0.0F, -bh * 0.04F, bl * 1.28F},
                         dims.trunk_base_radius * 1.02F * trunk_thickness_scale,
                         dims.trunk_base_radius * 0.84F *
                             trunk_thickness_scale),
      make_cylinder_ring({0.0F, -bh * 0.26F, bl * 1.34F},
                         dims.trunk_base_radius * 0.82F * trunk_thickness_scale,
                         dims.trunk_base_radius * 0.70F *
                             trunk_thickness_scale),
      make_cylinder_ring({0.0F, -bh * 0.48F, bl * 1.36F},
                         dims.trunk_base_radius * 0.62F * trunk_thickness_scale,
                         dims.trunk_base_radius * 0.54F *
                             trunk_thickness_scale),
      make_cylinder_ring({0.0F, -bh * 0.66F, bl * 1.33F},
                         dims.trunk_base_radius * 0.44F * trunk_thickness_scale,
                         dims.trunk_base_radius * 0.40F *
                             trunk_thickness_scale),
      make_cylinder_ring({0.0F, -bh * 0.78F, bl * 1.25F},
                         dims.trunk_tip_radius * 1.60F * trunk_thickness_scale,
                         dims.trunk_tip_radius * 1.40F * trunk_thickness_scale),
  }};
  append_closed_ring_mesh(vertices, indices, trunk);

  auto append_ear = [&](float side) {
    float const x_outer = side * (hw * 0.72F);
    float const x_inner = side * (hw * 0.30F);
    std::array<QVector3D, 7> const front{{
        {x_inner, bh * 0.66F, bl * 0.78F},
        {x_outer, bh * 0.58F, bl * 0.78F},
        {side * (hw * 0.88F), bh * 0.24F, bl * 0.82F},
        {side * (hw * 0.72F), -bh * 0.18F, bl * 0.86F},
        {side * (hw * 0.24F), -bh * 0.26F, bl * 0.84F},
        {side * (hw * 0.14F), bh * 0.12F, bl * 0.78F},
        {x_inner, bh * 0.44F, bl * 0.74F},
    }};
    std::array<QVector3D, 7> back = front;
    for (auto &p : back) {
      p.setZ(p.z() - hw * 0.08F);
    }
    append_closed_prism(vertices, indices, front, back);
  };
  append_ear(1.0F);
  append_ear(-1.0F);

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto get_whole_elephant_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_whole_elephant_mesh();
  return mesh.get();
}

auto create_faceted_elephant_head_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  float const hw = dims.head_width;
  float const hh = dims.head_height;
  float const hl = dims.head_length;

  std::array<torso_ring, 6> const sections{{
      {-0.42F, 0.02F, 0.18F, 0.16F, 0.12F},
      {-0.22F, 0.12F, 0.30F, 0.28F, 0.18F},
      {0.00F, 0.16F, 0.42F, 0.34F, 0.20F},
      {0.20F, 0.08F, 0.36F, 0.24F, 0.26F},
      {0.38F, -0.02F, 0.24F, 0.14F, 0.24F},
      {0.52F, -0.14F, 0.12F, 0.06F, 0.16F},
  }};

  std::array<std::array<QVector3D, 8>, 6> section_points{};
  for (std::size_t s = 0; s < sections.size(); ++s) {
    torso_ring const &section = sections[s];
    float const z = section.z * hl;
    float const yc = section.y * hh;
    float const half_width = section.half_width * hw;
    float const top = yc + section.top * hh;
    float const bot = yc - section.bottom * hh;
    float const crown_y = yc + section.top * hh * 0.58F;
    float const jaw_y = yc - section.bottom * hh * 0.42F;
    float const crown_w = half_width * 0.54F;
    float const side_w = half_width;
    float const jaw_w = half_width * 0.74F;

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

auto get_faceted_elephant_head_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_faceted_elephant_head_mesh();
  return mesh.get();
}

auto create_minimal_elephant_head_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  float const hw = dims.head_width;
  float const hh = dims.head_height;
  float const hl = dims.head_length;

  std::array<torso_ring, 5> const sections{{
      {-0.34F, 0.04F, 0.22F, 0.14F, 0.10F},
      {-0.16F, 0.12F, 0.34F, 0.24F, 0.16F},
      {0.08F, 0.12F, 0.42F, 0.28F, 0.18F},
      {0.30F, 0.00F, 0.28F, 0.16F, 0.20F},
      {0.48F, -0.12F, 0.14F, 0.06F, 0.14F},
  }};

  std::array<std::array<QVector3D, 8>, 5> rings{};
  for (std::size_t s = 0; s < sections.size(); ++s) {
    torso_ring const &section = sections[s];
    float const z = section.z * hl;
    float const yc = section.y * hh;
    float const half_width = section.half_width * hw;
    float const top = yc + section.top * hh;
    float const bot = yc - section.bottom * hh;
    float const crown_y = yc + section.top * hh * 0.56F;
    float const jaw_y = yc - section.bottom * hh * 0.38F;
    float const crown_w = half_width * 0.52F;
    float const side_w = half_width;
    float const jaw_w = half_width * 0.74F;

    rings[s] = {{
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

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  auto head_shell = build_closed_ring_mesh(rings);
  vertices = head_shell->get_vertices();
  indices = head_shell->get_indices();

  auto append_ear = [&](float lateral_sign) {
    QVector3D const ear_scale(hw * 0.56F, hh * 0.72F, hl * 0.045F);
    QVector3D const ear_offset(lateral_sign * hw * 0.36F, -hh * 0.02F,
                               -hl * 0.04F);
    std::array<QVector3D, 6> front{};
    std::array<QVector3D, 6> back{};
    std::array<QVector3D, 6> const profile{{
        {0.00F, 0.92F, 1.0F},
        {0.68F, 0.72F, 1.0F},
        {0.96F, 0.12F, 1.0F},
        {0.78F, -0.62F, 1.0F},
        {0.08F, -1.0F, 1.0F},
        {-0.18F, -0.26F, 1.0F},
    }};
    for (std::size_t i = 0; i < profile.size(); ++i) {
      QVector3D p = profile[i];
      p.setX(p.x() * lateral_sign);
      front[i] = ear_offset + QVector3D(p.x() * ear_scale.x(),
                                        p.y() * ear_scale.y(), ear_scale.z());
      back[i] = ear_offset + QVector3D(p.x() * ear_scale.x(),
                                       p.y() * ear_scale.y(), -ear_scale.z());
    }
    append_closed_prism(vertices, indices, front, back);
  };

  append_ear(1.0F);
  append_ear(-1.0F);
  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto get_minimal_elephant_head_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_minimal_elephant_head_mesh();
  return mesh.get();
}

auto create_unit_elephant_ear_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  constexpr float kThickness = 0.08F;
  std::array<QVector3D, 8> const front{{
      {0.0F, 1.0F, kThickness},
      {0.68F, 0.78F, kThickness},
      {1.0F, 0.18F, kThickness},
      {0.82F, -0.58F, kThickness},
      {0.12F, -1.0F, kThickness},
      {-0.62F, -0.70F, kThickness},
      {-0.92F, -0.06F, kThickness},
      {-0.48F, 0.72F, kThickness},
  }};
  std::array<QVector3D, 8> const back{{
      {0.0F, 1.0F, -kThickness},
      {0.68F, 0.78F, -kThickness},
      {1.0F, 0.18F, -kThickness},
      {0.82F, -0.58F, -kThickness},
      {0.12F, -1.0F, -kThickness},
      {-0.62F, -0.70F, -kThickness},
      {-0.92F, -0.06F, -kThickness},
      {-0.48F, 0.72F, -kThickness},
  }};

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(80);
  indices.reserve(132);
  append_closed_prism(vertices, indices, front, back);

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto get_unit_elephant_ear_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_unit_elephant_ear_mesh();
  return mesh.get();
}

auto create_unit_elephant_minimal_leg_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  std::array<std::array<QVector3D, 6>, 5> const rings{{
      {{{0.0F, 0.50F, 0.30F},
        {0.32F, 0.50F, 0.20F},
        {0.34F, 0.50F, -0.18F},
        {0.0F, 0.50F, -0.24F},
        {-0.34F, 0.50F, -0.18F},
        {-0.32F, 0.50F, 0.20F}}},
      {{{0.0F, 0.18F, 0.28F},
        {0.30F, 0.18F, 0.18F},
        {0.31F, 0.18F, -0.18F},
        {0.0F, 0.18F, -0.22F},
        {-0.31F, 0.18F, -0.18F},
        {-0.30F, 0.18F, 0.18F}}},
      {{{0.0F, -0.12F, 0.22F},
        {0.24F, -0.12F, 0.14F},
        {0.25F, -0.12F, -0.14F},
        {0.0F, -0.12F, -0.18F},
        {-0.25F, -0.12F, -0.14F},
        {-0.24F, -0.12F, 0.14F}}},
      {{{0.0F, -0.42F, 0.30F},
        {0.34F, -0.42F, 0.18F},
        {0.32F, -0.42F, -0.16F},
        {0.0F, -0.42F, -0.20F},
        {-0.32F, -0.42F, -0.16F},
        {-0.34F, -0.42F, 0.18F}}},
      {{{0.0F, -0.58F, 0.36F},
        {0.38F, -0.58F, 0.22F},
        {0.36F, -0.58F, -0.18F},
        {0.0F, -0.58F, -0.22F},
        {-0.36F, -0.58F, -0.18F},
        {-0.38F, -0.58F, 0.22F}}},
  }};
  return build_closed_ring_mesh(rings);
}

auto get_unit_elephant_minimal_leg_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_unit_elephant_minimal_leg_mesh();
  return mesh.get();
}

auto create_unit_elephant_minimal_trunk_mesh()
    -> std::unique_ptr<Render::GL::Mesh> {
  std::array<std::array<QVector3D, 6>, 5> const rings{{
      {{{0.0F, 0.50F, 0.18F},
        {0.26F, 0.50F, 0.10F},
        {0.24F, 0.50F, -0.12F},
        {0.0F, 0.50F, -0.16F},
        {-0.24F, 0.50F, -0.12F},
        {-0.26F, 0.50F, 0.10F}}},
      {{{0.0F, 0.20F, 0.30F},
        {0.22F, 0.20F, 0.24F},
        {0.20F, 0.20F, 0.04F},
        {0.0F, 0.20F, 0.00F},
        {-0.20F, 0.20F, 0.04F},
        {-0.22F, 0.20F, 0.24F}}},
      {{{0.0F, -0.05F, 0.42F},
        {0.16F, -0.05F, 0.36F},
        {0.14F, -0.05F, 0.20F},
        {0.0F, -0.05F, 0.16F},
        {-0.14F, -0.05F, 0.20F},
        {-0.16F, -0.05F, 0.36F}}},
      {{{0.0F, -0.30F, 0.52F},
        {0.11F, -0.30F, 0.48F},
        {0.10F, -0.30F, 0.34F},
        {0.0F, -0.30F, 0.30F},
        {-0.10F, -0.30F, 0.34F},
        {-0.11F, -0.30F, 0.48F}}},
      {{{0.0F, -0.56F, 0.62F},
        {0.07F, -0.56F, 0.60F},
        {0.06F, -0.56F, 0.50F},
        {0.0F, -0.56F, 0.46F},
        {-0.06F, -0.56F, 0.50F},
        {-0.07F, -0.56F, 0.60F}}},
  }};
  return build_closed_ring_mesh(rings);
}

auto get_unit_elephant_minimal_trunk_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_unit_elephant_minimal_trunk_mesh();
  return mesh.get();
}

auto create_unit_elephant_foot_mesh() -> std::unique_ptr<Render::GL::Mesh> {
  QVector3D const v0(-0.96F, 0.34F, -0.82F);
  QVector3D const v1(0.96F, 0.34F, -0.82F);
  QVector3D const v2(1.0F, 0.12F, 0.88F);
  QVector3D const v3(-1.0F, 0.12F, 0.88F);
  QVector3D const v4(-0.92F, -0.58F, -0.76F);
  QVector3D const v5(0.92F, -0.58F, -0.76F);
  QVector3D const v6(0.98F, -0.58F, 0.94F);
  QVector3D const v7(-0.98F, -0.58F, 0.94F);

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(24);
  indices.reserve(36);
  append_flat_quad(vertices, indices, v0, v1, v2, v3);
  append_flat_quad(vertices, indices, v4, v7, v6, v5);
  append_flat_quad(vertices, indices, v0, v3, v7, v4);
  append_flat_quad(vertices, indices, v1, v5, v6, v2);
  append_flat_quad(vertices, indices, v0, v4, v5, v1);
  append_flat_quad(vertices, indices, v3, v2, v6, v7);

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto get_unit_elephant_foot_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_unit_elephant_foot_mesh();
  return mesh.get();
}

auto create_unit_prism_segment_mesh(int sides)
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
    float const ang = t * k_two_pi;
    float const x = std::cos(ang);
    float const z = std::sin(ang);
    top.push_back(QVector3D(x, 0.5F, z));
    bottom.push_back(QVector3D(x, -0.5F, z));
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

auto get_unit_elephant_segment_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_unit_prism_segment_mesh(6);
  return mesh.get();
}

auto create_unit_pyramid_cone_mesh(int sides)
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
    float const ang = t * k_two_pi;
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

auto get_unit_elephant_tusk_mesh() -> Render::GL::Mesh * {
  static std::unique_ptr<Render::GL::Mesh> const mesh =
      create_unit_pyramid_cone_mesh(5);
  return mesh.get();
}

struct LegResult {
  QVector3D shoulder;
  QVector3D foot;
};

[[nodiscard]] auto compute_pose_leg(const Render::GL::ElephantDimensions &d,
                                    const Render::GL::ElephantGait &g,
                                    const ElephantPoseMotion &motion,
                                    float lateral_sign, float forward_bias,
                                    float phase_offset) noexcept -> LegResult {
  float const leg_phase =
      motion.is_fighting
          ? std::fmod(motion.anim_time / 1.15F + phase_offset, 1.0F)
          : std::fmod(motion.phase + phase_offset, 1.0F);

  float stride = 0.0F;
  float lift = 0.0F;

  if (motion.is_fighting) {
    bool const is_front = (forward_bias > 0.0F);
    float const local = leg_phase;

    float intensity = 0.70F;
    switch (motion.combat_phase) {
    case Render::GL::CombatAnimPhase::WindUp:
      intensity = 0.85F;
      break;
    case Render::GL::CombatAnimPhase::Strike:
    case Render::GL::CombatAnimPhase::Impact:
      intensity = 1.0F;
      break;
    case Render::GL::CombatAnimPhase::Recover:
      intensity = 0.80F;
      break;
    default:
      break;
    }

    float const base_stomp = d.leg_length * 0.58F;
    float const stomp_height =
        base_stomp * (0.7F + 0.3F * intensity) * (is_front ? 1.0F : 0.80F);
    float const stomp_stride = g.stride_swing * 0.32F * intensity;
    float const impact_sink =
        d.foot_radius * (0.20F + 0.10F * intensity) * (is_front ? 1.0F : 0.85F);

    if (local < 0.45F) {
      float const u = local / 0.45F;
      float const ease = 1.0F - std::cos(u * k_pi * 0.5F);
      lift = ease * stomp_height;
      stride = stomp_stride * ease * 0.35F;
    } else if (local < 0.65F) {
      lift = stomp_height;
      stride = stomp_stride * 0.35F;
    } else if (local < 0.78F) {
      float const u = (local - 0.65F) / 0.13F;
      float const slam = (1.0F - u);
      float const slam_pow = slam * slam * slam * slam;
      lift = slam_pow * stomp_height - impact_sink * (u * u);
      stride = stomp_stride * (0.35F + u * 0.65F);
    } else {
      float const u = (local - 0.78F) / 0.22F;
      float const recover = 1.0F - (u * u);
      lift = -impact_sink * recover;
      stride = stomp_stride * (1.0F - u * 0.25F);
    }
  } else if (motion.is_moving) {
    float const angle = leg_phase * 2.0F * k_pi;
    stride = std::sin(angle) * g.stride_swing * 0.6F;
    float const lift_raw = std::sin(angle);
    lift = lift_raw > 0.0F ? lift_raw * g.stride_lift * 0.8F : 0.0F;
  }

  QVector3D const shoulder(lateral_sign * d.body_width * 0.40F,
                           -d.body_height * 0.30F, forward_bias + stride);
  QVector3D const foot =
      shoulder + QVector3D(0.0F, -d.leg_length * 0.85F + lift, stride * 0.3F);

  return {shoulder, foot};
}

auto build_minimal_primitives(const ElephantSpecPose &pose,
                              std::array<PrimitiveInstance, 1> &out) noexcept
    -> std::size_t {
  (void)pose;
  {
    PrimitiveInstance &p = out[0];
    p.debug_name = "elephant.minimal.whole";
    p.shape = PrimitiveShape::Mesh;
    p.custom_mesh = get_whole_elephant_mesh();
    p.mesh_skinning = Render::Creature::MeshSkinning::ElephantWhole;
    p.params.anchor_bone = static_cast<BoneIndex>(ElephantBone::Root);
    p.params.half_extents = QVector3D(1.0F, 1.0F, 1.0F);
    p.color_role = kRoleSkin;
    p.material_id = 6;
    p.lod_mask = kLodMinimal;
  }
  return 1;
}

} // namespace

auto elephant_topology() noexcept -> const SkeletonTopology & {
  return kElephantTopology;
}

void evaluate_elephant_skeleton(const ElephantSpecPose &pose,
                                BonePalette &out_palette) noexcept {
  out_palette[static_cast<std::size_t>(ElephantBone::Root)] =
      translation_matrix(pose.barrel_center);

  QMatrix4x4 body;
  body.setColumn(0, QVector4D(pose.body_ellipsoid_x, 0.0F, 0.0F, 0.0F));
  body.setColumn(1, QVector4D(0.0F, pose.body_ellipsoid_y, 0.0F, 0.0F));
  body.setColumn(2, QVector4D(0.0F, 0.0F, pose.body_ellipsoid_z, 0.0F));
  body.setColumn(3, QVector4D(pose.barrel_center, 1.0F));
  out_palette[static_cast<std::size_t>(ElephantBone::Body)] = body;

  out_palette[static_cast<std::size_t>(ElephantBone::ShoulderFL)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_fl);
  out_palette[static_cast<std::size_t>(ElephantBone::ShoulderFR)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_fr);
  out_palette[static_cast<std::size_t>(ElephantBone::ShoulderBL)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_bl);
  out_palette[static_cast<std::size_t>(ElephantBone::ShoulderBR)] =
      translation_matrix(pose.barrel_center + pose.shoulder_offset_pose_br);

  out_palette[static_cast<std::size_t>(ElephantBone::KneeFL)] =
      translation_matrix(pose.knee_fl);
  out_palette[static_cast<std::size_t>(ElephantBone::KneeFR)] =
      translation_matrix(pose.knee_fr);
  out_palette[static_cast<std::size_t>(ElephantBone::KneeBL)] =
      translation_matrix(pose.knee_bl);
  out_palette[static_cast<std::size_t>(ElephantBone::KneeBR)] =
      translation_matrix(pose.knee_br);

  out_palette[static_cast<std::size_t>(ElephantBone::FootFL)] =
      translation_matrix(pose.foot_fl);
  out_palette[static_cast<std::size_t>(ElephantBone::FootFR)] =
      translation_matrix(pose.foot_fr);
  out_palette[static_cast<std::size_t>(ElephantBone::FootBL)] =
      translation_matrix(pose.foot_bl);
  out_palette[static_cast<std::size_t>(ElephantBone::FootBR)] =
      translation_matrix(pose.foot_br);

  out_palette[static_cast<std::size_t>(ElephantBone::Head)] =
      translation_matrix(pose.head_center);
  out_palette[static_cast<std::size_t>(ElephantBone::TrunkTip)] =
      translation_matrix(pose.trunk_end);
}

void fill_elephant_role_colors(
    const Render::GL::ElephantVariant &variant,
    std::array<QVector3D, kElephantRoleCount> &out_roles) noexcept {
  out_roles[0] = variant.skin_color;
  out_roles[1] = variant.skin_color;
  out_roles[2] = darken(variant.skin_color, 0.92F);
  out_roles[3] = darken(variant.skin_color, 0.88F);
  out_roles[4] = darken(variant.skin_color, 0.94F);
  out_roles[5] = variant.ear_inner_color;
  out_roles[6] = variant.tusk_color;
  out_roles[7] = QVector3D(0.04F, 0.03F, 0.03F);
  out_roles[8] = variant.toenail_color;
  out_roles[9] = QVector3D(0.05F, 0.04F, 0.04F);
}

void make_elephant_spec_pose(const Render::GL::ElephantDimensions &dims,
                             float bob, ElephantSpecPose &out_pose) noexcept {
  QVector3D const center(0.0F, dims.barrel_center_y + bob, 0.0F);
  out_pose.barrel_center = center;

  out_pose.body_ellipsoid_x = dims.body_width * 1.2F;
  out_pose.body_ellipsoid_y = dims.body_height + dims.neck_length * 0.3F;
  out_pose.body_ellipsoid_z = dims.body_length + dims.head_length * 0.3F;

  float const shoulder_dx = dims.body_width * 0.38F;
  float const shoulder_dy = -dims.body_height * 0.25F;
  float const front_dz = dims.body_length * 0.30F;
  float const rear_dz = -dims.body_length * 0.30F;

  out_pose.shoulder_offset_fl = QVector3D(shoulder_dx, shoulder_dy, front_dz);
  out_pose.shoulder_offset_fr = QVector3D(-shoulder_dx, shoulder_dy, front_dz);
  out_pose.shoulder_offset_bl = QVector3D(shoulder_dx, shoulder_dy, rear_dz);
  out_pose.shoulder_offset_br = QVector3D(-shoulder_dx, shoulder_dy, rear_dz);

  float const drop = -dims.leg_length * 0.70F;
  out_pose.foot_fl =
      center + out_pose.shoulder_offset_fl + QVector3D(0, drop, 0);
  out_pose.foot_fr =
      center + out_pose.shoulder_offset_fr + QVector3D(0, drop, 0);
  out_pose.foot_bl =
      center + out_pose.shoulder_offset_bl + QVector3D(0, drop, 0);
  out_pose.foot_br =
      center + out_pose.shoulder_offset_br + QVector3D(0, drop, 0);

  out_pose.shoulder_offset_pose_fl = out_pose.shoulder_offset_fl;
  out_pose.shoulder_offset_pose_fr = out_pose.shoulder_offset_fr;
  out_pose.shoulder_offset_pose_bl = out_pose.shoulder_offset_bl;
  out_pose.shoulder_offset_pose_br = out_pose.shoulder_offset_br;

  QVector3D const shoulder_fl = center + out_pose.shoulder_offset_pose_fl;
  QVector3D const shoulder_fr = center + out_pose.shoulder_offset_pose_fr;
  QVector3D const shoulder_bl = center + out_pose.shoulder_offset_pose_bl;
  QVector3D const shoulder_br = center + out_pose.shoulder_offset_pose_br;
  float const upper_len = dims.leg_length * 0.46F;
  float const lower_len = dims.leg_length * 0.44F;
  QVector3D const front_bend_hint(0.0F, 0.0F, dims.body_length * 0.10F);
  QVector3D const rear_bend_hint(0.0F, 0.0F, -dims.body_length * 0.08F);
  out_pose.knee_fl = solve_bent_leg_joint(
      shoulder_fl, out_pose.foot_fl, upper_len, lower_len, front_bend_hint);
  out_pose.knee_fr = solve_bent_leg_joint(
      shoulder_fr, out_pose.foot_fr, upper_len, lower_len, front_bend_hint);
  out_pose.knee_bl = solve_bent_leg_joint(shoulder_bl, out_pose.foot_bl,
                                          upper_len, lower_len, rear_bend_hint);
  out_pose.knee_br = solve_bent_leg_joint(shoulder_br, out_pose.foot_br,
                                          upper_len, lower_len, rear_bend_hint);

  out_pose.leg_radius = dims.leg_radius * 0.70F;
}

void make_elephant_spec_pose_animated(
    const Render::GL::ElephantDimensions &dims,
    const Render::GL::ElephantGait &gait, const ElephantPoseMotion &motion,
    ElephantSpecPose &out_pose) noexcept {

  make_elephant_spec_pose(dims, motion.bob, out_pose);

  QVector3D const center = out_pose.barrel_center;

  out_pose.body_half =
      QVector3D(dims.body_width * 0.50F, dims.body_height * 0.45F,
                dims.body_length * 0.375F);

  QVector3D const neck_base_world =
      center +
      QVector3D(0.0F, dims.body_height * 0.20F, dims.body_length * 0.45F);
  out_pose.neck_base_offset = neck_base_world - center;
  out_pose.neck_radius = dims.neck_width * 0.85F;

  out_pose.head_center =
      neck_base_world +
      QVector3D(0.0F, dims.neck_length * 0.50F, dims.head_length * 0.60F);

  out_pose.head_half =
      QVector3D(dims.head_width * 0.425F, dims.head_height * 0.40F,
                dims.head_length * 0.35F);

  out_pose.trunk_end =
      out_pose.head_center +
      QVector3D(0.0F, -dims.trunk_length * 0.50F, dims.trunk_length * 0.40F);
  out_pose.trunk_base_radius = dims.trunk_base_radius * 0.8F;

  float const front_forward = dims.body_length * 0.35F;
  float const rear_forward = -dims.body_length * 0.35F;

  auto apply_leg = [&](float lat, float fwd, float phase_offset,
                       QVector3D &shoulder_out, QVector3D &foot_out) noexcept {
    auto r = compute_pose_leg(dims, gait, motion, lat, fwd, phase_offset);
    shoulder_out = r.shoulder;
    foot_out = center + r.foot;
  };

  apply_leg(1.0F, front_forward, gait.front_leg_phase,
            out_pose.shoulder_offset_pose_fl, out_pose.foot_pose_fl);
  apply_leg(-1.0F, front_forward, gait.front_leg_phase + 0.50F,
            out_pose.shoulder_offset_pose_fr, out_pose.foot_pose_fr);
  apply_leg(1.0F, rear_forward, gait.rear_leg_phase,
            out_pose.shoulder_offset_pose_bl, out_pose.foot_pose_bl);
  apply_leg(-1.0F, rear_forward, gait.rear_leg_phase + 0.50F,
            out_pose.shoulder_offset_pose_br, out_pose.foot_pose_br);

  out_pose.foot_fl = out_pose.foot_pose_fl;
  out_pose.foot_fr = out_pose.foot_pose_fr;
  out_pose.foot_bl = out_pose.foot_pose_bl;
  out_pose.foot_br = out_pose.foot_pose_br;

  QVector3D const shoulder_fl = center + out_pose.shoulder_offset_pose_fl;
  QVector3D const shoulder_fr = center + out_pose.shoulder_offset_pose_fr;
  QVector3D const shoulder_bl = center + out_pose.shoulder_offset_pose_bl;
  QVector3D const shoulder_br = center + out_pose.shoulder_offset_pose_br;
  float const upper_len = dims.leg_length * 0.46F;
  float const lower_len = dims.leg_length * 0.44F;
  float const bend_scale = motion.is_moving || motion.is_fighting
                               ? 1.0F + std::min(gait.stride_lift * 5.0F, 0.45F)
                               : 1.0F;
  QVector3D const front_bend_hint(0.0F, 0.0F,
                                  dims.body_length * 0.08F * bend_scale);
  QVector3D const rear_bend_hint(0.0F, 0.0F,
                                 -dims.body_length * 0.06F * bend_scale);
  out_pose.knee_fl = solve_bent_leg_joint(
      shoulder_fl, out_pose.foot_fl, upper_len, lower_len, front_bend_hint);
  out_pose.knee_fr = solve_bent_leg_joint(
      shoulder_fr, out_pose.foot_fr, upper_len, lower_len, front_bend_hint);
  out_pose.knee_bl = solve_bent_leg_joint(shoulder_bl, out_pose.foot_bl,
                                          upper_len, lower_len, rear_bend_hint);
  out_pose.knee_br = solve_bent_leg_joint(shoulder_br, out_pose.foot_br,
                                          upper_len, lower_len, rear_bend_hint);

  out_pose.pose_leg_radius = dims.leg_radius * 0.85F;

  out_pose.foot_pad_offset_y = -dims.foot_radius * 0.18F;
  out_pose.foot_pad_half = QVector3D(dims.foot_radius, dims.foot_radius * 0.65F,
                                     dims.foot_radius * 1.10F);
}

} // namespace Render::Elephant

#include "../bone_palette_arena.h"
#include "../rigged_mesh_cache.h"
#include "../scene_renderer.h"

namespace Render::Elephant {

namespace {

auto build_baseline_pose() noexcept -> ElephantSpecPose {
  Render::GL::ElephantDimensions const dims =
      Render::GL::make_elephant_dimensions(0U);
  Render::GL::ElephantGait gait{};
  ElephantSpecPose pose{};
  make_elephant_spec_pose_animated(dims, gait, ElephantPoseMotion{}, pose);
  return pose;
}

auto baseline_pose() noexcept -> const ElephantSpecPose & {
  static const ElephantSpecPose pose = build_baseline_pose();
  return pose;
}

constexpr std::size_t kElephantFullPartCount = 1;

auto build_static_full_parts() noexcept
    -> std::array<PrimitiveInstance, kElephantFullPartCount> {
  (void)baseline_pose();
  std::array<PrimitiveInstance, kElephantFullPartCount> out{};

  using Render::Creature::kLodFull;

  auto root = static_cast<BoneIndex>(ElephantBone::Root);

  PrimitiveInstance &whole = out[0];
  whole.debug_name = "elephant.full.whole";
  whole.shape = PrimitiveShape::Mesh;
  whole.params.anchor_bone = root;
  whole.params.half_extents = QVector3D(1.0F, 1.0F, 1.0F);
  whole.custom_mesh = get_whole_elephant_mesh();
  whole.mesh_skinning = Render::Creature::MeshSkinning::ElephantWhole;
  whole.color_role = kRoleSkin;
  whole.material_id = 6;
  whole.lod_mask = kLodFull;
  return out;
}

constexpr std::size_t kElephantMinimalPartCount = 1;

auto build_static_minimal_parts() noexcept
    -> std::array<PrimitiveInstance, kElephantMinimalPartCount> {
  std::array<PrimitiveInstance, kElephantMinimalPartCount> out{};
  build_minimal_primitives(baseline_pose(), out);
  return out;
}

auto static_minimal_parts() noexcept
    -> const std::array<PrimitiveInstance, kElephantMinimalPartCount> & {
  static const auto parts = build_static_minimal_parts();
  return parts;
}

auto static_full_parts() noexcept
    -> const std::array<PrimitiveInstance, kElephantFullPartCount> & {
  static const auto parts = build_static_full_parts();
  return parts;
}

auto build_elephant_bind_palette() noexcept
    -> std::array<QMatrix4x4, kElephantBoneCount> {
  std::array<QMatrix4x4, kElephantBoneCount> out{};
  BonePalette tmp{};
  evaluate_elephant_skeleton(baseline_pose(), tmp);
  for (std::size_t i = 0; i < kElephantBoneCount; ++i) {
    out[i] = tmp[i];
  }
  return out;
}

} // namespace

auto elephant_creature_spec() noexcept
    -> const Render::Creature::CreatureSpec & {
  static const Render::Creature::CreatureSpec spec = [] {
    Render::Creature::CreatureSpec s;
    s.species_name = "elephant";
    s.topology = elephant_topology();
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

auto compute_elephant_bone_palette(const ElephantSpecPose &pose,
                                   std::span<QMatrix4x4> out_bones) noexcept
    -> std::uint32_t {
  if (out_bones.size() < kElephantBoneCount) {
    return 0U;
  }
  BonePalette tmp{};
  evaluate_elephant_skeleton(pose, tmp);
  for (std::size_t i = 0; i < kElephantBoneCount; ++i) {
    out_bones[i] = tmp[i];
  }
  return static_cast<std::uint32_t>(kElephantBoneCount);
}

auto elephant_bind_palette() noexcept -> std::span<const QMatrix4x4> {
  static const std::array<QMatrix4x4, kElephantBoneCount> palette =
      build_elephant_bind_palette();
  return std::span<const QMatrix4x4>(palette.data(), palette.size());
}

} // namespace Render::Elephant
