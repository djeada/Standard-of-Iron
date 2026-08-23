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
#include <utility>
#include <vector>

#include "dead_tree_mesh.h"
#include "gl/shader_cache.h"
#include "mesh_buffers.h"
#include "prop_mesh_builder.h"
#include "render/gl/backend/ring_loft_builder.h"
#include "render/gl/backend/static_mesh_upload.h"
#include "render/gl/platform_gl.h"
#include "render/gl/render_constants.h"
#include "vegetation_pipeline.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::Geometry;

constexpr GLuint k_foliage_position_location = 0;
constexpr GLuint k_foliage_tex_coord_location = 1;
constexpr GLuint k_foliage_normal_location = 2;

constexpr std::array<GLuint, 3> k_foliage_instance_locations{3, 4, 5};

constexpr std::array<GLuint, 2> k_stone_instance_locations{2, 3};

void VegetationPipeline::initialize_stone_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_stone_mesh);

  struct StoneVertex {
    QVector3D position;
    QVector3D normal;
  };

  using F = StoneVertex;
  std::vector<F> verts;
  std::vector<uint16_t> idx;

  constexpr int k_n = 8;
  constexpr float k_tau = 6.28318530F;
  constexpr int k_rings = 5;

  constexpr float k_perturb_amount = 0.07F;
  constexpr float k_perturb_freq_vertex = 2.3F;
  constexpr float k_perturb_freq_ring = 1.7F;

  constexpr float k_y_jitter_amount = 0.025F;
  constexpr float k_y_jitter_freq_vertex = 1.9F;
  constexpr float k_y_jitter_freq_ring = 0.9F;

  constexpr float k_apex_offset_x = 0.08F;
  constexpr float k_apex_height = 0.66F;
  constexpr float k_apex_offset_z = -0.07F;

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
      {-0.03F, 0.34F, 0.12F, 1.08F, 1.16F, -0.04F, 0.02F},
      {0.12F, 0.50F, -0.04F, 1.12F, 0.96F, 0.01F, -0.02F},
      {0.30F, 0.46F, 0.18F, 0.98F, 1.08F, 0.04F, -0.01F},
      {0.47F, 0.31F, 0.06F, 1.06F, 0.90F, 0.01F, -0.05F},
      {0.58F, 0.16F, 0.24F, 0.92F, 1.02F, 0.04F, -0.05F},
  };

  QVector3D ring_pts[k_rings][k_n];
  for (int ri = 0; ri < k_rings; ++ri) {
    const Ring& r = rings[ri];
    for (int i = 0; i < k_n; ++i) {
      float const t = static_cast<float>(i) / k_n;
      float const angle = t * k_tau + r.phase;

      float const perturb =
          1.0F + k_perturb_amount * std::sin(float(i) * k_perturb_freq_vertex +
                                             float(ri) * k_perturb_freq_ring);
      float const rx = r.radius * perturb * r.sx * std::cos(angle);
      float const rz = r.radius * perturb * r.sz * std::sin(angle);

      float const ry =
          r.y + k_y_jitter_amount * std::cos(float(i) * k_y_jitter_freq_vertex +
                                             float(ri) * k_y_jitter_freq_ring);
      ring_pts[ri][i] = QVector3D(rx + r.cx, ry, rz + r.cz);
    }
  }

  auto emit_quad = [&](const QVector3D& a,
                       const QVector3D& b,
                       const QVector3D& c,
                       const QVector3D& d) {
    QVector3D n = QVector3D::crossProduct(d - a, b - a);
    if (n.lengthSquared() > 1.0e-8F) {
      n.normalize();
    } else {
      n = QVector3D(0.0F, 1.0F, 0.0F);
    }
    auto base = static_cast<uint16_t>(verts.size());
    verts.push_back({a, n});
    verts.push_back({b, n});
    verts.push_back({c, n});
    verts.push_back({d, n});
    idx.push_back(base);
    idx.push_back(uint16_t(base + 2));
    idx.push_back(uint16_t(base + 1));
    idx.push_back(base);
    idx.push_back(uint16_t(base + 3));
    idx.push_back(uint16_t(base + 2));
  };

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

  QVector3D const bot_center(0.0F, -0.01F, 0.0F);
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
  release_mesh_buffers(*this, m_plant_mesh);

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

void VegetationPipeline::initialize_pine_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_pine_mesh);

  struct PineVertex {
    QVector3D position;
    QVector3D tex_coord;
    QVector3D normal;
  };

  constexpr int k_segments = 20;
  RingLoftBuilder loft(k_segments);
  loft.reserve(32);

  const int trunk_bottom = loft.add_ring({0.088F, -0.01F, -0.08F, 0.00F, 0.0F});
  const int trunk_kink =
      loft.add_ring({0.066F, 0.16F, 0.00F, 0.08F, 0.0F, QVector2D(0.010F, 0.006F)});
  const int trunk_mid =
      loft.add_ring({0.054F, 0.32F, 0.03F, 0.16F, 0.0F, QVector2D(0.022F, 0.012F)});
  const int trunk_top =
      loft.add_ring({0.046F, 0.48F, 0.08F, 0.26F, 0.0F, QVector2D(0.020F, 0.014F)});

  struct TierPlan {
    float base_y;
    float outer_r;
    QVector2D offset;
  };
  constexpr std::array<TierPlan, 5> k_tiers{{
      {0.52F, 0.330F, QVector2D(-0.030F, 0.044F)},
      {0.70F, 0.280F, QVector2D(0.036F, -0.024F)},
      {0.86F, 0.230F, QVector2D(-0.020F, -0.030F)},
      {1.00F, 0.180F, QVector2D(0.024F, 0.018F)},
      {1.12F, 0.132F, QVector2D(-0.014F, 0.022F)},
  }};

  std::vector<int> chain{trunk_bottom, trunk_kink, trunk_mid, trunk_top};
  for (std::size_t t = 0; t < k_tiers.size(); ++t) {
    const TierPlan& tier = k_tiers[t];
    float const r = tier.outer_r;
    float const y = tier.base_y;
    float const v0 = 0.34F + (0.145F * static_cast<float>(t));
    const QVector2D& o = tier.offset;

    chain.push_back(loft.add_ring({r * 0.36F, y, 0.14F, v0, 0.00F, o * 0.12F}));
    chain.push_back(
        loft.add_ring({r * 0.76F, y + 0.028F, 0.28F, v0 + 0.035F, 0.60F, o * 0.42F}));
    chain.push_back(loft.add_ring({r, y + 0.050F, 0.42F, v0 + 0.062F, 1.00F, o}));
    chain.push_back(
        loft.add_ring({r * 0.78F, y + 0.096F, 0.62F, v0 + 0.092F, 0.74F, o * 0.55F}));
    chain.push_back(
        loft.add_ring({r * 0.44F, y + 0.138F, 0.80F, v0 + 0.118F, 0.30F, o * 0.22F}));
  }

  const int tip_ring = loft.add_ring({0.024F, 1.32F, 0.95F, 1.10F, 0.20F});
  chain.push_back(tip_ring);

  for (std::size_t i = 1; i < chain.size(); ++i) {
    loft.connect(chain[i - 1U], chain[i]);
  }

  loft.cap(trunk_bottom, 0.0F, QVector2D(0.0F, 0.0F), 0.0F, 0.0F, false);
  loft.cap(tip_ring, 1.42F, QVector2D(0.0F, 0.0F), 1.16F, 0.30F, true);

  std::vector<PineVertex> vertices;
  vertices.reserve(loft.vertices().size());
  for (const auto& v : loft.vertices()) {
    vertices.push_back({v.position, QVector3D(v.u, v.v, v.weight), v.normal});
  }

  constexpr std::array<VertexAttributeLayout, 3> k_pine_attributes{{
      {k_foliage_position_location, vec3, offsetof(PineVertex, position)},
      {k_foliage_tex_coord_location, vec3, offsetof(PineVertex, tex_coord)},
      {k_foliage_normal_location, vec3, offsetof(PineVertex, normal)},
  }};
  upload_static_instanced_mesh(*this,
                               m_pine_mesh,
                               vertices.data(),
                               vertices.size(),
                               sizeof(PineVertex),
                               k_pine_attributes,
                               loft.indices().data(),
                               loft.indices().size(),
                               k_foliage_instance_locations);
}

void VegetationPipeline::initialize_olive_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_olive_mesh);

  struct OliveVertex {
    QVector3D position;
    QVector2D tex_coord;
    QVector3D normal;
  };

  RingLoftBuilder loft(olive_tree_segments);
  loft.reserve(96);

  auto add_ring = [&](float radius,
                      float y,
                      float normal_up,
                      float v_coord,
                      const QVector2D& offset = QVector2D(0.0F, 0.0F)) -> int {
    return loft.add_ring({radius, y, normal_up, v_coord, 0.0F, offset});
  };
  auto connect_rings = [&](int lower, int upper) {
    loft.connect(lower, upper);
  };
  auto add_cap = [&](int ring, float cap_y, const QVector2D& offset, float v) {
    loft.cap(ring, cap_y, offset, v);
  };

  int const t0 = add_ring(0.19F, -0.015F, -0.30F, 0.00F, QVector2D(-0.018F, 0.004F));
  int const t1 = add_ring(0.14F, 0.09F, -0.05F, 0.07F, QVector2D(0.012F, -0.014F));
  int const t2 = add_ring(0.105F, 0.18F, 0.08F, 0.14F, QVector2D(0.030F, 0.006F));
  int const t3 = add_ring(0.080F, 0.28F, 0.20F, 0.22F, QVector2D(0.012F, 0.030F));
  int const t4 = add_ring(0.065F, 0.36F, 0.34F, 0.29F, QVector2D(0.006F, 0.038F));
  connect_rings(t0, t1);
  connect_rings(t1, t2);
  connect_rings(t2, t3);
  connect_rings(t3, t4);

  auto add_branch = [&](float dir_x,
                        float dir_z,
                        const QVector2D& base_offset,
                        float base_y,
                        float length,
                        float rise,
                        float branch_r,
                        float leaf_r,
                        float v_start,
                        float lateral_bias) {
    float const len = std::sqrt(dir_x * dir_x + dir_z * dir_z);
    dir_x /= len;
    dir_z /= len;

    QVector2D const dir(dir_x, dir_z);
    QVector2D const ortho(-dir_z, dir_x);

    int const b0 = add_ring(branch_r, base_y, 0.08F, v_start, base_offset);

    float const mid_dist = length * 0.5F;
    QVector2D const mid_offset =
        base_offset + dir * mid_dist + ortho * (leaf_r * 0.16F * lateral_bias);
    int const b1 = add_ring(
        branch_r * 0.72F, base_y + rise * 0.48F, 0.26F, v_start + 0.08F, mid_offset);

    float const tip_dist = length;
    QVector2D const tip_offset =
        base_offset + dir * tip_dist + ortho * (leaf_r * 0.26F * lateral_bias);
    float const tip_y = base_y + rise;
    int const b2 =
        add_ring(branch_r * 0.42F, tip_y, 0.48F, v_start + 0.18F, tip_offset);

    connect_rings(b0, b1);
    connect_rings(b1, b2);

    QVector2D const crown_shift = ortho * (leaf_r * 0.18F * lateral_bias);
    float const ly = tip_y - leaf_r * 0.22F;
    int const l0 =
        add_ring(leaf_r * 0.30F, ly, -0.58F, 0.52F, tip_offset - crown_shift * 0.15F);
    int const l1 = add_ring(leaf_r * 0.68F,
                            ly + leaf_r * 0.16F,
                            -0.18F,
                            0.64F,
                            tip_offset + crown_shift * 0.20F);
    int const l2 = add_ring(leaf_r * 0.90F,
                            ly + leaf_r * 0.38F,
                            0.14F,
                            0.78F,
                            tip_offset + crown_shift * 0.58F);
    int const l3 = add_ring(
        leaf_r * 0.76F, ly + leaf_r * 0.62F, 0.50F, 0.92F, tip_offset + crown_shift);
    int const l4 = add_ring(leaf_r * 0.40F,
                            ly + leaf_r * 0.86F,
                            0.84F,
                            1.02F,
                            tip_offset + crown_shift * 0.56F);

    connect_rings(b2, l0);
    connect_rings(l0, l1);
    connect_rings(l1, l2);
    connect_rings(l2, l3);
    connect_rings(l3, l4);
    add_cap(l4, ly + leaf_r * 1.02F, tip_offset + crown_shift * 0.34F, 1.10F);
  };

  add_branch(0.92F,
             0.22F,
             QVector2D(0.020F, 0.030F),
             0.26F,
             0.36F,
             0.30F,
             0.030F,
             0.19F,
             0.30F,
             0.60F);
  add_branch(-0.72F,
             0.58F,
             QVector2D(0.006F, 0.036F),
             0.30F,
             0.34F,
             0.28F,
             0.026F,
             0.21F,
             0.32F,
             -0.54F);
  add_branch(0.42F,
             -0.91F,
             QVector2D(0.016F, 0.028F),
             0.28F,
             0.33F,
             0.27F,
             0.024F,
             0.18F,
             0.34F,
             0.42F);
  add_branch(-0.55F,
             -0.76F,
             QVector2D(0.012F, 0.022F),
             0.25F,
             0.39F,
             0.31F,
             0.027F,
             0.20F,
             0.28F,
             -0.36F);
  add_branch(0.98F,
             -0.08F,
             QVector2D(0.014F, 0.040F),
             0.34F,
             0.24F,
             0.24F,
             0.018F,
             0.16F,
             0.38F,
             0.28F);
  add_branch(-0.08F,
             1.0F,
             QVector2D(0.008F, 0.044F),
             0.36F,
             0.22F,
             0.23F,
             0.018F,
             0.15F,
             0.40F,
             -0.20F);
  add_branch(0.18F,
             0.98F,
             QVector2D(-0.004F, 0.046F),
             0.38F,
             0.20F,
             0.22F,
             0.016F,
             0.14F,
             0.42F,
             0.16F);
  add_branch(-0.96F,
             -0.18F,
             QVector2D(0.010F, 0.028F),
             0.32F,
             0.25F,
             0.20F,
             0.017F,
             0.135F,
             0.38F,
             -0.24F);
  add_branch(0.70F,
             0.71F,
             QVector2D(0.004F, 0.042F),
             0.39F,
             0.19F,
             0.19F,
             0.014F,
             0.12F,
             0.44F,
             0.18F);

  std::vector<OliveVertex> vertices;
  vertices.reserve(loft.vertices().size());
  for (const auto& v : loft.vertices()) {
    vertices.push_back({v.position, QVector2D(v.u, v.v), v.normal});
  }

  constexpr std::array<VertexAttributeLayout, 3> k_olive_attributes{{
      {k_foliage_position_location, vec3, offsetof(OliveVertex, position)},
      {k_foliage_tex_coord_location, vec2, offsetof(OliveVertex, tex_coord)},
      {k_foliage_normal_location, vec3, offsetof(OliveVertex, normal)},
  }};
  upload_static_instanced_mesh(*this,
                               m_olive_mesh,
                               vertices.data(),
                               vertices.size(),
                               sizeof(OliveVertex),
                               k_olive_attributes,
                               loft.indices().data(),
                               loft.indices().size(),
                               k_foliage_instance_locations);
}

void VegetationPipeline::initialize_dead_tree_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_dead_tree_mesh);

  const auto mesh = build_dead_tree_mesh();

  upload_prop_mesh_impl(mesh.vertices, mesh.indices, m_dead_tree_mesh);
}

void VegetationPipeline::initialize_iron_ore_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_iron_ore_mesh);

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  append_oriented_box(
      verts, idx, {-0.58F, 0.02F, -0.20F}, {0.48F, 0.12F, 0.18F}, 0.24F, 0.18F);
  append_oriented_box(
      verts, idx, {-0.42F, 0.10F, -0.18F}, {0.26F, 0.38F, 0.08F}, 0.20F, 0.17F);
  append_oriented_box(
      verts, idx, {-0.30F, 0.34F, -0.12F}, {-0.04F, 0.66F, 0.02F}, 0.13F, 0.12F);
  append_oriented_box(
      verts, idx, {0.00F, 0.30F, -0.06F}, {0.30F, 0.56F, 0.10F}, 0.12F, 0.10F);
  append_oriented_box(
      verts, idx, {-0.18F, 0.10F, 0.24F}, {0.20F, 0.31F, 0.42F}, 0.14F, 0.10F);
  append_oriented_box(
      verts, idx, {-0.32F, 0.09F, -0.38F}, {0.08F, 0.27F, -0.30F}, 0.12F, 0.09F);

  upload_prop_mesh_impl(verts, idx, m_iron_ore_mesh);
}

void VegetationPipeline::initialize_cypress_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_cypress_mesh);

  struct CypressVertex {
    QVector3D position;
    QVector3D tex_coord;
    QVector3D normal;
  };

  constexpr int k_segments = 20;
  RingLoftBuilder loft(k_segments);
  loft.reserve(16);

  struct SpireRing {
    float radius;
    float y;
    float normal_up;
    float v;
    float weight;
    QVector2D offset;
  };
  constexpr std::array<SpireRing, 12> k_spire{{
      {0.062F, -0.01F, -0.10F, 0.00F, 0.0F, QVector2D(0.0F, 0.0F)},
      {0.050F, 0.14F, 0.02F, 0.10F, 0.0F, QVector2D(0.008F, 0.005F)},
      {0.042F, 0.28F, 0.06F, 0.22F, 0.0F, QVector2D(0.016F, 0.009F)},
      {0.078F, 0.32F, -0.42F, 0.36F, 0.35F, QVector2D(0.014F, 0.008F)},
      {0.132F, 0.46F, -0.16F, 0.44F, 0.70F, QVector2D(0.010F, -0.006F)},
      {0.158F, 0.64F, 0.02F, 0.53F, 1.00F, QVector2D(-0.008F, 0.010F)},
      {0.162F, 0.84F, 0.04F, 0.62F, 1.00F, QVector2D(0.012F, 0.006F)},
      {0.150F, 1.04F, 0.08F, 0.70F, 1.00F, QVector2D(-0.010F, -0.008F)},
      {0.128F, 1.22F, 0.14F, 0.78F, 0.95F, QVector2D(0.008F, 0.012F)},
      {0.098F, 1.38F, 0.24F, 0.85F, 0.85F, QVector2D(-0.006F, 0.006F)},
      {0.062F, 1.52F, 0.42F, 0.92F, 0.70F, QVector2D(0.004F, -0.004F)},
      {0.028F, 1.64F, 0.72F, 0.98F, 0.45F, QVector2D(0.0F, 0.0F)},
  }};

  std::vector<int> chain;
  chain.reserve(k_spire.size());
  for (const SpireRing& ring : k_spire) {
    chain.push_back(loft.add_ring(
        {ring.radius, ring.y, ring.normal_up, ring.v, ring.weight, ring.offset}));
  }
  for (std::size_t i = 1; i < chain.size(); ++i) {
    loft.connect(chain[i - 1U], chain[i]);
  }
  loft.cap(chain.front(), -0.03F, QVector2D(0.0F, 0.0F), 0.0F, 0.0F, false);
  loft.cap(chain.back(), 1.72F, QVector2D(0.0F, 0.0F), 1.02F, 0.30F, true);

  std::vector<CypressVertex> vertices;
  vertices.reserve(loft.vertices().size());
  for (const auto& v : loft.vertices()) {
    vertices.push_back({v.position, QVector3D(v.u, v.v, v.weight), v.normal});
  }

  constexpr std::array<VertexAttributeLayout, 3> k_cypress_attributes{{
      {k_foliage_position_location, vec3, offsetof(CypressVertex, position)},
      {k_foliage_tex_coord_location, vec3, offsetof(CypressVertex, tex_coord)},
      {k_foliage_normal_location, vec3, offsetof(CypressVertex, normal)},
  }};
  upload_static_instanced_mesh(*this,
                               m_cypress_mesh,
                               vertices.data(),
                               vertices.size(),
                               sizeof(CypressVertex),
                               k_cypress_attributes,
                               loft.indices().data(),
                               loft.indices().size(),
                               k_foliage_instance_locations);
}

void VegetationPipeline::initialize_palm_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_palm_mesh);

  struct PalmVertex {
    QVector3D position;
    QVector2D tex_coord;
    QVector3D normal;
  };

  constexpr int k_trunk_segments = olive_tree_segments;
  constexpr float k_pi = 3.14159265F;
  constexpr float k_two_pi = 6.28318530718F;
  constexpr int k_frond_count = 11;
  constexpr int k_frond_stations = 7;

  RingLoftBuilder loft(k_trunk_segments);
  loft.reserve(10);

  struct TrunkRing {
    float radius;
    float y;
    float normal_up;
    float v;
    QVector2D offset;
  };
  constexpr std::array<TrunkRing, 8> k_trunk{{
      {0.088F, -0.015F, -0.34F, 0.04F, QVector2D(0.0F, 0.0F)},
      {0.086F, 0.20F, -0.04F, 0.11F, QVector2D(0.010F, 0.004F)},
      {0.072F, 0.42F, 0.02F, 0.19F, QVector2D(0.024F, 0.010F)},
      {0.062F, 0.62F, 0.06F, 0.27F, QVector2D(0.040F, 0.016F)},
      {0.055F, 0.76F, 0.10F, 0.36F, QVector2D(0.050F, 0.019F)},
      {0.050F, 0.84F, 0.14F, 0.45F, QVector2D(0.055F, 0.021F)},
      {0.046F, 0.90F, 0.18F, 0.52F, QVector2D(0.058F, 0.022F)},
      {0.042F, 0.95F, 0.26F, 0.58F, QVector2D(0.060F, 0.022F)},
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

  std::vector<PalmVertex> vertices;
  vertices.reserve(loft.vertices().size() +
                   static_cast<std::size_t>(k_frond_count * k_frond_stations * 4));
  for (const auto& vertex : loft.vertices()) {
    vertices.push_back({vertex.position, QVector2D(vertex.u, vertex.v), vertex.normal});
  }
  std::vector<std::uint16_t> indices = loft.indices();

  const QVector3D crown(
      k_trunk.back().offset.x(), k_trunk.back().y, k_trunk.back().offset.y());
  const auto first_frond_vertex = static_cast<std::uint16_t>(vertices.size());

  for (int f = 0; f < k_frond_count; ++f) {
    const float frond_u = static_cast<float>(f) / static_cast<float>(k_frond_count);
    const float wobble = std::sin(static_cast<float>(f) * 2.399963F);
    const float yaw = frond_u * k_two_pi + wobble * 0.12F;
    const float length = 0.66F + wobble * 0.08F;
    const float rise = 0.26F + wobble * 0.05F;
    const float droop = 0.62F + wobble * 0.10F;
    const QVector3D dir(std::cos(yaw), 0.0F, std::sin(yaw));
    const QVector3D side(-std::sin(yaw), 0.0F, std::cos(yaw));

    const auto base = static_cast<std::uint16_t>(vertices.size());
    for (int station = 0; station < k_frond_stations; ++station) {
      const float t =
          static_cast<float>(station) / static_cast<float>(k_frond_stations - 1);
      const float lift = rise * std::sin(t * 1.7278F) - droop * t * t * t;

      const float pinnate = (station % 2 == 0) ? 1.0F : 0.64F;
      const float half_width =
          0.104F * pinnate * std::sin(std::clamp(t * 0.86F + 0.10F, 0.0F, 1.0F) * k_pi);
      const float fold = -half_width * 1.15F;

      const QVector3D spine = crown + dir * (length * t) + QVector3D(0.0F, lift, 0.0F);
      const QVector2D uv(frond_u, 0.60F + t * 0.34F);
      const QVector3D up(0.0F, 1.0F, 0.0F);
      const QVector3D droop_edge(0.0F, fold, 0.0F);

      vertices.push_back({spine + side * half_width + droop_edge, uv, up});
      vertices.push_back({spine, uv, up});
      vertices.push_back({spine, uv, up});
      vertices.push_back({spine - side * half_width + droop_edge, uv, up});
    }

    for (int station = 0; station + 1 < k_frond_stations; ++station) {
      const auto lower = static_cast<std::uint16_t>(base + station * 4);
      const auto upper = static_cast<std::uint16_t>(lower + 4);
      for (int half = 0; half < 2; ++half) {
        const auto a0 = static_cast<std::uint16_t>(lower + half * 2);
        const auto a1 = static_cast<std::uint16_t>(lower + half * 2 + 1);
        const auto b0 = static_cast<std::uint16_t>(upper + half * 2);
        const auto b1 = static_cast<std::uint16_t>(upper + half * 2 + 1);
        indices.insert(indices.end(), {a0, b0, b1, a0, b1, a1});
      }
    }
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
      {k_foliage_tex_coord_location, vec2, offsetof(PalmVertex, tex_coord)},
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
