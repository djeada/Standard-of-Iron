#include "render/geom/mesh.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

namespace Render::GL {

namespace {

auto next_mesh_id() -> std::uint64_t {
  static std::atomic<std::uint64_t> next{1};
  return next.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

Mesh::Mesh(const std::vector<Vertex>& vertices,
           const std::vector<unsigned int>& indices)
    : m_vertices(vertices)
    , m_indices(indices)
    , m_id(next_mesh_id())
    , m_lifetime(std::make_shared<const int>(0)) {
  if (!m_vertices.empty()) {
    QVector3D min_pos(std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max());
    QVector3D max_pos(std::numeric_limits<float>::lowest(),
                      std::numeric_limits<float>::lowest(),
                      std::numeric_limits<float>::lowest());
    for (const auto& vertex : m_vertices) {
      const QVector3D position(
          vertex.position[0], vertex.position[1], vertex.position[2]);
      min_pos.setX(std::min(min_pos.x(), position.x()));
      min_pos.setY(std::min(min_pos.y(), position.y()));
      min_pos.setZ(std::min(min_pos.z(), position.z()));
      max_pos.setX(std::max(max_pos.x(), position.x()));
      max_pos.setY(std::max(max_pos.y(), position.y()));
      max_pos.setZ(std::max(max_pos.z(), position.z()));
    }
    m_bounds_center = (min_pos + max_pos) * 0.5F;
    for (const auto& vertex : m_vertices) {
      const QVector3D position(
          vertex.position[0], vertex.position[1], vertex.position[2]);
      m_bounds_radius =
          std::max(m_bounds_radius, (position - m_bounds_center).length());
    }
  }
}

Mesh::~Mesh() = default;

auto create_quad_mesh() -> std::unique_ptr<Mesh> {
  std::vector<Vertex> const vertices = {

      {{-1.0F, -1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F}},
      {{1.0F, -1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 0.0F}},
      {{1.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F}},
      {{-1.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F}}};

  std::vector<unsigned int> const indices = {0, 1, 2, 2, 3, 0};

  return std::make_unique<Mesh>(vertices, indices);
}

auto create_cube_mesh() -> std::unique_ptr<Mesh> {

  struct Face {
    QVector3D normal;
    std::array<QVector3D, 4> corners;
  };
  const std::array<Face, 6> faces{{
      {{0.0F, 0.0F, 1.0F},
       {{{-1.0F, -1.0F, 1.0F},
         {1.0F, -1.0F, 1.0F},
         {1.0F, 1.0F, 1.0F},
         {-1.0F, 1.0F, 1.0F}}}},
      {{0.0F, 0.0F, -1.0F},
       {{{1.0F, -1.0F, -1.0F},
         {-1.0F, -1.0F, -1.0F},
         {-1.0F, 1.0F, -1.0F},
         {1.0F, 1.0F, -1.0F}}}},
      {{1.0F, 0.0F, 0.0F},
       {{{1.0F, -1.0F, 1.0F},
         {1.0F, -1.0F, -1.0F},
         {1.0F, 1.0F, -1.0F},
         {1.0F, 1.0F, 1.0F}}}},
      {{-1.0F, 0.0F, 0.0F},
       {{{-1.0F, -1.0F, -1.0F},
         {-1.0F, -1.0F, 1.0F},
         {-1.0F, 1.0F, 1.0F},
         {-1.0F, 1.0F, -1.0F}}}},
      {{0.0F, 1.0F, 0.0F},
       {{{-1.0F, 1.0F, 1.0F},
         {1.0F, 1.0F, 1.0F},
         {1.0F, 1.0F, -1.0F},
         {-1.0F, 1.0F, -1.0F}}}},
      {{0.0F, -1.0F, 0.0F},
       {{{-1.0F, -1.0F, -1.0F},
         {1.0F, -1.0F, -1.0F},
         {1.0F, -1.0F, 1.0F},
         {-1.0F, -1.0F, 1.0F}}}},
  }};
  constexpr std::array<std::array<float, 2>, 4> k_uv{
      {{0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F}}};

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(24);
  indices.reserve(36);
  for (const Face& face : faces) {
    auto const base = static_cast<unsigned int>(vertices.size());
    for (std::size_t corner = 0; corner < face.corners.size(); ++corner) {
      const QVector3D& p = face.corners[corner];
      vertices.push_back({{p.x(), p.y(), p.z()},
                          {face.normal.x(), face.normal.y(), face.normal.z()},
                          {k_uv[corner][0], k_uv[corner][1]}});
    }
    for (unsigned int const offset : {0U, 1U, 2U, 2U, 3U, 0U}) {
      indices.push_back(base + offset);
    }
  }

  return std::make_unique<Mesh>(vertices, indices);
}

auto create_plane_mesh(float width,
                       float height,
                       int subdivisions) -> std::unique_ptr<Mesh> {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;

  float const half_width = width * 0.5F;
  float const half_height = height * 0.5F;

  for (int z = 0; z <= subdivisions; ++z) {
    for (int x = 0; x <= subdivisions; ++x) {
      float x_pos = (x / static_cast<float>(subdivisions)) * width - half_width;
      float z_pos = (z / static_cast<float>(subdivisions)) * height - half_height;

      vertices.push_back({{x_pos, 0.0F, z_pos},
                          {0.0F, 1.0F, 0.0F},
                          {x / static_cast<float>(subdivisions),
                           z / static_cast<float>(subdivisions)}});
    }
  }

  for (int z = 0; z < subdivisions; ++z) {
    for (int x = 0; x < subdivisions; ++x) {
      int top_left = z * (subdivisions + 1) + x;
      int top_right = top_left + 1;
      int bottom_left = (z + 1) * (subdivisions + 1) + x;
      int bottom_right = bottom_left + 1;

      indices.push_back(top_left);
      indices.push_back(bottom_left);
      indices.push_back(top_right);

      indices.push_back(top_right);
      indices.push_back(bottom_left);
      indices.push_back(bottom_right);
    }
  }

  return std::make_unique<Mesh>(vertices, indices);
}

} // namespace Render::GL
