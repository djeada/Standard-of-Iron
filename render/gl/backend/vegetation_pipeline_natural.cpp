#include <QDebug>
#include <QOpenGLContext>
#include <QString>
#include <qglobal.h>
#include <qopenglcontext.h>
#include <qopenglext.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "../render_constants.h"
#include "dead_tree_mesh.h"
#include "gl/shader_cache.h"
#include "mesh_buffers.h"
#include "prop_mesh_builder.h"
#include "vegetation_pipeline.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::Geometry;

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
    QVector3D n = QVector3D::crossProduct(b - a, d - a);
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
    idx.push_back(uint16_t(base + 1));
    idx.push_back(uint16_t(base + 2));
    idx.push_back(base);
    idx.push_back(uint16_t(base + 2));
    idx.push_back(uint16_t(base + 3));
  };

  auto emit_tri = [&](const QVector3D& a, const QVector3D& b, const QVector3D& c) {
    QVector3D n = QVector3D::crossProduct(b - a, c - a);
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
    idx.push_back(uint16_t(base + 1));
    idx.push_back(uint16_t(base + 2));
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

  glGenVertexArrays(1, &m_stone_mesh.vao);
  glBindVertexArray(m_stone_mesh.vao);

  glGenBuffers(1, &m_stone_mesh.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_stone_mesh.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(verts.size() * sizeof(StoneVertex)),
               verts.data(),
               GL_STATIC_DRAW);
  m_stone_mesh.vertex_count = static_cast<GLsizei>(verts.size());

  glGenBuffers(1, &m_stone_mesh.index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_stone_mesh.index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(idx.size() * sizeof(uint16_t)),
               idx.data(),
               GL_STATIC_DRAW);
  m_stone_mesh.index_count = static_cast<GLsizei>(idx.size());

  glEnableVertexAttribArray(position);
  glVertexAttribPointer(position,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(StoneVertex),
                        reinterpret_cast<void*>(offsetof(StoneVertex, position)));
  glEnableVertexAttribArray(normal);
  glVertexAttribPointer(normal,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(StoneVertex),
                        reinterpret_cast<void*>(offsetof(StoneVertex, normal)));

  glEnableVertexAttribArray(tex_coord);
  glVertexAttribDivisor(tex_coord, 1);
  glEnableVertexAttribArray(instance_position);
  glVertexAttribDivisor(instance_position, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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

  glGenVertexArrays(1, &m_plant_mesh.vao);
  glBindVertexArray(m_plant_mesh.vao);

  glGenBuffers(1, &m_plant_mesh.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_plant_mesh.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(plant_vertices), plant_vertices, GL_STATIC_DRAW);
  m_plant_mesh.vertex_count = plant_cross_quad_vertex_count;

  glEnableVertexAttribArray(position);
  glVertexAttribPointer(position,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(PlantVertex),
                        reinterpret_cast<void*>(offsetof(PlantVertex, position)));

  glEnableVertexAttribArray(normal);
  glVertexAttribPointer(normal,
                        vec2,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(PlantVertex),
                        reinterpret_cast<void*>(offsetof(PlantVertex, tex_coord)));

  glEnableVertexAttribArray(tex_coord);
  glVertexAttribPointer(tex_coord,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(PlantVertex),
                        reinterpret_cast<void*>(offsetof(PlantVertex, normal)));

  glGenBuffers(1, &m_plant_mesh.index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_plant_mesh.index_buffer);
  glBufferData(
      GL_ELEMENT_ARRAY_BUFFER, sizeof(plant_indices), plant_indices, GL_STATIC_DRAW);
  m_plant_mesh.index_count = plant_cross_quad_index_count;

  glEnableVertexAttribArray(instance_position);
  glVertexAttribDivisor(instance_position, 1);
  glEnableVertexAttribArray(instance_scale);
  glVertexAttribDivisor(instance_scale, 1);
  glEnableVertexAttribArray(instance_color);
  glVertexAttribDivisor(instance_color, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void VegetationPipeline::initialize_pine_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_pine_mesh);

  struct PineVertex {
    QVector3D position;

    QVector3D tex_coord;
    QVector3D normal;
  };

  constexpr int k_segments = 16;
  constexpr float k_two_pi = 6.28318530718F;

  std::vector<PineVertex> vertices;
  vertices.reserve(22 * k_segments + 2);

  std::vector<unsigned short> indices;
  indices.reserve(22 * k_segments * 6);

  auto add_ring = [&](float radius,
                      float y,
                      float normal_up,
                      float v_coord,
                      float bough,
                      const QVector2D& center_offset = QVector2D(0.0F, 0.0F)) -> int {
    const int start = static_cast<int>(vertices.size());
    for (int i = 0; i < k_segments; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(k_segments);
      const float angle = t * k_two_pi;
      const float nx = std::cos(angle);
      const float nz = std::sin(angle);
      QVector3D normal(nx, normal_up, nz);
      normal.normalize();
      QVector3D const position(
          radius * nx + center_offset.x(), y, radius * nz + center_offset.y());
      QVector3D const tex_coord(t, v_coord, bough);
      vertices.push_back({position, tex_coord, normal});
    }
    return start;
  };

  auto connect_rings = [&](int lower_start, int upper_start) {
    for (int i = 0; i < k_segments; ++i) {
      const int next = (i + 1) % k_segments;
      const auto lower0 = static_cast<unsigned short>(lower_start + i);
      const auto lower1 = static_cast<unsigned short>(lower_start + next);
      const auto upper0 = static_cast<unsigned short>(upper_start + i);
      const auto upper1 = static_cast<unsigned short>(upper_start + next);

      indices.push_back(lower0);
      indices.push_back(lower1);
      indices.push_back(upper1);
      indices.push_back(lower0);
      indices.push_back(upper1);
      indices.push_back(upper0);
    }
  };

  const int trunk_bottom = add_ring(0.14F, -0.01F, -0.08F, 0.00F, 0.0F);
  const int trunk_kink =
      add_ring(0.105F, 0.18F, 0.00F, 0.08F, 0.0F, QVector2D(0.010F, 0.006F));
  const int trunk_mid =
      add_ring(0.085F, 0.36F, 0.03F, 0.16F, 0.0F, QVector2D(0.022F, 0.012F));
  const int trunk_top =
      add_ring(0.075F, 0.56F, 0.08F, 0.26F, 0.0F, QVector2D(0.020F, 0.014F));

  const QVector2D t1o(-0.032F, 0.050F);
  const int c1_inner = add_ring(0.18F, 0.60F, 0.12F, 0.34F, 0.00F, t1o * 0.15F);
  const int c1_base = add_ring(0.34F, 0.65F, 0.24F, 0.42F, 0.60F, t1o * 0.40F);
  const int c1_outer = add_ring(0.50F, 0.70F, 0.34F, 0.50F, 1.00F, t1o);
  const int c1_mid = add_ring(0.45F, 0.77F, 0.56F, 0.58F, 0.74F, t1o * 0.55F);
  const int c1_top = add_ring(0.28F, 0.83F, 0.72F, 0.64F, 0.30F, t1o * 0.22F);

  const QVector2D t2o(0.040F, -0.026F);
  const int c2_inner = add_ring(0.15F, 0.85F, 0.16F, 0.66F, 0.00F, t2o * 0.12F);
  const int c2_base = add_ring(0.28F, 0.90F, 0.30F, 0.73F, 0.60F, t2o * 0.38F);
  const int c2_outer = add_ring(0.37F, 0.95F, 0.42F, 0.80F, 1.00F, t2o);
  const int c2_mid = add_ring(0.30F, 1.01F, 0.64F, 0.86F, 0.74F, t2o * 0.52F);
  const int c2_top = add_ring(0.18F, 1.06F, 0.78F, 0.90F, 0.30F, t2o * 0.20F);

  const QVector2D t3o(-0.022F, -0.032F);
  const int c3_inner = add_ring(0.10F, 1.08F, 0.20F, 0.91F, 0.00F, t3o * 0.10F);
  const int c3_base = add_ring(0.18F, 1.12F, 0.38F, 0.95F, 0.60F, t3o * 0.35F);
  const int c3_outer = add_ring(0.255F, 1.16F, 0.52F, 0.98F, 1.00F, t3o);
  const int c3_mid = add_ring(0.18F, 1.21F, 0.74F, 1.02F, 0.74F, t3o * 0.48F);
  const int c3_top = add_ring(0.09F, 1.25F, 0.86F, 1.06F, 0.30F, t3o * 0.15F);

  const int tip_ring = add_ring(0.03F, 1.30F, 0.95F, 1.12F, 0.20F);

  connect_rings(trunk_bottom, trunk_kink);
  connect_rings(trunk_kink, trunk_mid);
  connect_rings(trunk_mid, trunk_top);
  connect_rings(trunk_top, c1_inner);
  connect_rings(c1_inner, c1_base);
  connect_rings(c1_base, c1_outer);
  connect_rings(c1_outer, c1_mid);
  connect_rings(c1_mid, c1_top);
  connect_rings(c1_top, c2_inner);
  connect_rings(c2_inner, c2_base);
  connect_rings(c2_base, c2_outer);
  connect_rings(c2_outer, c2_mid);
  connect_rings(c2_mid, c2_top);
  connect_rings(c2_top, c3_inner);
  connect_rings(c3_inner, c3_base);
  connect_rings(c3_base, c3_outer);
  connect_rings(c3_outer, c3_mid);
  connect_rings(c3_mid, c3_top);
  connect_rings(c3_top, tip_ring);

  const auto trunk_cap_index = static_cast<unsigned short>(vertices.size());
  vertices.push_back({QVector3D(0.0F, 0.0F, 0.0F),
                      QVector3D(0.5F, 0.0F, 0.0F),
                      QVector3D(0.0F, -1.0F, 0.0F)});
  for (int i = 0; i < k_segments; ++i) {
    const int next = (i + 1) % k_segments;
    indices.push_back(static_cast<unsigned short>(trunk_bottom + next));
    indices.push_back(static_cast<unsigned short>(trunk_bottom + i));
    indices.push_back(trunk_cap_index);
  }

  const auto apex_index = static_cast<unsigned short>(vertices.size());
  vertices.push_back({QVector3D(0.0F, 1.36F, 0.0F),
                      QVector3D(0.5F, 1.18F, 0.30F),
                      QVector3D(0.0F, 1.0F, 0.0F)});
  for (int i = 0; i < k_segments; ++i) {
    const int next = (i + 1) % k_segments;
    indices.push_back(static_cast<unsigned short>(tip_ring + i));
    indices.push_back(static_cast<unsigned short>(tip_ring + next));
    indices.push_back(apex_index);
  }

  glGenVertexArrays(1, &m_pine_mesh.vao);
  glBindVertexArray(m_pine_mesh.vao);

  glGenBuffers(1, &m_pine_mesh.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_pine_mesh.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(PineVertex)),
               vertices.data(),
               GL_STATIC_DRAW);
  m_pine_mesh.vertex_count = static_cast<GLsizei>(vertices.size());

  glEnableVertexAttribArray(position);
  glVertexAttribPointer(position,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(PineVertex),
                        reinterpret_cast<void*>(offsetof(PineVertex, position)));

  glEnableVertexAttribArray(normal);
  glVertexAttribPointer(normal,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(PineVertex),
                        reinterpret_cast<void*>(offsetof(PineVertex, tex_coord)));

  glEnableVertexAttribArray(tex_coord);
  glVertexAttribPointer(tex_coord,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(PineVertex),
                        reinterpret_cast<void*>(offsetof(PineVertex, normal)));

  glGenBuffers(1, &m_pine_mesh.index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pine_mesh.index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned short)),
               indices.data(),
               GL_STATIC_DRAW);
  m_pine_mesh.index_count = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(instance_position);
  glVertexAttribDivisor(instance_position, 1);
  glEnableVertexAttribArray(instance_scale);
  glVertexAttribDivisor(instance_scale, 1);
  glEnableVertexAttribArray(instance_color);
  glVertexAttribDivisor(instance_color, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void VegetationPipeline::initialize_olive_pipeline() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_olive_mesh);

  struct OliveVertex {
    QVector3D position;
    QVector2D tex_coord;
    QVector3D normal;
  };

  constexpr int k_segments = olive_tree_segments;
  constexpr float k_two_pi = 6.28318530718F;

  std::vector<OliveVertex> vertices;
  vertices.reserve(k_segments * 96);
  std::vector<unsigned short> indices;
  indices.reserve(k_segments * 6 * 96);

  auto add_ring = [&](float radius,
                      float y,
                      float normal_up,
                      float v_coord,
                      const QVector2D& offset = QVector2D(0.0F, 0.0F)) -> int {
    const int start = static_cast<int>(vertices.size());
    for (int i = 0; i < k_segments; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(k_segments);
      const float angle = t * k_two_pi;
      const float nx = std::cos(angle);
      const float nz = std::sin(angle);
      QVector3D normal(nx, normal_up, nz);
      normal.normalize();
      QVector3D const position(radius * nx + offset.x(), y, radius * nz + offset.y());
      vertices.push_back({position, QVector2D(t, v_coord), normal});
    }
    return start;
  };

  auto connect_rings = [&](int lower, int upper) {
    for (int i = 0; i < k_segments; ++i) {
      const int next = (i + 1) % k_segments;
      indices.push_back(static_cast<unsigned short>(lower + i));
      indices.push_back(static_cast<unsigned short>(lower + next));
      indices.push_back(static_cast<unsigned short>(upper + next));
      indices.push_back(static_cast<unsigned short>(lower + i));
      indices.push_back(static_cast<unsigned short>(upper + next));
      indices.push_back(static_cast<unsigned short>(upper + i));
    }
  };

  auto add_cap = [&](int ring, float cap_y, const QVector2D& offset, float v) {
    const int top_idx = static_cast<int>(vertices.size());
    vertices.push_back({QVector3D(offset.x(), cap_y, offset.y()),
                        QVector2D(0.5F, v),
                        QVector3D(0.0F, 1.0F, 0.0F)});
    for (int i = 0; i < k_segments; ++i) {
      const int next = (i + 1) % k_segments;
      indices.push_back(static_cast<unsigned short>(ring + i));
      indices.push_back(static_cast<unsigned short>(ring + next));
      indices.push_back(static_cast<unsigned short>(top_idx));
    }
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

  m_olive_mesh.vertex_count = static_cast<GLsizei>(vertices.size());
  m_olive_mesh.index_count = static_cast<GLsizei>(indices.size());

  glGenVertexArrays(1, &m_olive_mesh.vao);
  glBindVertexArray(m_olive_mesh.vao);

  glGenBuffers(1, &m_olive_mesh.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_olive_mesh.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(OliveVertex)),
               vertices.data(),
               GL_STATIC_DRAW);

  glGenBuffers(1, &m_olive_mesh.index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_olive_mesh.index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned short)),
               indices.data(),
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0,
                        3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(OliveVertex),
                        reinterpret_cast<void*>(offsetof(OliveVertex, position)));

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1,
                        2,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(OliveVertex),
                        reinterpret_cast<void*>(offsetof(OliveVertex, tex_coord)));

  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2,
                        3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(OliveVertex),
                        reinterpret_cast<void*>(offsetof(OliveVertex, normal)));

  glEnableVertexAttribArray(instance_position);
  glVertexAttribDivisor(instance_position, 1);
  glEnableVertexAttribArray(instance_scale);
  glVertexAttribDivisor(instance_scale, 1);
  glEnableVertexAttribArray(instance_color);
  glVertexAttribDivisor(instance_color, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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

} // namespace Render::GL::BackendPipelines
