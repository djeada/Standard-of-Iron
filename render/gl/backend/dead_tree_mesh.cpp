#include "dead_tree_mesh.h"

#include <cmath>
#include <iterator>

namespace {

using Render::GL::BackendPipelines::PropMeshData;
using Render::GL::BackendPipelines::PropMeshVertex;
using V3 = QVector3D;

constexpr float k_tau = 6.28318530F;
constexpr int k_trunk_sides = 14;

constexpr float k_butt_x = -1.12F;
constexpr float k_tip_x = 1.12F;
constexpr float k_butt_radius = 0.255F;
constexpr float k_tip_radius = 0.140F;
constexpr float k_sink_ratio = 0.76F;
constexpr float k_bow_amplitude = 0.07F;

auto axis_t(float x) -> float {
  return (x - k_butt_x) / (k_tip_x - k_butt_x);
}

auto trunk_radius(float x) -> float {
  return k_butt_radius + (k_tip_radius - k_butt_radius) * axis_t(x);
}

auto trunk_center_y(float x) -> float {
  return trunk_radius(x) * k_sink_ratio;
}

auto trunk_center_z(float x) -> float {
  float const normalized = x / k_tip_x;
  return k_bow_amplitude * (1.0F - normalized * normalized) - k_bow_amplitude * 0.5F;
}

auto bark_jitter(int station, int side) -> float {
  uint32_t hash = static_cast<uint32_t>(station) * 0x9E3779B9U ^
                  static_cast<uint32_t>(side) * 0x85EBCA6BU;
  hash ^= hash >> 15;
  hash *= 0xC2B2AE35U;
  hash ^= hash >> 13;
  return static_cast<float>(hash & 0xFFFFU) / 65535.0F;
}

void append_quad(std::vector<PropMeshVertex>& verts,
                 std::vector<uint16_t>& idx,
                 const V3& p0,
                 const V3& p1,
                 const V3& p2,
                 const V3& p3,
                 const V3& n) {
  auto const base = static_cast<uint16_t>(verts.size());
  verts.insert(verts.end(),
               {PropMeshVertex{p0, n},
                PropMeshVertex{p1, n},
                PropMeshVertex{p2, n},
                PropMeshVertex{p3, n}});
  idx.insert(idx.end(),
             {base,
              uint16_t(base + 1),
              uint16_t(base + 2),
              base,
              uint16_t(base + 2),
              uint16_t(base + 3)});
}

void append_box(std::vector<PropMeshVertex>& verts,
                std::vector<uint16_t>& idx,
                const V3& lo,
                const V3& hi) {
  const float x0 = lo.x();
  const float y0 = lo.y();
  const float z0 = lo.z();
  const float x1 = hi.x();
  const float y1 = hi.y();
  const float z1 = hi.z();

  append_quad(
      verts, idx, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, {0, 0, 1});
  append_quad(
      verts, idx, {x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {0, 0, -1});
  append_quad(
      verts, idx, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, {-1, 0, 0});
  append_quad(
      verts, idx, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {1, 0, 0});
  append_quad(
      verts, idx, {x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {0, 1, 0});
  append_quad(
      verts, idx, {x0, y0, z1}, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {0, -1, 0});
}

void append_oriented_box(std::vector<PropMeshVertex>& verts,
                         std::vector<uint16_t>& idx,
                         const V3& a,
                         const V3& b,
                         float half_width,
                         float half_depth) {
  V3 dir = b - a;
  if (dir.lengthSquared() < 1.0e-8F) {
    dir = {0.0F, 1.0F, 0.0F};
  } else {
    dir.normalize();
  }

  V3 const reference =
      std::abs(dir.y()) < 0.9F ? V3(0.0F, 1.0F, 0.0F) : V3(1.0F, 0.0F, 0.0F);
  V3 side = QVector3D::crossProduct(reference, dir);
  if (side.lengthSquared() < 1.0e-8F) {
    side = {1.0F, 0.0F, 0.0F};
  } else {
    side.normalize();
  }
  V3 depth = QVector3D::crossProduct(dir, side);
  if (depth.lengthSquared() < 1.0e-8F) {
    depth = {0.0F, 0.0F, 1.0F};
  } else {
    depth.normalize();
  }

  side *= half_width;
  depth *= half_depth;

  V3 const a0 = a - side - depth;
  V3 const a1 = a + side - depth;
  V3 const a2 = a + side + depth;
  V3 const a3 = a - side + depth;
  V3 const b0 = b - side - depth;
  V3 const b1 = b + side - depth;
  V3 const b2 = b + side + depth;
  V3 const b3 = b - side + depth;

  auto face = [&](const V3& p0, const V3& p1, const V3& p2, const V3& p3) {
    V3 n = QVector3D::crossProduct(p1 - p0, p3 - p0);
    if (n.lengthSquared() > 1.0e-8F) {
      n.normalize();
    } else {
      n = {0.0F, 1.0F, 0.0F};
    }
    append_quad(verts, idx, p0, p1, p2, p3, n);
  };

  face(a0, b0, b1, a1);
  face(a3, a2, b2, b3);
  face(a1, b1, b2, a2);
  face(a0, a3, b3, b0);
  face(a0, a1, a2, a3);
  face(b0, b3, b2, b1);
}

auto make_ring(float x, int station) -> std::vector<V3> {
  float const center_y = trunk_center_y(x);
  float const center_z = trunk_center_z(x);
  float const radius = trunk_radius(x);
  float const phase = 0.18F + 0.34F * axis_t(x);

  std::vector<V3> ring;
  ring.reserve(k_trunk_sides);
  for (int i = 0; i < k_trunk_sides; ++i) {
    float const angle =
        phase + k_tau * static_cast<float>(i) / static_cast<float>(k_trunk_sides);
    float const wobble = 0.92F + 0.15F * bark_jitter(station, i);
    ring.emplace_back(x,
                      center_y + radius * wobble * std::sin(angle),
                      center_z + radius * wobble * 1.04F * std::cos(angle));
  }
  return ring;
}

void append_splintered_break(std::vector<PropMeshVertex>& verts,
                             std::vector<uint16_t>& idx,
                             const std::vector<V3>& ring,
                             float base_x) {
  float const center_y = trunk_center_y(base_x);
  float const center_z = trunk_center_z(base_x);
  int const count = static_cast<int>(ring.size());

  auto tri = [&](const V3& p0, const V3& p1, const V3& p2) {
    V3 n = QVector3D::crossProduct(p1 - p0, p2 - p0);
    if (n.lengthSquared() > 1.0e-8F) {
      n.normalize();
    } else {
      n = {1.0F, 0.0F, 0.0F};
    }
    auto const base = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {PropMeshVertex{p0, n}, PropMeshVertex{p1, n}, PropMeshVertex{p2, n}});
    idx.insert(idx.end(), {base, uint16_t(base + 1), uint16_t(base + 2)});
  };

  std::vector<V3> shards;
  shards.reserve(count);
  for (int i = 0; i < count; ++i) {
    float const reach = 0.02F + 0.20F * bark_jitter(91, i);
    float const pull = 0.38F + 0.26F * bark_jitter(92, i);
    shards.emplace_back(base_x + reach,
                        center_y + (ring[i].y() - center_y) * pull,
                        center_z + (ring[i].z() - center_z) * pull);
  }

  V3 const apex(base_x + 0.055F, center_y, center_z);
  for (int i = 0; i < count; ++i) {
    int const next = (i + 1) % count;
    tri(ring[i], shards[i], ring[next]);
    tri(ring[next], shards[i], shards[next]);
    tri(shards[i], apex, shards[next]);
  }
}

void connect_rings(std::vector<PropMeshVertex>& verts,
                   std::vector<uint16_t>& idx,
                   const std::vector<V3>& a,
                   const std::vector<V3>& b) {
  int const count = static_cast<int>(a.size());
  for (int i = 0; i < count; ++i) {
    int const next = (i + 1) % count;
    V3 n = QVector3D::crossProduct(b[i] - a[i], a[next] - a[i]);
    if (n.lengthSquared() > 1.0e-8F) {
      n.normalize();
    } else {
      n = {0.0F, 1.0F, 0.0F};
    }
    append_quad(verts, idx, a[i], b[i], b[next], a[next], n);
  }
}

void append_flat_cap(std::vector<PropMeshVertex>& verts,
                     std::vector<uint16_t>& idx,
                     const std::vector<V3>& ring,
                     float center_y,
                     float center_z,
                     bool negative_x) {
  V3 const normal(negative_x ? -1.0F : 1.0F, 0.0F, 0.0F);
  V3 const center(ring.front().x(), center_y, center_z);
  int const count = static_cast<int>(ring.size());
  for (int i = 0; i < count; ++i) {
    int const next = (i + 1) % count;
    auto const base = static_cast<uint16_t>(verts.size());
    if (negative_x) {
      verts.insert(verts.end(),
                   {PropMeshVertex{center, normal},
                    PropMeshVertex{ring[i], normal},
                    PropMeshVertex{ring[next], normal}});
    } else {
      verts.insert(verts.end(),
                   {PropMeshVertex{center, normal},
                    PropMeshVertex{ring[next], normal},
                    PropMeshVertex{ring[i], normal}});
    }
    idx.insert(idx.end(), {base, uint16_t(base + 1), uint16_t(base + 2)});
  }
}

} // namespace

namespace Render::GL::BackendPipelines {

auto build_dead_tree_mesh() -> PropMeshData {
  PropMeshData mesh;
  auto& verts = mesh.vertices;
  auto& idx = mesh.indices;

  constexpr float station_x[] = {
      k_butt_x, -0.75F, -0.38F, 0.00F, 0.38F, 0.75F, k_tip_x};
  constexpr int station_count = static_cast<int>(std::size(station_x));

  std::vector<std::vector<V3>> rings;
  rings.reserve(station_count);
  for (int station = 0; station < station_count; ++station) {
    rings.push_back(make_ring(station_x[station], station));
  }
  for (int station = 0; station + 1 < station_count; ++station) {
    connect_rings(verts, idx, rings[station], rings[station + 1]);
  }

  append_flat_cap(
      verts, idx, rings.front(), trunk_center_y(k_butt_x), trunk_center_z(k_butt_x), true);
  append_splintered_break(verts, idx, rings.back(), k_tip_x);

  append_oriented_box(
      verts, idx, {-0.62F, 0.24F, 0.15F}, {-0.86F, 0.46F, 0.26F}, 0.038F, 0.030F);
  append_oriented_box(
      verts, idx, {0.02F, 0.20F, -0.18F}, {0.24F, 0.44F, -0.29F}, 0.040F, 0.031F);
  append_oriented_box(
      verts, idx, {0.64F, 0.15F, 0.13F}, {0.88F, 0.31F, 0.23F}, 0.031F, 0.026F);

  append_box(verts, idx, {-0.48F, 0.01F, -0.34F}, {-0.24F, 0.05F, -0.24F});
  append_box(verts, idx, {0.20F, 0.00F, 0.24F}, {0.42F, 0.04F, 0.33F});

  return mesh;
}

} // namespace Render::GL::BackendPipelines
