#include "dead_tree_mesh.h"

#include <cmath>

namespace {

using Render::GL::BackendPipelines::PropMeshData;
using Render::GL::BackendPipelines::PropMeshVertex;
using V3 = QVector3D;

constexpr float k_tau = 6.28318530F;
constexpr int k_trunk_sides = 10;

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
  V3 axis = b - a;
  V3 side(-axis.y(), axis.x(), 0.0F);
  if (side.lengthSquared() < 1.0e-8F) {
    side = {1.0F, 0.0F, 0.0F};
  } else {
    side.normalize();
  }
  side *= half_width;
  V3 const depth(0.0F, 0.0F, half_depth);

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

auto make_ring(float x,
               float center_y,
               float center_z,
               float radius_y,
               float radius_z,
               float phase) -> std::vector<V3> {
  std::vector<V3> ring;
  ring.reserve(k_trunk_sides);
  for (int i = 0; i < k_trunk_sides; ++i) {
    float const angle =
        phase + k_tau * static_cast<float>(i) / static_cast<float>(k_trunk_sides);
    ring.emplace_back(x,
                      center_y + radius_y * std::sin(angle),
                      center_z + radius_z * std::cos(angle));
  }
  return ring;
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

  constexpr float trunk_phase = 0.18F;

  const auto ring_a = make_ring(-1.12F, 0.18F, -0.02F, 0.18F, 0.19F, trunk_phase);
  const auto ring_b = make_ring(-0.74F, 0.17F, -0.01F, 0.20F, 0.21F, trunk_phase);
  const auto ring_c = make_ring(-0.36F, 0.16F, -0.01F, 0.21F, 0.22F, trunk_phase);
  const auto ring_d = make_ring(0.00F, 0.15F, 0.00F, 0.22F, 0.23F, trunk_phase);
  const auto ring_e = make_ring(0.38F, 0.15F, 0.01F, 0.21F, 0.22F, trunk_phase);
  const auto ring_f = make_ring(0.76F, 0.16F, 0.01F, 0.20F, 0.21F, trunk_phase);
  const auto ring_g = make_ring(1.12F, 0.17F, 0.02F, 0.18F, 0.19F, trunk_phase);

  connect_rings(verts, idx, ring_a, ring_b);
  connect_rings(verts, idx, ring_b, ring_c);
  connect_rings(verts, idx, ring_c, ring_d);
  connect_rings(verts, idx, ring_d, ring_e);
  connect_rings(verts, idx, ring_e, ring_f);
  connect_rings(verts, idx, ring_f, ring_g);

  append_flat_cap(verts, idx, ring_a, 0.18F, -0.02F, true);
  append_flat_cap(verts, idx, ring_g, 0.17F, 0.02F, false);

  append_oriented_box(
      verts, idx, {-0.62F, 0.21F, 0.15F}, {-0.86F, 0.42F, 0.25F}, 0.038F, 0.030F);
  append_oriented_box(
      verts, idx, {0.02F, 0.22F, -0.18F}, {0.24F, 0.48F, -0.28F}, 0.042F, 0.032F);
  append_oriented_box(
      verts, idx, {0.64F, 0.22F, 0.14F}, {0.88F, 0.38F, 0.23F}, 0.034F, 0.028F);

  // A charred, antler-like crown grows out of the fallen trunk. Its tall forked
  // profile gives this authored prop the cursed-wood silhouette its name
  // promises, while the horizontal trunk still anchors it to the terrain.
  append_oriented_box(
      verts, idx, {-0.10F, 0.20F, 0.02F}, {-0.06F, 0.92F, 0.00F}, 0.095F, 0.085F);
  append_oriented_box(
      verts, idx, {-0.06F, 0.86F, 0.00F}, {-0.30F, 1.42F, -0.06F}, 0.070F, 0.060F);
  append_oriented_box(
      verts, idx, {-0.30F, 1.38F, -0.06F}, {-0.54F, 1.72F, -0.10F}, 0.045F, 0.042F);
  append_oriented_box(
      verts, idx, {-0.03F, 0.82F, 0.02F}, {0.30F, 1.26F, 0.10F}, 0.068F, 0.058F);
  append_oriented_box(
      verts, idx, {0.30F, 1.22F, 0.10F}, {0.56F, 1.55F, 0.18F}, 0.042F, 0.040F);
  append_oriented_box(
      verts, idx, {-0.18F, 0.66F, -0.02F}, {-0.58F, 0.94F, 0.18F}, 0.050F, 0.045F);
  append_oriented_box(
      verts, idx, {0.16F, 0.62F, 0.04F}, {0.58F, 0.88F, -0.18F}, 0.048F, 0.043F);

  append_box(verts, idx, {-0.48F, 0.01F, -0.34F}, {-0.24F, 0.05F, -0.24F});
  append_box(verts, idx, {0.20F, 0.00F, 0.24F}, {0.42F, 0.04F, 0.33F});

  return mesh;
}

} // namespace Render::GL::BackendPipelines
