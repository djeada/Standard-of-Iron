#include <QDebug>
#include <QOpenGLContext>
#include <QString>
#include <qglobal.h>
#include <qopenglcontext.h>
#include <qopenglext.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

#include "render/backend/dead_tree_mesh.h"
#include "render/backend/iron_ore_mesh.h"
#include "render/backend/mesh_buffers.h"
#include "render/backend/prop_mesh_builder.h"
#include "render/backend/ring_loft_builder.h"
#include "render/backend/static_mesh_upload.h"
#include "render/backend/tree_mesh.h"
#include "render/backend/vegetation_pipeline.h"
#include "render/gl/platform_gl.h"
#include "render/gl/render_constants.h"
#include "render/gl/shader_cache.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::Geometry;

constexpr GLuint k_foliage_position_location = 0;
constexpr GLuint k_foliage_tex_coord_location = 1;
constexpr GLuint k_foliage_normal_location = 2;

constexpr std::array<GLuint, 3> k_foliage_instance_locations{3, 4, 5};

constexpr std::array<GLuint, 3> k_stone_instance_locations{2, 3, 4};

void VegetationPipeline::initialize_stone_pipeline() {
  initializeOpenGLFunctions();
  m_stone_mesh.reset();

  struct StoneVertex {
    QVector3D position;
    QVector3D normal;
  };

  using F = StoneVertex;
  std::vector<F> verts;
  std::vector<uint16_t> idx;

  constexpr int k_n = 12;
  constexpr float k_tau = 6.28318530F;
  constexpr int k_rings = 7;

  constexpr float k_radial_jitter = 0.11F;
  constexpr float k_y_jitter = 0.030F;

  constexpr float k_apex_offset_x = -0.16F;
  constexpr float k_apex_height = 0.70F;
  constexpr float k_apex_offset_z = 0.06F;

  auto jitter = [](int ring, int vertex, int salt) -> float {
    uint32_t hash = static_cast<uint32_t>(ring + 1) * 0x9E3779B9U ^
                    static_cast<uint32_t>(vertex + 1) * 0x85EBCA6BU ^
                    static_cast<uint32_t>(salt + 1) * 0xC2B2AE35U;
    hash ^= hash >> 15;
    hash *= 0x27D4EB2FU;
    hash ^= hash >> 13;
    return static_cast<float>(hash & 0xFFFFU) / 65535.0F;
  };

  struct Ring {
    float y;
    float radius;
    float phase;
    float sx;
    float sz;
    float cx;
    float cz;
  };
  const Ring rings[] = {
      {-0.07F, 0.37F, 0.08F, 1.10F, 1.01F, 0.00F, 0.00F},
      {0.05F, 0.45F, 0.05F, 1.11F, 0.99F, 0.01F, 0.00F},
      {0.17F, 0.48F, 0.03F, 1.13F, 0.98F, 0.00F, 0.01F},
      {0.28F, 0.46F, 0.04F, 1.12F, 0.99F, -0.03F, 0.02F},
      {0.42F, 0.40F, 0.06F, 1.09F, 1.01F, -0.07F, 0.04F},
      {0.52F, 0.34F, 0.06F, 1.11F, 0.94F, -0.10F, 0.05F},
      {0.59F, 0.28F, 0.07F, 1.09F, 0.89F, -0.11F, 0.06F},
  };
  static_assert(std::size(rings) == static_cast<std::size_t>(k_rings));

  QVector3D ring_pts[k_rings][k_n];
  for (int ri = 0; ri < k_rings; ++ri) {
    const Ring& r = rings[ri];
    for (int i = 0; i < k_n; ++i) {
      float const t = static_cast<float>(i) / k_n;
      float const angle_jitter = (jitter(ri, i, 3) - 0.5F) * (k_tau / k_n) * 0.55F;
      float const angle = t * k_tau + r.phase + angle_jitter;

      float const perturb = 1.0F + k_radial_jitter * (jitter(ri, i, 1) - 0.5F) * 2.0F;
      float const rx = r.radius * perturb * r.sx * std::cos(angle);
      float const rz = r.radius * perturb * r.sz * std::sin(angle);

      // The upper ledge dips towards one broken face.
      float const crown_dip = ri >= 4 ? 0.24F * rx : 0.0F;
      float const ry = r.y - crown_dip + k_y_jitter * (jitter(ri, i, 2) - 0.5F) * 2.0F;
      ring_pts[ri][i] = QVector3D(rx + r.cx, ry, rz + r.cz);
    }
  }

  auto emit_tri = [&](const QVector3D& a, const QVector3D& b, const QVector3D& c) {
    QVector3D n = QVector3D::crossProduct(c - a, b - a);
    if (n.lengthSquared() > 1.0e-8F) {
      n.normalize();
    } else {
      n = QVector3D(0.0F, 1.0F, 0.0F);
    }
    auto base = static_cast<uint16_t>(verts.size());
    verts.push_back({a, n});
    verts.push_back({b, n});
    verts.push_back({c, n});
    idx.push_back(base);
    idx.push_back(uint16_t(base + 2));
    idx.push_back(uint16_t(base + 1));
  };

  auto emit_quad = [&](const QVector3D& a,
                       const QVector3D& b,
                       const QVector3D& c,
                       const QVector3D& d) {
    if ((c - a).lengthSquared() <= (d - b).lengthSquared()) {
      emit_tri(a, b, c);
      emit_tri(a, c, d);
    } else {
      emit_tri(a, b, d);
      emit_tri(b, c, d);
    }
  };

  for (int ri = 0; ri < k_rings - 1; ++ri) {
    for (int i = 0; i < k_n; ++i) {
      int const next = (i + 1) % k_n;

      const QVector3D& a = ring_pts[ri][i];
      const QVector3D& b = ring_pts[ri][next];
      const QVector3D& c = ring_pts[ri + 1][next];
      const QVector3D& d = ring_pts[ri + 1][i];
      emit_quad(a, b, c, d);
    }
  }

  QVector3D const apex(k_apex_offset_x, k_apex_height, k_apex_offset_z);
  for (int i = 0; i < k_n; ++i) {
    int const next = (i + 1) % k_n;
    emit_tri(ring_pts[k_rings - 1][i], ring_pts[k_rings - 1][next], apex);
  }

  QVector3D const bot_center(0.0F, -0.09F, 0.0F);
  for (int i = 0; i < k_n; ++i) {
    int const next = (i + 1) % k_n;

    emit_tri(ring_pts[0][next], ring_pts[0][i], bot_center);
  }

  constexpr std::array<VertexAttributeLayout, 2> k_stone_attributes{{
      {k_foliage_position_location, vec3, offsetof(StoneVertex, position)},
      {1, vec3, offsetof(StoneVertex, normal)},
  }};
  upload_static_instanced_mesh(*this,
                               m_stone_mesh,
                               verts.data(),
                               verts.size(),
                               sizeof(StoneVertex),
                               k_stone_attributes,
                               idx.data(),
                               idx.size(),
                               k_stone_instance_locations);
}

void VegetationPipeline::initialize_plant_pipeline() {
  initializeOpenGLFunctions();
  m_plant_mesh.reset();

  struct PlantVertex {
    QVector3D position;
    QVector2D tex_coord;
    QVector3D normal;
  };

  constexpr float k_c60 = 0.2500F;
  constexpr float k_s60 = 0.4330F;
  const PlantVertex plant_vertices[] = {

      {{-0.5F, 0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, 0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, 1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}},
      {{-0.5F, 1.0F, 0.0F}, {0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, 0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}},
      {{-0.5F, 0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}},
      {{-0.5F, 1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 0.0F, -1.0F}},
      {{0.5F, 1.0F, 0.0F}, {0.0F, 1.0F}, {0.0F, 0.0F, -1.0F}},

      {{-k_c60, 0.0F, -k_s60}, {0.0F, 0.0F}, {-0.8660F, 0.0F, 0.5F}},
      {{k_c60, 0.0F, k_s60}, {1.0F, 0.0F}, {-0.8660F, 0.0F, 0.5F}},
      {{k_c60, 1.0F, k_s60}, {1.0F, 1.0F}, {-0.8660F, 0.0F, 0.5F}},
      {{-k_c60, 1.0F, -k_s60}, {0.0F, 1.0F}, {-0.8660F, 0.0F, 0.5F}},
      {{k_c60, 0.0F, k_s60}, {0.0F, 0.0F}, {0.8660F, 0.0F, -0.5F}},
      {{-k_c60, 0.0F, -k_s60}, {1.0F, 0.0F}, {0.8660F, 0.0F, -0.5F}},
      {{-k_c60, 1.0F, -k_s60}, {1.0F, 1.0F}, {0.8660F, 0.0F, -0.5F}},
      {{k_c60, 1.0F, k_s60}, {0.0F, 1.0F}, {0.8660F, 0.0F, -0.5F}},

      {{k_c60, 0.0F, -k_s60}, {0.0F, 0.0F}, {-0.8660F, 0.0F, -0.5F}},
      {{-k_c60, 0.0F, k_s60}, {1.0F, 0.0F}, {-0.8660F, 0.0F, -0.5F}},
      {{-k_c60, 1.0F, k_s60}, {1.0F, 1.0F}, {-0.8660F, 0.0F, -0.5F}},
      {{k_c60, 1.0F, -k_s60}, {0.0F, 1.0F}, {-0.8660F, 0.0F, -0.5F}},
      {{-k_c60, 0.0F, k_s60}, {0.0F, 0.0F}, {0.8660F, 0.0F, 0.5F}},
      {{k_c60, 0.0F, -k_s60}, {1.0F, 0.0F}, {0.8660F, 0.0F, 0.5F}},
      {{k_c60, 1.0F, -k_s60}, {1.0F, 1.0F}, {0.8660F, 0.0F, 0.5F}},
      {{-k_c60, 1.0F, k_s60}, {0.0F, 1.0F}, {0.8660F, 0.0F, 0.5F}},
  };

  const unsigned short plant_indices[] = {
      0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
      12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
  };

  constexpr std::array<VertexAttributeLayout, 3> k_plant_attributes{{
      {k_foliage_position_location, vec3, offsetof(PlantVertex, position)},
      {k_foliage_tex_coord_location, vec2, offsetof(PlantVertex, tex_coord)},
      {k_foliage_normal_location, vec3, offsetof(PlantVertex, normal)},
  }};
  upload_static_instanced_mesh(*this,
                               m_plant_mesh,
                               static_cast<const void*>(plant_vertices),
                               plant_cross_quad_vertex_count,
                               sizeof(PlantVertex),
                               k_plant_attributes,
                               static_cast<const void*>(plant_indices),
                               plant_cross_quad_index_count,
                               k_foliage_instance_locations);
}

namespace {

void upload_tree_mesh(QOpenGLFunctions_3_3_Core& gl,
                      StaticMeshBuffers& target,
                      const TreeMesh& tree) {
  constexpr std::array<VertexAttributeLayout, 3> k_attributes{{
      {k_foliage_position_location, vec3, offsetof(RingVertex, position)},
      {k_foliage_tex_coord_location, vec3, offsetof(RingVertex, u)},
      {k_foliage_normal_location, vec3, offsetof(RingVertex, normal)},
  }};
  static_assert(offsetof(RingVertex, v) == offsetof(RingVertex, u) + sizeof(float));
  static_assert(offsetof(RingVertex, weight) ==
                offsetof(RingVertex, v) + sizeof(float));
  upload_static_instanced_mesh(gl,
                               target,
                               tree.vertices.data(),
                               tree.vertices.size(),
                               sizeof(RingVertex),
                               k_attributes,
                               tree.indices.data(),
                               tree.indices.size(),
                               k_foliage_instance_locations);
}

} // namespace

void VegetationPipeline::initialize_pine_pipeline() {
  initializeOpenGLFunctions();
  m_pine_mesh.reset();

  upload_tree_mesh(*this, m_pine_mesh, build_pine_tree_mesh());
}

void VegetationPipeline::initialize_olive_pipeline() {
  initializeOpenGLFunctions();
  m_olive_mesh.reset();

  upload_tree_mesh(*this, m_olive_mesh, build_olive_tree_mesh());
}

void VegetationPipeline::initialize_dead_tree_pipeline() {
  initializeOpenGLFunctions();
  m_dead_tree_mesh.reset();

  const auto mesh = build_dead_tree_mesh();

  upload_prop_mesh_impl(mesh.vertices, mesh.indices, m_dead_tree_mesh);
}

void VegetationPipeline::initialize_iron_ore_pipeline() {
  initializeOpenGLFunctions();
  m_iron_ore_mesh.reset();

  PropMeshData mesh = build_iron_ore_mesh();
  upload_prop_mesh_impl(mesh.vertices, mesh.indices, m_iron_ore_mesh);
}

void VegetationPipeline::initialize_cypress_pipeline() {
  initializeOpenGLFunctions();
  m_cypress_mesh.reset();

  upload_tree_mesh(*this, m_cypress_mesh, build_cypress_tree_mesh());
}

void VegetationPipeline::initialize_palm_pipeline() {
  initializeOpenGLFunctions();
  m_palm_mesh.reset();

  struct PalmVertex {
    QVector3D position;
    QVector3D tex_coord;
    QVector3D normal;
  };

  constexpr int k_trunk_segments = olive_tree_segments;
  constexpr float k_pi = 3.14159265F;
  constexpr float k_two_pi = 6.28318530718F;
  constexpr int k_frond_count = 12;
  constexpr int k_frond_stations = 8;
  constexpr int k_dead_frond_count = 4;

  constexpr float k_leaf_v = 0.60F;
  constexpr float k_dead_v = 0.30F;

  constexpr float k_fibre_material = 0.5F;
  constexpr float k_dead_frond_material = 1.0F;
  constexpr float k_live_frond_material = 2.0F;
  constexpr float k_frond_edge_material = 0.5F;

  RingLoftBuilder loft(k_trunk_segments);
  loft.reserve(16);

  struct TrunkRing {
    float radius;
    float y;
    float normal_up;
    float v;
    QVector2D offset;
  };
  constexpr std::array<TrunkRing, 9> k_trunk{{
      {0.092F, -0.015F, -0.34F, 0.04F, QVector2D(0.0F, 0.0F)},
      {0.088F, 0.22F, -0.04F, 0.10F, QVector2D(0.010F, 0.004F)},
      {0.076F, 0.46F, 0.02F, 0.16F, QVector2D(0.026F, 0.011F)},
      {0.066F, 0.68F, 0.06F, 0.22F, QVector2D(0.044F, 0.018F)},
      {0.058F, 0.86F, 0.10F, 0.28F, QVector2D(0.058F, 0.023F)},
      {0.052F, 0.98F, 0.14F, 0.34F, QVector2D(0.066F, 0.026F)},
      {0.048F, 1.06F, 0.18F, 0.40F, QVector2D(0.070F, 0.027F)},
      {0.044F, 1.12F, 0.26F, 0.46F, QVector2D(0.072F, 0.028F)},
      {0.040F, 1.16F, 0.34F, 0.50F, QVector2D(0.073F, 0.028F)},
  }};
  std::vector<int> trunk_chain;
  trunk_chain.reserve(k_trunk.size());
  for (const TrunkRing& ring : k_trunk) {
    trunk_chain.push_back(loft.add_ring(
        {ring.radius, ring.y, ring.normal_up, ring.v, 0.0F, ring.offset}));
  }
  for (std::size_t i = 1; i < trunk_chain.size(); ++i) {
    loft.connect(trunk_chain[i - 1U], trunk_chain[i]);
  }
  loft.cap(trunk_chain.front(), -0.04F, QVector2D(0.0F, 0.0F), 0.0F, 0.0F, false);

  const QVector3D crown(
      k_trunk.back().offset.x(), k_trunk.back().y, k_trunk.back().offset.y());

  {
    const QVector2D cc(crown.x(), crown.z());
    const int h0 = loft.add_ring(
        {0.070F, crown.y() + 0.012F, -0.20F, k_leaf_v, k_fibre_material, cc});
    const int h1 = loft.add_ring(
        {0.105F, crown.y() + 0.06F, 0.10F, k_leaf_v + 0.06F, k_fibre_material, cc});
    const int h2 = loft.add_ring(
        {0.080F, crown.y() + 0.15F, 0.55F, k_leaf_v + 0.12F, k_fibre_material, cc});
    loft.connect(trunk_chain.back(), h0);
    loft.connect(h0, h1);
    loft.connect(h1, h2);
    loft.cap(h2, crown.y() + 0.24F, cc, k_leaf_v + 0.18F, k_fibre_material, true);
  }

  for (const float yaw : {0.9F, 3.6F}) {
    const QVector2D cc(crown.x() + std::cos(yaw) * 0.12F,
                       crown.z() + std::sin(yaw) * 0.12F);
    const int d0 = loft.add_ring(
        {0.030F, crown.y() - 0.30F, -0.60F, k_dead_v, k_fibre_material, cc});
    const int d1 = loft.add_ring(
        {0.060F, crown.y() - 0.20F, -0.10F, k_dead_v, k_fibre_material, cc});
    const int d2 = loft.add_ring(
        {0.045F, crown.y() - 0.08F, 0.40F, k_dead_v, k_fibre_material, cc});
    loft.connect(d0, d1);
    loft.connect(d1, d2);
    loft.cap(d0, crown.y() - 0.35F, cc, k_dead_v, k_fibre_material, false);
    loft.cap(d2, crown.y() - 0.02F, cc, k_dead_v, k_fibre_material, true);
  }

  std::vector<PalmVertex> vertices;
  constexpr int k_vertices_per_frond =
      2 * k_frond_stations + 8 * (k_frond_stations - 1);
  vertices.reserve(loft.vertices().size() +
                   (k_frond_count + k_dead_frond_count) * k_vertices_per_frond);
  for (const auto& vertex : loft.vertices()) {
    vertices.push_back(
        {vertex.position, QVector3D(vertex.u, vertex.v, vertex.weight), vertex.normal});
  }
  std::vector<std::uint16_t> indices = loft.indices();
  // Match the trunk and crown winding to their outward radial normals.
  for (std::size_t i = 0; i < indices.size(); i += 3) {
    std::swap(indices[i + 1], indices[i + 2]);
  }

  const auto first_frond_vertex = static_cast<std::uint16_t>(vertices.size());

  auto add_frond = [&](float yaw,
                       float length,
                       float rise,
                       float droop,
                       float width,
                       float base_lift,
                       float v_base,
                       float v_span,
                       float frond_u,
                       float material) {
    const QVector3D dir(std::cos(yaw), 0.0F, std::sin(yaw));
    const QVector3D side(-std::sin(yaw), 0.0F, std::cos(yaw));
    auto spine = [&](float t) {
      const float lift = base_lift + rise * std::sin(t * 1.9F) - droop * t * t * t;
      const float sweep = 0.065F * std::sin(yaw * 3.0F) * t * t;
      return crown + dir * (length * t) + side * sweep + QVector3D(0.0F, lift, 0.0F);
    };
    auto vertex = [&](const QVector3D& pos, float t, float across) {
      vertices.push_back({pos,
                          {frond_u,
                           v_base + t * v_span,
                           material + across * k_frond_edge_material},
                          {}});
    };
    const auto rib = static_cast<std::uint16_t>(vertices.size());
    for (int station = 0; station < k_frond_stations; ++station) {
      const float t = static_cast<float>(station) / (k_frond_stations - 1);
      const float half_rib = width * (0.17F - t * 0.14F);
      vertex(spine(t) + side * half_rib, t, 0.10F);
      vertex(spine(t) - side * half_rib, t, 0.10F);
      if (station > 0) {
        const auto a = static_cast<std::uint16_t>(rib + 2 * (station - 1));
        indices.insert(indices.end(),
                       {a,
                        std::uint16_t(a + 2),
                        std::uint16_t(a + 3),
                        a,
                        std::uint16_t(a + 3),
                        std::uint16_t(a + 1)});
      }
    }
    // Each pair sweeps forward from the rib. Air shows through the tips,
    // while the overlapping bases keep the frond readable from a distance.
    for (int pair = 0; pair < k_frond_stations - 1; ++pair) {
      for (int hand : {-1, 1}) {
        const float t = 0.035F + (pair + (hand > 0 ? 0.20F : 0.0F)) *
                                     (0.88F / (k_frond_stations - 1));
        const float fan = std::sin((0.15F + 0.80F * t) * k_pi);
        const float span = width * (0.35F + 1.05F * fan);
        const QVector3D root = spine(t);
        const QVector3D shoulder = spine(t + 0.05F) + side * (span * hand * 0.44F);
        const QVector3D end = spine(t + 0.13F) + side * (span * hand) -
                              QVector3D(0.0F, span * (0.46F + 0.42F * t), 0.0F);
        const auto a = static_cast<std::uint16_t>(vertices.size());
        const QVector3D half_blade = dir * (length * 0.055F * fan);
        vertex(root, t, 0.0F);
        vertex(shoulder - half_blade, t + 0.02F, 0.47F);
        vertex(end, t + 0.13F, 1.0F);
        vertex(shoulder + half_blade, t + 0.08F, 0.47F);
        const auto left = static_cast<std::uint16_t>(a + (hand > 0 ? 1 : 3));
        const auto right = static_cast<std::uint16_t>(a + (hand > 0 ? 3 : 1));
        indices.insert(indices.end(),
                       {a, left, std::uint16_t(a + 2), a, std::uint16_t(a + 2), right});
      }
    }
  };

  for (int f = 0; f < k_frond_count; ++f) {
    const float frond_u = static_cast<float>(f) / static_cast<float>(k_frond_count);
    const float wobble = std::sin(static_cast<float>(f) * 2.399963F);
    const float ring = (f % 2 == 0) ? 1.0F : 0.0F;
    const float yaw = frond_u * k_two_pi + wobble * 0.14F;
    add_frond(yaw,
              0.70F + wobble * 0.07F - ring * 0.08F,
              0.23F + wobble * 0.06F + ring * 0.08F,
              0.42F + wobble * 0.14F,
              0.138F,
              0.08F + ring * 0.05F,
              k_leaf_v,
              0.34F,
              frond_u,
              k_live_frond_material);
  }
  for (int f = 0; f < k_dead_frond_count; ++f) {
    const float frond_u =
        (static_cast<float>(f) + 0.5F) / static_cast<float>(k_dead_frond_count);
    const float wobble = std::sin(static_cast<float>(f) * 1.7F + 0.6F);
    add_frond(frond_u * k_two_pi + wobble * 0.2F,
              0.30F + wobble * 0.04F,
              0.0F,
              0.62F,
              0.070F,
              -0.03F,
              k_dead_v,
              0.0F,
              frond_u,
              k_dead_frond_material);
  }

  for (std::size_t i = first_frond_vertex; i < vertices.size(); ++i) {
    vertices[i].normal = QVector3D(0.0F, 0.0F, 0.0F);
  }
  for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
    if (indices[i] < first_frond_vertex) {
      continue;
    }
    const QVector3D& p0 = vertices[indices[i]].position;
    const QVector3D& p1 = vertices[indices[i + 1]].position;
    const QVector3D& p2 = vertices[indices[i + 2]].position;
    const QVector3D face = QVector3D::crossProduct(p1 - p0, p2 - p0);
    vertices[indices[i]].normal += face;
    vertices[indices[i + 1]].normal += face;
    vertices[indices[i + 2]].normal += face;
  }
  for (std::size_t i = first_frond_vertex; i < vertices.size(); ++i) {
    const QVector3D normal = vertices[i].normal;
    vertices[i].normal =
        normal.isNull() ? QVector3D(0.0F, 1.0F, 0.0F) : normal.normalized();
  }

  constexpr std::array<VertexAttributeLayout, 3> k_palm_attributes{{
      {k_foliage_position_location, vec3, offsetof(PalmVertex, position)},
      {k_foliage_tex_coord_location, vec3, offsetof(PalmVertex, tex_coord)},
      {k_foliage_normal_location, vec3, offsetof(PalmVertex, normal)},
  }};
  upload_static_instanced_mesh(*this,
                               m_palm_mesh,
                               vertices.data(),
                               vertices.size(),
                               sizeof(PalmVertex),
                               k_palm_attributes,
                               indices.data(),
                               indices.size(),
                               k_foliage_instance_locations);
}

} // namespace Render::GL::BackendPipelines
