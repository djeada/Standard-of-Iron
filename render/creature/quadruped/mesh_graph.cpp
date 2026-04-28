#include "mesh_graph.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::Creature::Quadruped {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTwoPi = 2.0F * kPi;

auto make_vertex(const QVector3D &pos,
                 const QVector3D &normal) -> Render::GL::Vertex {
  QVector3D n = normal;
  if (n.lengthSquared() <= 1.0e-8F) {
    n = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    n.normalize();
  }
  return {{pos.x(), pos.y(), pos.z()}, {n.x(), n.y(), n.z()}, {0.0F, 0.0F}};
}

void append_flat_triangle(std::vector<Render::GL::Vertex> &vertices,
                          std::vector<unsigned int> &indices,
                          const QVector3D &a, const QVector3D &b,
                          const QVector3D &c) {
  QVector3D normal = QVector3D::crossProduct(b - a, c - a);
  if (normal.lengthSquared() <= 1.0e-8F) {
    normal = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    normal.normalize();
  }

  unsigned int const base = static_cast<unsigned int>(vertices.size());
  vertices.push_back(make_vertex(a, normal));
  vertices.push_back(make_vertex(b, normal));
  vertices.push_back(make_vertex(c, normal));
  indices.insert(indices.end(), {base, base + 1U, base + 2U});
}

void append_flat_quad(std::vector<Render::GL::Vertex> &vertices,
                      std::vector<unsigned int> &indices, const QVector3D &a,
                      const QVector3D &b, const QVector3D &c,
                      const QVector3D &d) {
  QVector3D normal = QVector3D::crossProduct(b - a, c - a);
  if (normal.lengthSquared() <= 1.0e-8F) {
    normal = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    normal.normalize();
  }

  unsigned int const base = static_cast<unsigned int>(vertices.size());
  vertices.push_back(make_vertex(a, normal));
  vertices.push_back(make_vertex(b, normal));
  vertices.push_back(make_vertex(c, normal));
  vertices.push_back(make_vertex(d, normal));
  indices.insert(indices.end(),
                 {base, base + 1U, base + 2U, base + 2U, base + 3U, base});
}

auto ring_basis(const QVector3D &tangent) -> std::pair<QVector3D, QVector3D> {
  QVector3D forward = tangent;
  if (forward.lengthSquared() <= 1.0e-8F) {
    forward = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    forward.normalize();
  }
  QVector3D up_ref =
      std::abs(QVector3D::dotProduct(forward, QVector3D(0.0F, 1.0F, 0.0F))) >
              0.95F
          ? QVector3D(1.0F, 0.0F, 0.0F)
          : QVector3D(0.0F, 1.0F, 0.0F);
  QVector3D right = QVector3D::crossProduct(up_ref, forward);
  if (right.lengthSquared() <= 1.0e-8F) {
    right = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    right.normalize();
  }
  QVector3D up = QVector3D::crossProduct(forward, right);
  if (up.lengthSquared() <= 1.0e-8F) {
    up = QVector3D(0.0F, 1.0F, 0.0F);
  } else {
    up.normalize();
  }
  return {right, up};
}

auto make_oriented_ring(const QVector3D &center, const QVector3D &tangent,
                        float radius_x, float radius_y,
                        std::size_t vertex_count) -> std::vector<QVector3D> {
  auto [right, up] = ring_basis(tangent);
  std::vector<QVector3D> ring;
  ring.reserve(vertex_count);
  for (std::size_t i = 0; i < vertex_count; ++i) {
    float const angle =
        (static_cast<float>(i) / static_cast<float>(vertex_count)) * kTwoPi;
    ring.push_back(center + right * (std::cos(angle) * radius_x) +
                   up * (std::sin(angle) * radius_y));
  }
  return ring;
}

auto make_oval_ring(float cx, float cz, float top_y, float bot_y,
                    float half_w) -> std::vector<QVector3D> {
  float const yc = (top_y + bot_y) * 0.5F;
  float const ry = (top_y - bot_y) * 0.5F;
  float const upper_y = yc + ry * 0.85F;
  float const upper_w = half_w * 0.45F;
  float const shoulder_y = yc + ry * 0.50F;
  float const shoulder_w = half_w * 0.85F;
  float const mid_y = yc - ry * 0.05F;
  float const mid_w = half_w;
  float const belly_y = yc - ry * 0.70F;
  float const belly_w = half_w * 0.65F;
  return {
      {cx, top_y, cz},
      {cx + upper_w, upper_y, cz},
      {cx + shoulder_w, shoulder_y, cz},
      {cx + mid_w, mid_y, cz},
      {cx + belly_w, belly_y, cz},
      {cx, bot_y, cz},
      {cx - belly_w, belly_y, cz},
      {cx - mid_w, mid_y, cz},
      {cx - shoulder_w, shoulder_y, cz},
      {cx - upper_w, upper_y, cz},
  };
}

auto make_round_ring(QVector3D center, float rx, float ry,
                     std::size_t vertex_count) -> std::vector<QVector3D> {
  return make_oriented_ring(center, QVector3D(0.0F, 0.0F, 1.0F), rx, ry,
                            vertex_count);
}

void append_ring_strip(std::vector<Render::GL::Vertex> &vertices,
                       std::vector<unsigned int> &indices,
                       const std::vector<std::vector<QVector3D>> &rings) {
  if (rings.size() < 2U || rings.front().size() < 3U) {
    return;
  }
  std::size_t const ring_vertices = rings.front().size();
  for (const auto &ring : rings) {
    if (ring.size() != ring_vertices) {
      return;
    }
  }

  for (std::size_t r = 0; r + 1U < rings.size(); ++r) {
    for (std::size_t p = 0; p < ring_vertices; ++p) {
      std::size_t const next = (p + 1U) % ring_vertices;
      append_flat_quad(vertices, indices, rings[r][p], rings[r][next],
                       rings[r + 1U][next], rings[r + 1U][p]);
    }
  }

  auto add_cap = [&](std::size_t row, bool reverse) {
    QVector3D center;
    for (const QVector3D &p : rings[row]) {
      center += p;
    }
    center /= static_cast<float>(ring_vertices);
    for (std::size_t p = 0; p < ring_vertices; ++p) {
      std::size_t const next = (p + 1U) % ring_vertices;
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
  add_cap(rings.size() - 1U, false);
}

void append_closed_prism(std::vector<Render::GL::Vertex> &vertices,
                         std::vector<unsigned int> &indices,
                         const std::vector<QVector3D> &front,
                         const std::vector<QVector3D> &back) {
  if (front.size() < 3U || front.size() != back.size()) {
    return;
  }
  for (std::size_t i = 0; i < front.size(); ++i) {
    std::size_t const next = (i + 1U) % front.size();
    append_flat_quad(vertices, indices, front[i], front[next], back[next],
                     back[i]);
  }

  QVector3D front_center;
  QVector3D back_center;
  for (std::size_t i = 0; i < front.size(); ++i) {
    front_center += front[i];
    back_center += back[i];
  }
  front_center /= static_cast<float>(front.size());
  back_center /= static_cast<float>(back.size());
  for (std::size_t i = 0; i < front.size(); ++i) {
    std::size_t const next = (i + 1U) % front.size();
    append_flat_triangle(vertices, indices, front_center, front[i],
                         front[next]);
    append_flat_triangle(vertices, indices, back_center, back[next], back[i]);
  }
}

auto build_barrel_mesh(const BarrelNode &node)
    -> std::unique_ptr<Render::GL::Mesh> {
  std::vector<std::vector<QVector3D>> rings;
  rings.reserve(node.rings.size());
  for (const BarrelRing &ring : node.rings) {
    rings.push_back(make_oval_ring(0.0F, ring.z * node.scale.z(),
                                   (ring.y + ring.top) * node.scale.y(),
                                   (ring.y - ring.bottom) * node.scale.y(),
                                   ring.half_width * node.scale.x()));
  }

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  append_ring_strip(vertices, indices, rings);
  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto build_ellipsoid_mesh(const EllipsoidNode &node)
    -> std::unique_ptr<Render::GL::Mesh> {
  std::size_t const ring_count = std::max<std::size_t>(node.ring_count, 3U);
  std::size_t const ring_vertices =
      std::max<std::size_t>(node.ring_vertices, 6U);
  std::vector<std::vector<QVector3D>> rings;
  rings.reserve(ring_count);
  for (std::size_t i = 0; i < ring_count; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(ring_count - 1U);
    float const phi = -0.5F * kPi + t * kPi;
    float const cross = std::max(std::abs(std::cos(phi)), 0.04F);
    float const z = node.center.z() + std::sin(phi) * node.radii.z();
    rings.push_back(make_round_ring({node.center.x(), node.center.y(), z},
                                    node.radii.x() * cross,
                                    node.radii.y() * cross, ring_vertices));
  }

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  append_ring_strip(vertices, indices, rings);
  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto build_column_leg_mesh(const ColumnLegNode &node)
    -> std::unique_ptr<Render::GL::Mesh> {
  std::size_t const ring_count = std::max<std::size_t>(node.ring_count, 3U);
  std::size_t const ring_vertices =
      std::max<std::size_t>(node.ring_vertices, 6U);
  std::vector<std::vector<QVector3D>> rings;
  rings.reserve(ring_count);
  for (std::size_t i = 0; i < ring_count; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(ring_count - 1U);
    float const y =
        node.top_center.y() + (node.bottom_y - node.top_center.y()) * t;
    float taper = 1.0F - (1.0F - node.shaft_taper) * std::min(t * 1.2F, 1.0F);
    if (i + 1U == ring_count) {
      taper *= node.foot_radius_scale;
    } else if (i + 2U == ring_count) {
      taper *= 1.0F + (node.foot_radius_scale - 1.0F) * 0.5F;
    }
    rings.push_back(make_oriented_ring(
        {node.top_center.x(), y, node.top_center.z()},
        QVector3D(0.0F, -1.0F, 0.0F), node.top_radius_x * taper,
        node.top_radius_z * taper, ring_vertices));
  }

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  append_ring_strip(vertices, indices, rings);
  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto build_path_tube_mesh(const QVector3D &start, const QVector3D &end,
                          float start_radius, float end_radius, float sag,
                          std::size_t segment_count, std::size_t ring_vertices)
    -> std::unique_ptr<Render::GL::Mesh> {
  segment_count = std::max<std::size_t>(segment_count, 2U);
  ring_vertices = std::max<std::size_t>(ring_vertices, 6U);
  std::vector<std::vector<QVector3D>> rings;
  rings.reserve(segment_count);
  for (std::size_t i = 0; i < segment_count; ++i) {
    float const t =
        static_cast<float>(i) / static_cast<float>(segment_count - 1U);
    QVector3D center = start * (1.0F - t) + end * t;
    center += QVector3D(0.0F, std::sin(t * kPi) * sag, 0.0F);
    QVector3D const tangent =
        (end - start) + QVector3D(0.0F, std::cos(t * kPi) * sag * 0.5F, 0.0F);
    float const radius = start_radius + (end_radius - start_radius) * t;
    rings.push_back(make_oriented_ring(center, tangent, radius, radius * 0.9F,
                                       ring_vertices));
  }

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  append_ring_strip(vertices, indices, rings);
  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto build_snout_mesh(const SnoutNode &node)
    -> std::unique_ptr<Render::GL::Mesh> {
  return build_path_tube_mesh(node.start, node.end, node.base_radius,
                              node.tip_radius, node.sag, node.segment_count,
                              node.ring_vertices);
}

auto build_flat_fan_mesh(const FlatFanNode &node)
    -> std::unique_ptr<Render::GL::Mesh> {
  QVector3D axis = node.thickness_axis;
  if (axis.lengthSquared() <= 1.0e-8F) {
    axis = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    axis.normalize();
  }
  QVector3D const half_offset = axis * (node.thickness * 0.5F);
  std::vector<QVector3D> front;
  std::vector<QVector3D> back;
  front.reserve(node.outline.size());
  back.reserve(node.outline.size());
  for (const QVector3D &p : node.outline) {
    front.push_back(p + half_offset);
    back.push_back(p - half_offset);
  }

  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;
  append_closed_prism(vertices, indices, front, back);
  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

auto build_cone_mesh(const ConeNode &node)
    -> std::unique_ptr<Render::GL::Mesh> {
  return build_path_tube_mesh(node.base_center, node.tip, node.base_radius,
                              std::max(node.base_radius * 0.05F, 0.002F), 0.0F,
                              2U, node.ring_vertices);
}

auto build_tube_mesh(const TubeNode &node)
    -> std::unique_ptr<Render::GL::Mesh> {
  return build_path_tube_mesh(node.start, node.end, node.start_radius,
                              node.end_radius, node.sag, node.segment_count,
                              node.ring_vertices);
}

auto compile_node_mesh(const MeshNodeData &data)
    -> std::unique_ptr<Render::GL::Mesh> {
  return std::visit(
      [](const auto &node) -> std::unique_ptr<Render::GL::Mesh> {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, BarrelNode>) {
          return build_barrel_mesh(node);
        } else if constexpr (std::is_same_v<T, EllipsoidNode>) {
          return build_ellipsoid_mesh(node);
        } else if constexpr (std::is_same_v<T, ColumnLegNode>) {
          return build_column_leg_mesh(node);
        } else if constexpr (std::is_same_v<T, SnoutNode>) {
          return build_snout_mesh(node);
        } else if constexpr (std::is_same_v<T, FlatFanNode>) {
          return build_flat_fan_mesh(node);
        } else if constexpr (std::is_same_v<T, ConeNode>) {
          return build_cone_mesh(node);
        } else {
          return build_tube_mesh(node);
        }
      },
      data);
}

} // namespace

auto compile_mesh_graph(std::span<const MeshNode> nodes) -> CompiledMeshGraph {
  CompiledMeshGraph compiled;
  compiled.meshes.reserve(nodes.size());
  compiled.primitives.reserve(nodes.size());

  for (const MeshNode &node : nodes) {
    std::unique_ptr<Render::GL::Mesh> mesh = compile_node_mesh(node.data);
    if (mesh == nullptr || mesh->get_indices().empty()) {
      continue;
    }

    compiled.debug_names.emplace_back(node.debug_name);

    PrimitiveInstance primitive{};
    primitive.debug_name = compiled.debug_names.back();
    primitive.shape = PrimitiveShape::Mesh;
    primitive.params.anchor_bone = node.anchor_bone;
    primitive.custom_mesh = mesh.get();
    primitive.mesh_skinning = MeshSkinning::Rigid;
    primitive.color_role = node.color_role;
    primitive.material_id = node.material_id;
    primitive.lod_mask = node.lod_mask;

    compiled.meshes.push_back(std::move(mesh));
    compiled.primitives.push_back(primitive);
  }

  return compiled;
}

auto compile_combined_mesh_graph(std::span<const MeshNode> nodes)
    -> std::unique_ptr<Render::GL::Mesh> {
  std::vector<Render::GL::Vertex> vertices;
  std::vector<unsigned int> indices;

  for (const MeshNode &node : nodes) {
    std::unique_ptr<Render::GL::Mesh> mesh = compile_node_mesh(node.data);
    if (mesh == nullptr) {
      continue;
    }
    unsigned int const base = static_cast<unsigned int>(vertices.size());
    auto const &mesh_vertices = mesh->get_vertices();
    auto const &mesh_indices = mesh->get_indices();
    for (auto vertex : mesh_vertices) {
      vertex.color_role = node.color_role;
      vertices.push_back(vertex);
    }
    for (unsigned int idx : mesh_indices) {
      indices.push_back(base + idx);
    }
  }

  return std::make_unique<Render::GL::Mesh>(vertices, indices);
}

} // namespace Render::Creature::Quadruped
