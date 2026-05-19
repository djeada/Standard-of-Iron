#include "vegetation_pipeline.h"

#include <QDebug>
#include <QOpenGLContext>
#include <qglobal.h>
#include <qopenglcontext.h>
#include <qopenglext.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <GL/gl.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../render_constants.h"
#include "dead_tree_mesh.h"
#include "gl/shader_cache.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::Geometry;

VegetationPipeline::VegetationPipeline(ShaderCache* shader_cache)
    : m_shader_cache(shader_cache) {
}

VegetationPipeline::~VegetationPipeline() {
  shutdown();
}

auto VegetationPipeline::initialize() -> bool {
  initializeOpenGLFunctions();

  if (m_shader_cache == nullptr) {
    return false;
  }

  m_stone_shader = m_shader_cache->get(QStringLiteral("stone_instanced"));
  m_plant_shader = m_shader_cache->get(QStringLiteral("plant_instanced"));
  m_pine_shader = m_shader_cache->get(QStringLiteral("pine_instanced"));
  m_olive_shader = m_shader_cache->get(QStringLiteral("olive_instanced"));
  m_firecamp_shader = m_shader_cache->get(QStringLiteral("firecamp"));
  m_tent_shader = m_shader_cache->get(QStringLiteral("tent_instanced"));
  m_supply_cart_shader = m_shader_cache->get(QStringLiteral("supply_cart_instanced"));
  m_weapon_rack_shader = m_shader_cache->get(QStringLiteral("weapon_rack_instanced"));
  m_ruins_shader = m_shader_cache->get(QStringLiteral("ruins_instanced"));
  m_dead_tree_shader = m_shader_cache->get(QStringLiteral("dead_tree_instanced"));

  m_iron_ore_shader = m_shader_cache->get(QStringLiteral("iron_ore_instanced"));

  m_magic_shrine_shader = m_shader_cache->get(QStringLiteral("magic_shrine_instanced"));

  if (m_stone_shader == nullptr) {
    qWarning() << "VegetationPipeline: stone shader missing";
  }
  if (m_plant_shader == nullptr) {
    qWarning() << "VegetationPipeline: plant shader missing";
  }
  if (m_pine_shader == nullptr) {
    qWarning() << "VegetationPipeline: pine shader missing";
  }
  if (m_olive_shader == nullptr) {
    qWarning() << "VegetationPipeline: olive shader missing";
  }
  if (m_firecamp_shader == nullptr) {
    qWarning() << "VegetationPipeline: firecamp shader missing";
  }
  if (m_tent_shader == nullptr) {
    qWarning() << "VegetationPipeline: tent_instanced shader missing";
  }
  if (m_supply_cart_shader == nullptr) {
    qWarning() << "VegetationPipeline: supply_cart_instanced shader missing";
  }
  if (m_weapon_rack_shader == nullptr) {
    qWarning() << "VegetationPipeline: weapon_rack_instanced shader missing";
  }
  if (m_ruins_shader == nullptr) {
    qWarning() << "VegetationPipeline: ruins_instanced shader missing";
  }
  if (m_dead_tree_shader == nullptr) {
    qWarning() << "VegetationPipeline: dead_tree_instanced shader missing";
  }
  if (m_iron_ore_shader == nullptr) {
    qWarning() << "VegetationPipeline: iron_ore_instanced shader missing";
  }
  if (m_magic_shrine_shader == nullptr) {
    qWarning() << "VegetationPipeline: magic_shrine_instanced shader missing";
  }

  initialize_stone_pipeline();
  initialize_plant_pipeline();
  initialize_pine_pipeline();
  initialize_olive_pipeline();
  initialize_fire_camp_pipeline();
  initialize_tent_pipeline();
  initialize_supply_cart_pipeline();
  initialize_weapon_rack_pipeline();
  initialize_ruins_pipeline();
  initialize_dead_tree_pipeline();
  initialize_iron_ore_pipeline();
  initialize_magic_shrine_pipeline();
  cache_uniforms();

  m_initialized = true;
  return true;
}

void VegetationPipeline::shutdown() {
  shutdown_stone_pipeline();
  shutdown_plant_pipeline();
  shutdown_pine_pipeline();
  shutdown_olive_pipeline();
  shutdown_fire_camp_pipeline();
  shutdown_tent_pipeline();
  shutdown_supply_cart_pipeline();
  shutdown_weapon_rack_pipeline();
  shutdown_ruins_pipeline();
  shutdown_dead_tree_pipeline();
  shutdown_iron_ore_pipeline();
  shutdown_magic_shrine_pipeline();
  m_initialized = false;
}

void VegetationPipeline::cache_uniforms() {
  if (m_stone_shader != nullptr) {
    m_stone_uniforms.view_proj = m_stone_shader->optional_uniform_handle("u_view_proj");
    m_stone_uniforms.light_direction =
        m_stone_shader->uniform_handle("u_light_direction");
  }

  if (m_plant_shader != nullptr) {
    m_plant_uniforms.view_proj = m_plant_shader->optional_uniform_handle("u_view_proj");
    m_plant_uniforms.time = m_plant_shader->uniform_handle("u_time");
    m_plant_uniforms.wind_strength = m_plant_shader->uniform_handle("u_wind_strength");
    m_plant_uniforms.wind_speed = m_plant_shader->uniform_handle("u_wind_speed");
    m_plant_uniforms.light_direction =
        m_plant_shader->uniform_handle("u_light_direction");
  }

  if (m_pine_shader != nullptr) {
    m_pine_uniforms.view_proj = m_pine_shader->optional_uniform_handle("u_view_proj");
    m_pine_uniforms.time = m_pine_shader->uniform_handle("u_time");
    m_pine_uniforms.wind_strength = m_pine_shader->uniform_handle("u_wind_strength");
    m_pine_uniforms.wind_speed = m_pine_shader->uniform_handle("u_wind_speed");
    m_pine_uniforms.light_direction =
        m_pine_shader->uniform_handle("u_light_direction");
  }

  if (m_olive_shader != nullptr) {
    m_olive_uniforms.view_proj = m_olive_shader->optional_uniform_handle("u_view_proj");
    m_olive_uniforms.time = m_olive_shader->uniform_handle("u_time");
    m_olive_uniforms.wind_strength = m_olive_shader->uniform_handle("u_wind_strength");
    m_olive_uniforms.wind_speed = m_olive_shader->uniform_handle("u_wind_speed");
    m_olive_uniforms.light_direction =
        m_olive_shader->uniform_handle("u_light_direction");
  }

  if (m_firecamp_shader != nullptr) {
    m_firecamp_uniforms.view_proj =
        m_firecamp_shader->optional_uniform_handle("u_view_proj");
    m_firecamp_uniforms.time = m_firecamp_shader->uniform_handle("u_time");
    m_firecamp_uniforms.flicker_speed =
        m_firecamp_shader->uniform_handle("u_flicker_speed");
    m_firecamp_uniforms.flicker_amount =
        m_firecamp_shader->uniform_handle("u_flicker_amount");
    m_firecamp_uniforms.glow_strength =
        m_firecamp_shader->uniform_handle("u_glow_strength");
    m_firecamp_uniforms.fire_texture =
        m_firecamp_shader->uniform_handle("fire_texture");
    m_firecamp_uniforms.camera_right =
        m_firecamp_shader->uniform_handle("u_camera_right");
    m_firecamp_uniforms.camera_forward =
        m_firecamp_shader->uniform_handle("u_camera_forward");
  }

  auto cache_prop_uniforms = [&](PropUniforms& u, GL::Shader* shader) {
    if (shader == nullptr) {
      return;
    }
    u.view_proj = shader->optional_uniform_handle("u_view_proj");
    u.light_direction = shader->uniform_handle("u_light_direction");
  };

  cache_prop_uniforms(m_tent_uniforms, m_tent_shader);
  cache_prop_uniforms(m_supply_cart_uniforms, m_supply_cart_shader);
  cache_prop_uniforms(m_weapon_rack_uniforms, m_weapon_rack_shader);
  cache_prop_uniforms(m_ruins_uniforms, m_ruins_shader);
  cache_prop_uniforms(m_dead_tree_uniforms, m_dead_tree_shader);
  cache_prop_uniforms(m_iron_ore_uniforms, m_iron_ore_shader);
  cache_prop_uniforms(m_magic_shrine_uniforms, m_magic_shrine_shader);
}

void VegetationPipeline::initialize_stone_pipeline() {
  initializeOpenGLFunctions();
  shutdown_stone_pipeline();

  struct StoneVertex {
    QVector3D position;
    QVector3D normal;
  };

  using F = StoneVertex;
  std::vector<F> verts;
  std::vector<uint16_t> idx;

  constexpr int k_n = 8;
  constexpr float k_tau = 6.28318530F;
  constexpr int k_rings = 4;

  constexpr float k_perturb_amount = 0.07F;
  constexpr float k_perturb_freq_vertex = 2.3F;
  constexpr float k_perturb_freq_ring = 1.7F;

  constexpr float k_y_jitter_amount = 0.025F;
  constexpr float k_y_jitter_freq_vertex = 1.9F;
  constexpr float k_y_jitter_freq_ring = 0.9F;

  constexpr float k_apex_offset_x = 0.02F;
  constexpr float k_apex_height = 0.64F;
  constexpr float k_apex_offset_z = -0.04F;

  struct Ring {
    float y;
    float radius;
    float phase;
    float sx;
    float sz;
  };
  const Ring rings[] = {
      {0.00F, 0.38F, 0.12F, 1.00F, 1.10F},
      {0.22F, 0.52F, 0.00F, 1.10F, 1.00F},
      {0.40F, 0.38F, 0.20F, 0.95F, 1.05F},
      {0.54F, 0.20F, 0.08F, 1.00F, 0.92F},
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
      ring_pts[ri][i] = QVector3D(rx, ry, rz);
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

  glGenVertexArrays(1, &m_stone_vao);
  glBindVertexArray(m_stone_vao);

  glGenBuffers(1, &m_stone_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_stone_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(verts.size() * sizeof(StoneVertex)),
               verts.data(),
               GL_STATIC_DRAW);
  m_stone_vertex_count = static_cast<GLsizei>(verts.size());

  glGenBuffers(1, &m_stone_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_stone_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(idx.size() * sizeof(uint16_t)),
               idx.data(),
               GL_STATIC_DRAW);
  m_stone_index_count = static_cast<GLsizei>(idx.size());

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

void VegetationPipeline::shutdown_stone_pipeline() {

  if (QOpenGLContext::currentContext() == nullptr) {
    m_stone_vao = 0;
    m_stone_vertex_buffer = 0;
    m_stone_index_buffer = 0;
    m_stone_vertex_count = 0;
    m_stone_index_count = 0;
    return;
  }

  initializeOpenGLFunctions();
  if (m_stone_index_buffer != 0U) {
    glDeleteBuffers(1, &m_stone_index_buffer);
    m_stone_index_buffer = 0;
  }
  if (m_stone_vertex_buffer != 0U) {
    glDeleteBuffers(1, &m_stone_vertex_buffer);
    m_stone_vertex_buffer = 0;
  }
  if (m_stone_vao != 0U) {
    glDeleteVertexArrays(1, &m_stone_vao);
    m_stone_vao = 0;
  }
  m_stone_vertex_count = 0;
  m_stone_index_count = 0;
}

void VegetationPipeline::initialize_plant_pipeline() {
  initializeOpenGLFunctions();
  shutdown_plant_pipeline();

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

  glGenVertexArrays(1, &m_plant_vao);
  glBindVertexArray(m_plant_vao);

  glGenBuffers(1, &m_plant_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_plant_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(plant_vertices), plant_vertices, GL_STATIC_DRAW);
  m_plant_vertex_count = plant_cross_quad_vertex_count;

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

  glGenBuffers(1, &m_plant_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_plant_index_buffer);
  glBufferData(
      GL_ELEMENT_ARRAY_BUFFER, sizeof(plant_indices), plant_indices, GL_STATIC_DRAW);
  m_plant_index_count = plant_cross_quad_index_count;

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

void VegetationPipeline::shutdown_plant_pipeline() {

  if (QOpenGLContext::currentContext() == nullptr) {
    m_plant_vao = 0;
    m_plant_vertex_buffer = 0;
    m_plant_index_buffer = 0;
    m_plant_vertex_count = 0;
    m_plant_index_count = 0;
    return;
  }

  initializeOpenGLFunctions();
  if (m_plant_index_buffer != 0U) {
    glDeleteBuffers(1, &m_plant_index_buffer);
    m_plant_index_buffer = 0;
  }
  if (m_plant_vertex_buffer != 0U) {
    glDeleteBuffers(1, &m_plant_vertex_buffer);
    m_plant_vertex_buffer = 0;
  }
  if (m_plant_vao != 0U) {
    glDeleteVertexArrays(1, &m_plant_vao);
    m_plant_vao = 0;
  }
  m_plant_vertex_count = 0;
  m_plant_index_count = 0;
}

void VegetationPipeline::initialize_pine_pipeline() {
  initializeOpenGLFunctions();
  shutdown_pine_pipeline();

  struct PineVertex {
    QVector3D position;
    QVector2D tex_coord;
    QVector3D normal;
  };

  constexpr int k_segments = 8;
  constexpr float k_two_pi = 6.28318530718F;

  std::vector<PineVertex> vertices;
  vertices.reserve(22 * k_segments + 2);

  std::vector<unsigned short> indices;
  indices.reserve(22 * k_segments * 6);

  auto add_ring = [&](float radius,
                      float y,
                      float normal_up,
                      float v_coord,
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
      QVector2D const tex_coord(t, v_coord);
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

  const int trunk_bottom = add_ring(0.11F, 0.00F, -0.04F, 0.00F);
  const int trunk_kink =
      add_ring(0.10F, 0.18F, 0.00F, 0.08F, QVector2D(0.010F, 0.006F));
  const int trunk_mid = add_ring(0.09F, 0.36F, 0.03F, 0.16F, QVector2D(0.018F, 0.010F));
  const int trunk_top =
      add_ring(0.075F, 0.56F, 0.08F, 0.26F, QVector2D(0.020F, 0.014F));

  const QVector2D t1o(-0.032F, 0.050F);
  const int c1_inner = add_ring(0.18F, 0.60F, 0.12F, 0.34F, t1o * 0.15F);
  const int c1_base = add_ring(0.34F, 0.65F, 0.24F, 0.42F, t1o * 0.40F);
  const int c1_outer = add_ring(0.54F, 0.70F, 0.38F, 0.50F, t1o);
  const int c1_mid = add_ring(0.45F, 0.77F, 0.56F, 0.58F, t1o * 0.55F);
  const int c1_top = add_ring(0.28F, 0.83F, 0.72F, 0.64F, t1o * 0.22F);

  const QVector2D t2o(0.040F, -0.026F);
  const int c2_inner = add_ring(0.15F, 0.85F, 0.16F, 0.66F, t2o * 0.12F);
  const int c2_base = add_ring(0.28F, 0.90F, 0.30F, 0.73F, t2o * 0.38F);
  const int c2_outer = add_ring(0.40F, 0.95F, 0.46F, 0.80F, t2o);
  const int c2_mid = add_ring(0.30F, 1.01F, 0.64F, 0.86F, t2o * 0.52F);
  const int c2_top = add_ring(0.18F, 1.06F, 0.78F, 0.90F, t2o * 0.20F);

  const QVector2D t3o(-0.022F, -0.032F);
  const int c3_inner = add_ring(0.10F, 1.08F, 0.20F, 0.91F, t3o * 0.10F);
  const int c3_base = add_ring(0.18F, 1.12F, 0.38F, 0.95F, t3o * 0.35F);
  const int c3_outer = add_ring(0.28F, 1.16F, 0.56F, 0.98F, t3o);
  const int c3_mid = add_ring(0.18F, 1.21F, 0.74F, 1.02F, t3o * 0.48F);
  const int c3_top = add_ring(0.09F, 1.25F, 0.86F, 1.06F, t3o * 0.15F);

  const int tip_ring = add_ring(0.03F, 1.30F, 0.95F, 1.12F);

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
                      QVector2D(0.5F, 0.0F),
                      QVector3D(0.0F, -1.0F, 0.0F)});
  for (int i = 0; i < k_segments; ++i) {
    const int next = (i + 1) % k_segments;
    indices.push_back(static_cast<unsigned short>(trunk_bottom + next));
    indices.push_back(static_cast<unsigned short>(trunk_bottom + i));
    indices.push_back(trunk_cap_index);
  }

  const auto apex_index = static_cast<unsigned short>(vertices.size());
  vertices.push_back({QVector3D(0.0F, 1.36F, 0.0F),
                      QVector2D(0.5F, 1.18F),
                      QVector3D(0.0F, 1.0F, 0.0F)});
  for (int i = 0; i < k_segments; ++i) {
    const int next = (i + 1) % k_segments;
    indices.push_back(static_cast<unsigned short>(tip_ring + i));
    indices.push_back(static_cast<unsigned short>(tip_ring + next));
    indices.push_back(apex_index);
  }

  glGenVertexArrays(1, &m_pine_vao);
  glBindVertexArray(m_pine_vao);

  glGenBuffers(1, &m_pine_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_pine_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(PineVertex)),
               vertices.data(),
               GL_STATIC_DRAW);
  m_pine_vertex_count = static_cast<GLsizei>(vertices.size());

  glEnableVertexAttribArray(position);
  glVertexAttribPointer(position,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(PineVertex),
                        reinterpret_cast<void*>(offsetof(PineVertex, position)));

  glEnableVertexAttribArray(normal);
  glVertexAttribPointer(normal,
                        vec2,
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

  glGenBuffers(1, &m_pine_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pine_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned short)),
               indices.data(),
               GL_STATIC_DRAW);
  m_pine_index_count = static_cast<GLsizei>(indices.size());

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

void VegetationPipeline::shutdown_pine_pipeline() {

  if (QOpenGLContext::currentContext() == nullptr) {
    m_pine_vao = 0;
    m_pine_vertex_buffer = 0;
    m_pine_index_buffer = 0;
    m_pine_vertex_count = 0;
    m_pine_index_count = 0;
    return;
  }

  initializeOpenGLFunctions();
  if (m_pine_index_buffer != 0U) {
    glDeleteBuffers(1, &m_pine_index_buffer);
    m_pine_index_buffer = 0;
  }
  if (m_pine_vertex_buffer != 0U) {
    glDeleteBuffers(1, &m_pine_vertex_buffer);
    m_pine_vertex_buffer = 0;
  }
  if (m_pine_vao != 0U) {
    glDeleteVertexArrays(1, &m_pine_vao);
    m_pine_vao = 0;
  }
  m_pine_vertex_count = 0;
  m_pine_index_count = 0;
}

void VegetationPipeline::initialize_olive_pipeline() {
  initializeOpenGLFunctions();
  shutdown_olive_pipeline();

  struct OliveVertex {
    QVector3D position;
    QVector2D tex_coord;
    QVector3D normal;
  };

  constexpr int k_segments = olive_tree_segments;
  constexpr float k_two_pi = 6.28318530718F;

  std::vector<OliveVertex> vertices;
  vertices.reserve(k_segments * 56);
  std::vector<unsigned short> indices;
  indices.reserve(k_segments * 6 * 56);

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

  int const t0 = add_ring(0.14F, 0.00F, -0.2F, 0.00F);
  int const t1 = add_ring(0.12F, 0.08F, 0.0F, 0.06F);
  int const t2 = add_ring(0.09F, 0.15F, 0.1F, 0.12F);
  connect_rings(t0, t1);
  connect_rings(t1, t2);

  auto add_branch = [&](float dir_x,
                        float dir_z,
                        float base_y,
                        float length,
                        float branch_r,
                        float leaf_r,
                        float v_start) {
    float const len = std::sqrt(dir_x * dir_x + dir_z * dir_z);
    dir_x /= len;
    dir_z /= len;

    float const rise = 0.5F;
    float const dx = dir_x * std::cos(0.5F);
    float const dz = dir_z * std::cos(0.5F);
    float const dy = std::sin(0.4F);

    int const b0 = add_ring(branch_r, base_y, 0.0F, v_start);

    float const mid_dist = length * 0.5F;
    QVector2D const mid_offset(dx * mid_dist, dz * mid_dist);
    int const b1 = add_ring(
        branch_r * 0.6F, base_y + dy * mid_dist, 0.3F, v_start + 0.1F, mid_offset);

    float const tip_dist = length;
    QVector2D const tip_offset(dx * tip_dist, dz * tip_dist);
    float const tip_y = base_y + dy * tip_dist;
    int const b2 = add_ring(branch_r * 0.3F, tip_y, 0.5F, v_start + 0.2F, tip_offset);

    connect_rings(b0, b1);
    connect_rings(b1, b2);

    float const ly = tip_y - leaf_r * 0.28F;
    int const l0 = add_ring(leaf_r * 0.42F, ly, -0.5F, 0.50F, tip_offset);
    int const l1 =
        add_ring(leaf_r * 0.95F, ly + leaf_r * 0.26F, -0.1F, 0.62F, tip_offset);
    int const l2 =
        add_ring(leaf_r * 1.05F, ly + leaf_r * 0.54F, 0.2F, 0.76F, tip_offset);
    int const l3 =
        add_ring(leaf_r * 0.72F, ly + leaf_r * 0.82F, 0.6F, 0.91F, tip_offset);

    connect_rings(b2, l0);
    connect_rings(l0, l1);
    connect_rings(l1, l2);
    connect_rings(l2, l3);
    add_cap(l3, ly + leaf_r * 1.0F, tip_offset, 1.0F);
  };

  add_branch(0.8F, 0.3F, 0.14F, 0.32F, 0.025F, 0.20F, 0.18F);
  add_branch(-0.7F, 0.5F, 0.15F, 0.34F, 0.022F, 0.22F, 0.20F);
  add_branch(0.4F, -0.9F, 0.16F, 0.31F, 0.020F, 0.18F, 0.22F);
  add_branch(-0.5F, -0.7F, 0.14F, 0.36F, 0.024F, 0.21F, 0.19F);
  add_branch(0.95F, -0.25F, 0.17F, 0.24F, 0.018F, 0.17F, 0.24F);
  add_branch(-0.2F, 0.95F, 0.16F, 0.25F, 0.018F, 0.18F, 0.23F);

  m_olive_vertex_count = static_cast<GLsizei>(vertices.size());
  m_olive_index_count = static_cast<GLsizei>(indices.size());

  glGenVertexArrays(1, &m_olive_vao);
  glBindVertexArray(m_olive_vao);

  glGenBuffers(1, &m_olive_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_olive_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(OliveVertex)),
               vertices.data(),
               GL_STATIC_DRAW);

  glGenBuffers(1, &m_olive_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_olive_index_buffer);
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

void VegetationPipeline::shutdown_olive_pipeline() {

  if (QOpenGLContext::currentContext() == nullptr) {
    m_olive_vao = 0;
    m_olive_vertex_buffer = 0;
    m_olive_index_buffer = 0;
    m_olive_vertex_count = 0;
    m_olive_index_count = 0;
    return;
  }

  initializeOpenGLFunctions();
  if (m_olive_index_buffer != 0U) {
    glDeleteBuffers(1, &m_olive_index_buffer);
    m_olive_index_buffer = 0;
  }
  if (m_olive_vertex_buffer != 0U) {
    glDeleteBuffers(1, &m_olive_vertex_buffer);
    m_olive_vertex_buffer = 0;
  }
  if (m_olive_vao != 0U) {
    glDeleteVertexArrays(1, &m_olive_vao);
    m_olive_vao = 0;
  }
  m_olive_vertex_count = 0;
  m_olive_index_count = 0;
}

void VegetationPipeline::initialize_fire_camp_pipeline() {
  initializeOpenGLFunctions();
  shutdown_fire_camp_pipeline();

  struct FireCampVertex {
    QVector3D position;
    QVector2D tex_coord;
  };

  constexpr std::size_t k_firecamp_vertex_reserve = 12;
  constexpr std::size_t k_firecamp_index_reserve = 18;
  std::vector<FireCampVertex> vertices;
  vertices.reserve(k_firecamp_vertex_reserve);
  std::vector<unsigned short> indices;
  indices.reserve(k_firecamp_index_reserve);

  auto append_plane = [&](float plane_index) {
    auto const base = static_cast<unsigned short>(vertices.size());
    vertices.push_back({QVector3D(-1.0F, 0.0F, plane_index), QVector2D(0.0F, 0.0F)});
    vertices.push_back({QVector3D(1.0F, 0.0F, plane_index), QVector2D(1.0F, 0.0F)});
    vertices.push_back({QVector3D(1.0F, 2.0F, plane_index), QVector2D(1.0F, 1.0F)});
    vertices.push_back({QVector3D(-1.0F, 2.0F, plane_index), QVector2D(0.0F, 1.0F)});

    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
  };

  append_plane(0.0F);
  append_plane(1.0F);
  append_plane(2.0F);

  glGenVertexArrays(1, &m_firecamp_vao);
  glBindVertexArray(m_firecamp_vao);

  glGenBuffers(1, &m_firecamp_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_firecamp_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(FireCampVertex)),
               vertices.data(),
               GL_STATIC_DRAW);
  m_firecamp_vertex_count = static_cast<GLsizei>(vertices.size());

  glEnableVertexAttribArray(position);
  glVertexAttribPointer(position,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(FireCampVertex),
                        reinterpret_cast<void*>(0));

  glEnableVertexAttribArray(normal);
  glVertexAttribPointer(normal,
                        vec2,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(FireCampVertex),
                        reinterpret_cast<void*>(offsetof(FireCampVertex, tex_coord)));

  glGenBuffers(1, &m_firecamp_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_firecamp_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned short)),
               indices.data(),
               GL_STATIC_DRAW);
  m_firecamp_index_count = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(instance_position);
  glVertexAttribDivisor(instance_position, 1);

  glEnableVertexAttribArray(instance_scale);
  glVertexAttribDivisor(instance_scale, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void VegetationPipeline::shutdown_fire_camp_pipeline() {

  if (QOpenGLContext::currentContext() == nullptr) {
    m_firecamp_vao = 0;
    m_firecamp_vertex_buffer = 0;
    m_firecamp_index_buffer = 0;
    m_firecamp_vertex_count = 0;
    m_firecamp_index_count = 0;
    return;
  }

  initializeOpenGLFunctions();
  if (m_firecamp_index_buffer != 0U) {
    glDeleteBuffers(1, &m_firecamp_index_buffer);
    m_firecamp_index_buffer = 0;
  }
  if (m_firecamp_vertex_buffer != 0U) {
    glDeleteBuffers(1, &m_firecamp_vertex_buffer);
    m_firecamp_vertex_buffer = 0;
  }
  if (m_firecamp_vao != 0U) {
    glDeleteVertexArrays(1, &m_firecamp_vao);
    m_firecamp_vao = 0;
  }
  m_firecamp_vertex_count = 0;
  m_firecamp_index_count = 0;
}

void VegetationPipeline::upload_prop_mesh_impl(
    const std::vector<std::pair<QVector3D, QVector3D>>& verts,
    const std::vector<uint16_t>& idx,
    GLuint& vao,
    GLuint& vbo,
    GLuint& ibo,
    GLsizei& vert_count,
    GLsizei& idx_count) {

  using namespace Render::GL::VertexAttrib;
  using namespace Render::GL::ComponentCount;

  struct V {
    QVector3D pos;
    QVector3D nrm;
  };
  std::vector<V> flat;
  flat.reserve(verts.size());
  for (const auto& [p, n] : verts) {
    flat.push_back({p, n});
  }

  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(flat.size() * sizeof(V)),
               flat.data(),
               GL_STATIC_DRAW);
  vert_count = static_cast<GLsizei>(flat.size());

  glEnableVertexAttribArray(position);
  glVertexAttribPointer(position,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(V),
                        reinterpret_cast<void*>(offsetof(V, pos)));
  glEnableVertexAttribArray(normal);
  glVertexAttribPointer(normal,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(V),
                        reinterpret_cast<void*>(offsetof(V, nrm)));

  glGenBuffers(1, &ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(idx.size() * sizeof(uint16_t)),
               idx.data(),
               GL_STATIC_DRAW);
  idx_count = static_cast<GLsizei>(idx.size());

  glEnableVertexAttribArray(tex_coord);
  glVertexAttribDivisor(tex_coord, 1);
  glEnableVertexAttribArray(instance_position);
  glVertexAttribDivisor(instance_position, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void VegetationPipeline::delete_prop_pipeline_impl(
    GLuint& vao, GLuint& vbo, GLuint& ibo, GLsizei& vc, GLsizei& ic) {
  if (ibo != 0U) {
    glDeleteBuffers(1, &ibo);
    ibo = 0;
  }
  if (vbo != 0U) {
    glDeleteBuffers(1, &vbo);
    vbo = 0;
  }
  if (vao != 0U) {
    glDeleteVertexArrays(1, &vao);
    vao = 0;
  }
  vc = 0;
  ic = 0;
}

static void append_box(std::vector<std::pair<QVector3D, QVector3D>>& verts,
                       std::vector<uint16_t>& idx,
                       const QVector3D& lo,
                       const QVector3D& hi) {

  using F = std::pair<QVector3D, QVector3D>;
  const float x0 = lo.x();
  const float y0 = lo.y();
  const float z0 = lo.z();
  const float x1 = hi.x();
  const float y1 = hi.y();
  const float z1 = hi.z();

  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D const n(0, 0, 1);
    verts.insert(verts.end(),
                 {F{{x0, y0, z1}, n},
                  F{{x1, y0, z1}, n},
                  F{{x1, y1, z1}, n},
                  F{{x0, y1, z1}, n}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D const n(0, 0, -1);
    verts.insert(verts.end(),
                 {F{{x1, y0, z0}, n},
                  F{{x0, y0, z0}, n},
                  F{{x0, y1, z0}, n},
                  F{{x1, y1, z0}, n}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D const n(1, 0, 0);
    verts.insert(verts.end(),
                 {F{{x1, y0, z0}, n},
                  F{{x1, y0, z1}, n},
                  F{{x1, y1, z1}, n},
                  F{{x1, y1, z0}, n}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D const n(-1, 0, 0);
    verts.insert(verts.end(),
                 {F{{x0, y0, z1}, n},
                  F{{x0, y0, z0}, n},
                  F{{x0, y1, z0}, n},
                  F{{x0, y1, z1}, n}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D const n(0, 1, 0);
    verts.insert(verts.end(),
                 {F{{x0, y1, z0}, n},
                  F{{x0, y1, z1}, n},
                  F{{x1, y1, z1}, n},
                  F{{x1, y1, z0}, n}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D const n(0, -1, 0);
    verts.insert(verts.end(),
                 {F{{x0, y0, z1}, n},
                  F{{x0, y0, z0}, n},
                  F{{x1, y0, z0}, n},
                  F{{x1, y0, z1}, n}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }
}

static void append_quad(std::vector<std::pair<QVector3D, QVector3D>>& verts,
                        std::vector<uint16_t>& idx,
                        const QVector3D& p0,
                        const QVector3D& p1,
                        const QVector3D& p2,
                        const QVector3D& p3,
                        const QVector3D& n) {
  using F = std::pair<QVector3D, QVector3D>;
  auto b = static_cast<uint16_t>(verts.size());
  verts.insert(verts.end(), {F{p0, n}, F{p1, n}, F{p2, n}, F{p3, n}});
  idx.insert(
      idx.end(),
      {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
}

static void append_oriented_box(std::vector<std::pair<QVector3D, QVector3D>>& verts,
                                std::vector<uint16_t>& idx,
                                const QVector3D& a,
                                const QVector3D& b,
                                float half_width,
                                float half_depth) {
  QVector3D const axis = b - a;
  QVector3D side(-axis.y(), axis.x(), 0.0F);
  if (side.lengthSquared() < 1.0e-8F) {
    side = {1.0F, 0.0F, 0.0F};
  } else {
    side.normalize();
  }
  side *= half_width;
  const QVector3D depth(0.0F, 0.0F, half_depth);

  const QVector3D a0 = a - side - depth;
  const QVector3D a1 = a + side - depth;
  const QVector3D a2 = a + side + depth;
  const QVector3D a3 = a - side + depth;
  const QVector3D b0 = b - side - depth;
  const QVector3D b1 = b + side - depth;
  const QVector3D b2 = b + side + depth;
  const QVector3D b3 = b - side + depth;

  auto face = [&](const QVector3D& p0,
                  const QVector3D& p1,
                  const QVector3D& p2,
                  const QVector3D& p3) {
    QVector3D n = QVector3D::crossProduct(p1 - p0, p3 - p0);
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

static void append_disc_xaxis(std::vector<std::pair<QVector3D, QVector3D>>& verts,
                              std::vector<uint16_t>& idx,
                              float cx,
                              float cy,
                              float cz,
                              float r,
                              float hthk,
                              int segs) {
  using F = std::pair<QVector3D, QVector3D>;
  constexpr float k_tau = 6.28318530F;
  const float xa = cx - hthk;
  const float xb = cx + hthk;
  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * i / segs;
    float const a1 = k_tau * (i + 1) / segs;
    float const y0 = cy + r * std::sin(a0);
    float const z0_ = cz + r * std::cos(a0);
    float const y1 = cy + r * std::sin(a1);
    float const z1_ = cz + r * std::cos(a1);
    float const amid = (a0 + a1) * 0.5F;
    QVector3D const n(0.0F, std::sin(amid), std::cos(amid));
    append_quad(
        verts, idx, {xa, y0, z0_}, {xa, y1, z1_}, {xb, y1, z1_}, {xb, y0, z0_}, n);
  }

  QVector3D const ncf(1, 0, 0);
  QVector3D const cf{xb, cy, cz};
  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * i / segs;
    float const a1 = k_tau * (i + 1) / segs;
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {F{cf, ncf},
                  F{{xb, cy + r * std::sin(a0), cz + r * std::cos(a0)}, ncf},
                  F{{xb, cy + r * std::sin(a1), cz + r * std::cos(a1)}, ncf}});
    idx.insert(idx.end(), {b, uint16_t(b + 1), uint16_t(b + 2)});
  }

  QVector3D const ncb(-1, 0, 0);
  QVector3D const cb{xa, cy, cz};
  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * i / segs;
    float const a1 = k_tau * (i + 1) / segs;
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {F{cb, ncb},
                  F{{xa, cy + r * std::sin(a1), cz + r * std::cos(a1)}, ncb},
                  F{{xa, cy + r * std::sin(a0), cz + r * std::cos(a0)}, ncb}});
    idx.insert(idx.end(), {b, uint16_t(b + 1), uint16_t(b + 2)});
  }
}

static void append_vert_prism(std::vector<std::pair<QVector3D, QVector3D>>& verts,
                              std::vector<uint16_t>& idx,
                              float cx,
                              float y0,
                              float cz,
                              float r,
                              float height,
                              int segs) {
  using F = std::pair<QVector3D, QVector3D>;
  constexpr float k_tau = 6.28318530F;
  const float y1 = y0 + height;
  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * i / segs;
    float const a1 = k_tau * (i + 1) / segs;
    float const x0 = cx + r * std::cos(a0);
    float const z0_ = cz + r * std::sin(a0);
    float const x1 = cx + r * std::cos(a1);
    float const z1_ = cz + r * std::sin(a1);
    float const amid = (a0 + a1) * 0.5F;
    QVector3D const n(std::cos(amid), 0.0F, std::sin(amid));
    append_quad(
        verts, idx, {x0, y0, z0_}, {x1, y0, z1_}, {x1, y1, z1_}, {x0, y1, z0_}, n);
  }

  QVector3D const tn(0, 1, 0);
  QVector3D const tc{cx, y1, cz};
  for (int i = 0; i < segs; ++i) {
    float const a0 = k_tau * i / segs;
    float const a1 = k_tau * (i + 1) / segs;
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(),
                 {F{tc, tn},
                  F{{cx + r * std::cos(a0), y1, cz + r * std::sin(a0)}, tn},
                  F{{cx + r * std::cos(a1), y1, cz + r * std::sin(a1)}, tn}});
    idx.insert(idx.end(), {b, uint16_t(b + 1), uint16_t(b + 2)});
  }
}

void VegetationPipeline::initialize_tent_pipeline() {
  initializeOpenGLFunctions();
  shutdown_tent_pipeline();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  constexpr float H = 0.88F;
  constexpr float W = 0.62F;
  constexpr float Dp = 0.60F;

  const QVector3D A(-W, 0.0F, -Dp);
  const QVector3D B(W, 0.0F, -Dp);
  const QVector3D C(0.0F, H, -Dp);
  const QVector3D D(-W, 0.0F, Dp);
  const QVector3D E(W, 0.0F, Dp);
  const QVector3D F(0.0F, H, Dp);

  constexpr float inv_sqrt2 = 0.70711F;
  const QVector3D nL(-inv_sqrt2, inv_sqrt2, 0.0F);
  const QVector3D nR(inv_sqrt2, inv_sqrt2, 0.0F);

  using P = std::pair<QVector3D, QVector3D>;

  {
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{A, nL}, P{D, nL}, P{F, nL}, P{C, nL}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{B, nR}, P{C, nR}, P{F, nR}, P{E, nR}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});
  }

  {
    const QVector3D nF(0.0F, 0.0F, -1.0F);
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{A, nF}, P{C, nF}, P{B, nF}});
    idx.insert(idx.end(), {b, uint16_t(b + 1), uint16_t(b + 2)});
  }

  {
    const QVector3D nBk(0.0F, 0.0F, 1.0F);
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{D, nBk}, P{E, nBk}, P{F, nBk}});
    idx.insert(idx.end(), {b, uint16_t(b + 1), uint16_t(b + 2)});
  }

  append_box(verts, idx, {-W, -0.02F, -Dp}, {W, 0.00F, Dp});

  append_box(verts, idx, {-0.030F, 0.00F, -0.035F}, {0.030F, H * 0.90F, 0.035F});

  append_box(verts, idx, {-0.24F, 0.00F, -Dp - 0.02F}, {-0.16F, 0.44F, -Dp + 0.02F});
  append_box(verts, idx, {0.16F, 0.00F, -Dp - 0.02F}, {0.24F, 0.44F, -Dp + 0.02F});
  append_box(verts, idx, {-0.24F, 0.41F, -Dp - 0.02F}, {0.24F, 0.47F, -Dp + 0.02F});

  {
    constexpr float aw_ext = 0.30F;
    constexpr float aw_y = H * 0.46F;
    constexpr float inv_aw = 0.83205F;
    const QVector3D nAw(0.0F, inv_aw, -inv_aw);

    const QVector3D al(-W * 0.72F, aw_y, -Dp);
    const QVector3D ar(W * 0.72F, aw_y, -Dp);
    const QVector3D bl(-W * 0.72F, 0.04F, -Dp - aw_ext);
    const QVector3D br(W * 0.72F, 0.04F, -Dp - aw_ext);

    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{al, nAw}, P{ar, nAw}, P{br, nAw}, P{bl, nAw}});
    idx.insert(
        idx.end(),
        {b, uint16_t(b + 1), uint16_t(b + 2), b, uint16_t(b + 2), uint16_t(b + 3)});

    const QVector3D nAwU(0.0F, -inv_aw, inv_aw);
    auto bu = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{bl, nAwU}, P{br, nAwU}, P{ar, nAwU}, P{al, nAwU}});
    idx.insert(idx.end(),
               {bu,
                uint16_t(bu + 1),
                uint16_t(bu + 2),
                bu,
                uint16_t(bu + 2),
                uint16_t(bu + 3)});

    append_box(verts,
               idx,
               {-W * 0.72F - 0.025F, 0.00F, -Dp - aw_ext},
               {-W * 0.72F + 0.025F, aw_y, -Dp - aw_ext + 0.025F});
    append_box(verts,
               idx,
               {W * 0.72F - 0.025F, 0.00F, -Dp - aw_ext},
               {W * 0.72F + 0.025F, aw_y, -Dp - aw_ext + 0.025F});
  }

  constexpr float sk = 0.07F;
  append_box(verts, idx, {-W - sk, 0.00F, -Dp - sk}, {-W, 0.07F, -Dp});
  append_box(verts, idx, {W, 0.00F, -Dp - sk}, {W + sk, 0.07F, -Dp});
  append_box(verts, idx, {-W - sk, 0.00F, Dp}, {-W, 0.07F, Dp + sk});
  append_box(verts, idx, {W, 0.00F, Dp}, {W + sk, 0.07F, Dp + sk});

  upload_prop_mesh_impl(verts,
                        idx,
                        m_tent_vao,
                        m_tent_vertex_buffer,
                        m_tent_index_buffer,
                        m_tent_vertex_count,
                        m_tent_index_count);
}

void VegetationPipeline::shutdown_tent_pipeline() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_tent_vao = m_tent_vertex_buffer = m_tent_index_buffer = 0;
    m_tent_vertex_count = m_tent_index_count = 0;
    return;
  }
  initializeOpenGLFunctions();
  delete_prop_pipeline_impl(m_tent_vao,
                            m_tent_vertex_buffer,
                            m_tent_index_buffer,
                            m_tent_vertex_count,
                            m_tent_index_count);
}

void VegetationPipeline::initialize_supply_cart_pipeline() {
  initializeOpenGLFunctions();
  shutdown_supply_cart_pipeline();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  constexpr int k_wheel_sides = 12;
  constexpr float k_front_wheel_r = 0.22F;
  constexpr float k_rear_wheel_r = 0.28F;
  constexpr float k_front_wheel_t = 0.06F;
  constexpr float k_rear_wheel_t = 0.08F;

  append_box(verts, idx, {-0.56F, 0.34F, -0.72F}, {-0.42F, 0.42F, 0.54F});
  append_box(verts, idx, {0.42F, 0.34F, -0.72F}, {0.56F, 0.42F, 0.54F});
  append_box(verts, idx, {-0.54F, 0.30F, -0.56F}, {0.54F, 0.38F, -0.42F});
  append_box(verts, idx, {-0.58F, 0.32F, 0.32F}, {0.58F, 0.40F, 0.50F});

  append_disc_xaxis(verts,
                    idx,
                    -0.76F,
                    0.24F,
                    -0.48F,
                    k_front_wheel_r,
                    k_front_wheel_t,
                    k_wheel_sides);
  append_disc_xaxis(verts,
                    idx,
                    0.76F,
                    0.24F,
                    -0.48F,
                    k_front_wheel_r,
                    k_front_wheel_t,
                    k_wheel_sides);
  append_disc_xaxis(
      verts, idx, -0.80F, 0.30F, 0.42F, k_rear_wheel_r, k_rear_wheel_t, k_wheel_sides);
  append_disc_xaxis(
      verts, idx, 0.80F, 0.30F, 0.42F, k_rear_wheel_r, k_rear_wheel_t, k_wheel_sides);
  append_box(verts, idx, {-0.82F, 0.21F, -0.53F}, {-0.72F, 0.29F, -0.43F});
  append_box(verts, idx, {0.72F, 0.21F, -0.53F}, {0.82F, 0.29F, -0.43F});
  append_box(verts, idx, {-0.85F, 0.26F, 0.36F}, {-0.75F, 0.34F, 0.48F});
  append_box(verts, idx, {0.75F, 0.26F, 0.36F}, {0.85F, 0.34F, 0.48F});

  append_box(verts, idx, {-0.54F, 0.42F, -0.52F}, {0.54F, 0.50F, 0.50F});
  append_box(verts, idx, {-0.62F, 0.42F, -0.56F}, {-0.50F, 0.84F, 0.56F});
  append_box(verts, idx, {0.50F, 0.42F, -0.56F}, {0.62F, 0.84F, 0.56F});
  append_box(verts, idx, {-0.54F, 0.42F, -0.60F}, {0.54F, 0.80F, -0.46F});
  append_box(verts, idx, {-0.54F, 0.42F, 0.44F}, {0.54F, 0.70F, 0.58F});
  append_box(verts, idx, {-0.56F, 0.78F, -0.56F}, {-0.48F, 0.94F, -0.48F});
  append_box(verts, idx, {0.48F, 0.78F, -0.56F}, {0.56F, 0.94F, -0.48F});
  append_box(verts, idx, {-0.56F, 0.78F, 0.48F}, {-0.48F, 0.90F, 0.56F});
  append_box(verts, idx, {0.48F, 0.78F, 0.48F}, {0.56F, 0.90F, 0.56F});
  append_box(verts, idx, {-0.30F, 0.50F, -0.52F}, {0.30F, 0.64F, -0.34F});

  append_box(verts, idx, {-0.18F, 0.30F, -0.70F}, {-0.08F, 0.38F, -1.40F});
  append_box(verts, idx, {0.08F, 0.30F, -0.70F}, {0.18F, 0.38F, -1.40F});
  append_box(verts, idx, {-0.20F, 0.28F, -1.28F}, {0.20F, 0.36F, -1.20F});

  append_vert_prism(verts, idx, -0.18F, 0.50F, -0.02F, 0.15F, 0.44F, 8);
  append_vert_prism(verts, idx, 0.20F, 0.50F, 0.08F, 0.14F, 0.40F, 8);
  append_box(verts, idx, {-0.40F, 0.50F, 0.14F}, {-0.04F, 0.84F, 0.42F});
  append_box(verts, idx, {0.00F, 0.50F, -0.30F}, {0.36F, 0.72F, -0.02F});
  append_box(verts, idx, {-0.16F, 0.86F, -0.10F}, {0.22F, 1.00F, 0.18F});

  upload_prop_mesh_impl(verts,
                        idx,
                        m_supply_cart_vao,
                        m_supply_cart_vertex_buffer,
                        m_supply_cart_index_buffer,
                        m_supply_cart_vertex_count,
                        m_supply_cart_index_count);
}

void VegetationPipeline::shutdown_supply_cart_pipeline() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_supply_cart_vao = m_supply_cart_vertex_buffer = m_supply_cart_index_buffer = 0;
    m_supply_cart_vertex_count = m_supply_cart_index_count = 0;
    return;
  }
  initializeOpenGLFunctions();
  delete_prop_pipeline_impl(m_supply_cart_vao,
                            m_supply_cart_vertex_buffer,
                            m_supply_cart_index_buffer,
                            m_supply_cart_vertex_count,
                            m_supply_cart_index_count);
}

void VegetationPipeline::initialize_weapon_rack_pipeline() {
  initializeOpenGLFunctions();
  shutdown_weapon_rack_pipeline();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  auto append_blade_tip = [&](float cx,
                              float y0,
                              float z,
                              float half_width,
                              float height,
                              float half_depth) {
    const QVector3D base_l(cx - half_width, y0, z - half_depth);
    const QVector3D base_r(cx + half_width, y0, z - half_depth);
    const QVector3D base_rf(cx + half_width, y0, z + half_depth);
    const QVector3D base_lf(cx - half_width, y0, z + half_depth);
    const QVector3D tip(cx, y0 + height, z);

    auto tri = [&](const QVector3D& p0, const QVector3D& p1, const QVector3D& p2) {
      QVector3D n = QVector3D::crossProduct(p1 - p0, p2 - p0);
      if (n.lengthSquared() > 1.0e-8F) {
        n.normalize();
      } else {
        n = {0.0F, 1.0F, 0.0F};
      }
      auto base = static_cast<uint16_t>(verts.size());
      verts.insert(verts.end(), {{p0, n}, {p1, n}, {p2, n}});
      idx.insert(idx.end(), {base, uint16_t(base + 1), uint16_t(base + 2)});
    };

    tri(base_l, base_r, tip);
    tri(base_r, base_rf, tip);
    tri(base_rf, base_lf, tip);
    tri(base_lf, base_l, tip);
  };

  auto append_leaf_blade = [&](float cx,
                               float y0,
                               float z,
                               float half_width,
                               float height,
                               float half_depth) {
    const QVector3D base(cx, y0, z);
    const QVector3D shoulder_l(cx - half_width, y0 + height * 0.34F, z);
    const QVector3D shoulder_r(cx + half_width, y0 + height * 0.34F, z);
    const QVector3D waist_l(cx - half_width * 0.46F, y0 + height * 0.72F, z);
    const QVector3D waist_r(cx + half_width * 0.46F, y0 + height * 0.72F, z);
    const QVector3D tip(cx, y0 + height, z);
    const QVector3D ridge_f(cx, y0 + height * 0.48F, z + half_depth);
    const QVector3D ridge_b(cx, y0 + height * 0.48F, z - half_depth);

    auto tri = [&](const QVector3D& p0, const QVector3D& p1, const QVector3D& p2) {
      QVector3D n = QVector3D::crossProduct(p1 - p0, p2 - p0);
      if (n.lengthSquared() > 1.0e-8F) {
        n.normalize();
      } else {
        n = {0.0F, 1.0F, 0.0F};
      }
      auto base_idx = static_cast<uint16_t>(verts.size());
      verts.insert(verts.end(), {{p0, n}, {p1, n}, {p2, n}});
      idx.insert(idx.end(), {base_idx, uint16_t(base_idx + 1), uint16_t(base_idx + 2)});
    };

    tri(base, shoulder_l, ridge_f);
    tri(base, ridge_f, shoulder_r);
    tri(shoulder_l, waist_l, ridge_f);
    tri(shoulder_r, ridge_f, waist_r);
    tri(waist_l, tip, ridge_f);
    tri(waist_r, ridge_f, tip);

    tri(base, ridge_b, shoulder_l);
    tri(base, shoulder_r, ridge_b);
    tri(shoulder_l, ridge_b, waist_l);
    tri(shoulder_r, waist_r, ridge_b);
    tri(waist_l, ridge_b, tip);
    tri(waist_r, tip, ridge_b);
  };

  auto append_sword_blade = [&](const QVector3D& base,
                                const QVector3D& tip,
                                float base_half_width,
                                float tip_half_width,
                                float half_depth) {
    QVector3D const axis = tip - base;
    QVector3D side(-axis.y(), axis.x(), 0.0F);
    if (side.lengthSquared() < 1.0e-8F) {
      side = {1.0F, 0.0F, 0.0F};
    } else {
      side.normalize();
    }
    const QVector3D mid = base + axis * 0.58F;
    const QVector3D base_l = base - side * base_half_width;
    const QVector3D base_r = base + side * base_half_width;
    const QVector3D mid_l = mid - side * (base_half_width * 0.72F);
    const QVector3D mid_r = mid + side * (base_half_width * 0.72F);
    const QVector3D tip_l = tip - side * tip_half_width;
    const QVector3D tip_r = tip + side * tip_half_width;
    const QVector3D ridge_f = mid + QVector3D(0.0F, 0.0F, half_depth);
    const QVector3D ridge_b = mid - QVector3D(0.0F, 0.0F, half_depth);

    auto quad = [&](const QVector3D& p0,
                    const QVector3D& p1,
                    const QVector3D& p2,
                    const QVector3D& p3) {
      QVector3D n = QVector3D::crossProduct(p1 - p0, p3 - p0);
      if (n.lengthSquared() > 1.0e-8F) {
        n.normalize();
      } else {
        n = {0.0F, 1.0F, 0.0F};
      }
      append_quad(verts, idx, p0, p1, p2, p3, n);
    };

    quad(base_l, mid_l, ridge_f, base);
    quad(base, ridge_f, mid_r, base_r);
    quad(mid_l, tip_l, tip, ridge_f);
    quad(ridge_f, tip, tip_r, mid_r);

    quad(base, ridge_b, mid_l, base_l);
    quad(base_r, mid_r, ridge_b, base);
    quad(ridge_b, tip, tip_l, mid_l);
    quad(mid_r, tip_r, tip, ridge_b);
  };

  append_box(verts, idx, {-0.78F, 0.00F, -0.13F}, {-0.64F, 1.42F, 0.03F});
  append_box(verts, idx, {0.64F, 0.00F, -0.13F}, {0.78F, 1.42F, 0.03F});
  append_box(verts, idx, {-0.88F, 0.00F, 0.13F}, {-0.66F, 0.18F, 0.33F});
  append_box(verts, idx, {0.66F, 0.00F, 0.13F}, {0.88F, 0.18F, 0.33F});
  append_box(verts, idx, {-0.86F, 0.34F, -0.10F}, {0.86F, 0.48F, 0.08F});
  append_box(verts, idx, {-0.84F, 0.96F, -0.12F}, {0.84F, 1.10F, 0.06F});
  append_box(verts, idx, {-0.74F, 1.28F, -0.10F}, {0.74F, 1.40F, 0.04F});
  append_oriented_box(
      verts, idx, {-0.70F, 0.16F, -0.08F}, {-0.18F, 0.96F, -0.08F}, 0.045F, 0.055F);
  append_oriented_box(
      verts, idx, {0.70F, 0.16F, -0.08F}, {0.18F, 0.96F, -0.08F}, 0.045F, 0.055F);

  for (float const x : {-0.58F, -0.30F, 0.00F, 0.30F, 0.58F}) {
    append_box(verts, idx, {x - 0.050F, 0.46F, 0.08F}, {x + 0.050F, 0.58F, 0.24F});
    append_box(verts, idx, {x - 0.045F, 1.10F, 0.06F}, {x + 0.045F, 1.22F, 0.20F});
  }

  append_oriented_box(
      verts, idx, {-0.64F, 0.05F, 0.17F}, {-0.50F, 1.72F, 0.09F}, 0.032F, 0.034F);
  append_leaf_blade(-0.485F, 1.62F, 0.09F, 0.115F, 0.42F, 0.035F);
  append_box(verts, idx, {-0.675F, 0.00F, 0.13F}, {-0.595F, 0.12F, 0.21F});

  append_box(verts, idx, {-0.335F, 0.02F, 0.12F}, {-0.210F, 0.15F, 0.24F});
  append_oriented_box(
      verts, idx, {-0.280F, 0.12F, 0.17F}, {-0.235F, 0.38F, 0.12F}, 0.044F, 0.034F);
  append_oriented_box(
      verts, idx, {-0.525F, 0.34F, 0.13F}, {0.030F, 0.41F, 0.13F}, 0.042F, 0.048F);
  append_sword_blade(
      {-0.250F, 0.42F, 0.12F}, {-0.335F, 1.82F, 0.07F}, 0.080F, 0.018F, 0.030F);
  append_blade_tip(-0.345F, 1.72F, 0.07F, 0.070F, 0.24F, 0.026F);

  append_box(verts, idx, {0.030F, 0.03F, 0.14F}, {0.140F, 0.14F, 0.25F});
  append_oriented_box(
      verts, idx, {0.085F, 0.12F, 0.18F}, {0.080F, 0.34F, 0.13F}, 0.036F, 0.030F);
  append_oriented_box(
      verts, idx, {-0.075F, 0.30F, 0.14F}, {0.285F, 0.37F, 0.14F}, 0.038F, 0.044F);
  append_sword_blade(
      {0.085F, 0.38F, 0.13F}, {0.180F, 1.56F, 0.08F}, 0.065F, 0.015F, 0.026F);
  append_blade_tip(0.185F, 1.46F, 0.08F, 0.058F, 0.22F, 0.024F);

  append_oriented_box(
      verts, idx, {0.54F, 0.05F, 0.16F}, {0.68F, 1.70F, 0.08F}, 0.030F, 0.032F);
  append_leaf_blade(0.690F, 1.60F, 0.08F, 0.105F, 0.40F, 0.033F);
  append_box(verts, idx, {0.505F, 0.00F, 0.12F}, {0.585F, 0.12F, 0.20F});

  {
    constexpr float k_depth = 0.026F;
    struct Pt {
      float x;
      float y;
      float z;
    };
    static constexpr Pt pts[] = {
        {0.25F, 0.06F, -0.02F},
        {0.43F, 0.38F, -0.05F},
        {0.56F, 0.74F, -0.08F},
        {0.60F, 1.06F, -0.08F},
        {0.55F, 1.42F, -0.06F},
        {0.40F, 1.72F, -0.03F},
        {0.22F, 1.94F, -0.01F},
    };
    for (int i = 0; i < 6; ++i) {
      append_oriented_box(verts,
                          idx,
                          {pts[i].x, pts[i].y, pts[i].z},
                          {pts[i + 1].x, pts[i + 1].y, pts[i + 1].z},
                          0.026F,
                          k_depth);
    }
    append_oriented_box(
        verts, idx, {0.225F, 0.10F, -0.02F}, {0.205F, 1.88F, -0.01F}, 0.006F, 0.006F);
    append_box(verts, idx, {0.520F, 0.86F, -0.12F}, {0.625F, 1.12F, 0.02F});
  }

  upload_prop_mesh_impl(verts,
                        idx,
                        m_weapon_rack_vao,
                        m_weapon_rack_vertex_buffer,
                        m_weapon_rack_index_buffer,
                        m_weapon_rack_vertex_count,
                        m_weapon_rack_index_count);
}

void VegetationPipeline::shutdown_weapon_rack_pipeline() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_weapon_rack_vao = m_weapon_rack_vertex_buffer = m_weapon_rack_index_buffer = 0;
    m_weapon_rack_vertex_count = m_weapon_rack_index_count = 0;
    return;
  }
  initializeOpenGLFunctions();
  delete_prop_pipeline_impl(m_weapon_rack_vao,
                            m_weapon_rack_vertex_buffer,
                            m_weapon_rack_index_buffer,
                            m_weapon_rack_vertex_count,
                            m_weapon_rack_index_count);
}

void VegetationPipeline::initialize_ruins_pipeline() {
  initializeOpenGLFunctions();
  shutdown_ruins_pipeline();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  auto add_rubble = [&](const QVector3D& lo, const QVector3D& hi) {
    append_box(verts, idx, lo, hi);
  };

  append_box(verts, idx, {-0.92F, -0.02F, -0.70F}, {0.90F, 0.06F, 0.68F});
  append_box(verts, idx, {-0.76F, 0.06F, -0.56F}, {0.76F, 0.12F, 0.54F});

  append_box(verts, idx, {-0.78F, 0.00F, -0.26F}, {-0.48F, 0.14F, 0.06F});
  append_vert_prism(verts, idx, -0.63F, 0.14F, -0.10F, 0.13F, 1.34F, 10);
  append_box(verts, idx, {-0.80F, 1.48F, -0.28F}, {-0.46F, 1.66F, 0.08F});
  append_box(verts, idx, {-0.74F, 1.66F, -0.24F}, {-0.50F, 1.78F, 0.02F});
  append_oriented_box(
      verts, idx, {-0.74F, 0.24F, -0.34F}, {-0.52F, 1.10F, -0.28F}, 0.05F, 0.05F);

  append_box(verts, idx, {0.46F, 0.00F, -0.20F}, {0.74F, 0.12F, 0.10F});
  append_vert_prism(verts, idx, 0.60F, 0.12F, -0.05F, 0.12F, 0.88F, 10);
  append_box(verts, idx, {0.44F, 1.00F, -0.24F}, {0.76F, 1.16F, 0.12F});
  append_oriented_box(
      verts, idx, {0.70F, 0.16F, 0.16F}, {0.48F, 0.86F, 0.20F}, 0.04F, 0.05F);

  append_box(verts, idx, {-0.34F, 0.00F, 0.26F}, {-0.06F, 0.14F, 0.56F});
  append_vert_prism(verts, idx, -0.20F, 0.14F, 0.40F, 0.12F, 1.42F, 10);
  append_box(verts, idx, {-0.38F, 1.56F, 0.22F}, {0.00F, 1.74F, 0.60F});
  append_oriented_box(
      verts, idx, {-0.06F, 0.22F, 0.52F}, {0.18F, 0.86F, 0.46F}, 0.05F, 0.05F);

  append_box(verts, idx, {-0.62F, 0.12F, -0.52F}, {0.02F, 0.72F, -0.32F});
  append_box(verts, idx, {-0.62F, 0.74F, -0.50F}, {-0.18F, 1.04F, -0.30F});
  append_box(verts, idx, {-0.06F, 0.12F, -0.48F}, {0.24F, 0.52F, -0.30F});
  append_box(verts, idx, {-0.58F, 1.08F, -0.46F}, {-0.24F, 1.24F, -0.26F});
  append_box(verts, idx, {-0.18F, 1.02F, -0.42F}, {0.12F, 1.16F, -0.24F});

  append_box(verts, idx, {0.12F, 0.12F, 0.14F}, {0.52F, 0.86F, 0.48F});
  append_box(verts, idx, {0.08F, 0.88F, 0.18F}, {0.46F, 1.08F, 0.52F});
  append_box(verts, idx, {0.30F, 0.18F, -0.56F}, {0.60F, 0.42F, -0.34F});

  append_oriented_box(
      verts, idx, {-0.42F, 1.46F, -0.10F}, {0.18F, 1.22F, -0.08F}, 0.10F, 0.08F);
  append_oriented_box(
      verts, idx, {0.18F, 1.22F, -0.08F}, {0.54F, 1.08F, -0.04F}, 0.08F, 0.07F);
  append_oriented_box(
      verts, idx, {-0.26F, 1.60F, 0.30F}, {0.10F, 1.30F, 0.26F}, 0.08F, 0.08F);

  add_rubble({-0.82F, 0.02F, -0.64F}, {-0.54F, 0.16F, -0.44F});
  add_rubble({-0.40F, 0.02F, -0.64F}, {-0.08F, 0.18F, -0.46F});
  add_rubble({0.06F, 0.02F, -0.66F}, {0.34F, 0.12F, -0.46F});
  add_rubble({0.42F, 0.02F, -0.62F}, {0.76F, 0.18F, -0.40F});
  add_rubble({-0.64F, 0.02F, 0.44F}, {-0.28F, 0.16F, 0.64F});
  add_rubble({-0.14F, 0.02F, 0.50F}, {0.20F, 0.14F, 0.66F});
  add_rubble({0.28F, 0.02F, 0.36F}, {0.64F, 0.16F, 0.62F});
  add_rubble({-0.18F, 0.02F, -0.08F}, {0.10F, 0.10F, 0.14F});

  append_oriented_box(
      verts, idx, {-0.28F, 0.14F, -0.24F}, {-0.02F, 0.08F, 0.02F}, 0.07F, 0.08F);
  append_oriented_box(
      verts, idx, {0.22F, 0.12F, 0.04F}, {0.52F, 0.06F, 0.28F}, 0.06F, 0.08F);
  append_oriented_box(
      verts, idx, {-0.52F, 0.12F, 0.18F}, {-0.28F, 0.06F, 0.34F}, 0.06F, 0.06F);

  upload_prop_mesh_impl(verts,
                        idx,
                        m_ruins_vao,
                        m_ruins_vertex_buffer,
                        m_ruins_index_buffer,
                        m_ruins_vertex_count,
                        m_ruins_index_count);
}

void VegetationPipeline::shutdown_ruins_pipeline() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_ruins_vao = m_ruins_vertex_buffer = m_ruins_index_buffer = 0;
    m_ruins_vertex_count = m_ruins_index_count = 0;
    return;
  }
  initializeOpenGLFunctions();
  delete_prop_pipeline_impl(m_ruins_vao,
                            m_ruins_vertex_buffer,
                            m_ruins_index_buffer,
                            m_ruins_vertex_count,
                            m_ruins_index_count);
}

void VegetationPipeline::initialize_dead_tree_pipeline() {
  initializeOpenGLFunctions();
  shutdown_dead_tree_pipeline();

  const auto mesh = build_dead_tree_mesh();

  upload_prop_mesh_impl(mesh.vertices,
                        mesh.indices,
                        m_dead_tree_vao,
                        m_dead_tree_vertex_buffer,
                        m_dead_tree_index_buffer,
                        m_dead_tree_vertex_count,
                        m_dead_tree_index_count);
}

void VegetationPipeline::shutdown_dead_tree_pipeline() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_dead_tree_vao = m_dead_tree_vertex_buffer = m_dead_tree_index_buffer = 0;
    m_dead_tree_vertex_count = m_dead_tree_index_count = 0;
    return;
  }
  initializeOpenGLFunctions();
  delete_prop_pipeline_impl(m_dead_tree_vao,
                            m_dead_tree_vertex_buffer,
                            m_dead_tree_index_buffer,
                            m_dead_tree_vertex_count,
                            m_dead_tree_index_count);
}

void VegetationPipeline::initialize_iron_ore_pipeline() {
  initializeOpenGLFunctions();
  shutdown_iron_ore_pipeline();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  // Flat base slab — ore seam embedded in ground
  append_box(verts, idx, {-0.55F, -0.02F, -0.45F}, {0.55F, 0.10F, 0.45F});

  // Main ore mass — blocky central body
  append_box(verts, idx, {-0.42F, 0.08F, -0.35F}, {0.40F, 0.42F, 0.32F});

  // Upper-left angular chunk
  append_box(verts, idx, {-0.38F, 0.38F, -0.28F}, {-0.06F, 0.70F, 0.18F});

  // Upper-right chunk
  append_box(verts, idx, {0.06F, 0.34F, -0.22F}, {0.34F, 0.64F, 0.14F});

  // Front protrusion
  append_box(verts, idx, {-0.20F, 0.10F, 0.28F}, {0.18F, 0.42F, 0.50F});

  // Back overhang
  append_box(verts, idx, {-0.28F, 0.12F, -0.50F}, {0.14F, 0.34F, -0.28F});

  // Small surface nodules
  append_box(verts, idx, {-0.44F, 0.40F, -0.08F}, {-0.26F, 0.58F, 0.10F});
  append_box(verts, idx, {0.22F, 0.38F, 0.08F}, {0.40F, 0.54F, 0.26F});

  upload_prop_mesh_impl(verts,
                        idx,
                        m_iron_ore_vao,
                        m_iron_ore_vertex_buffer,
                        m_iron_ore_index_buffer,
                        m_iron_ore_vertex_count,
                        m_iron_ore_index_count);
}

void VegetationPipeline::shutdown_iron_ore_pipeline() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_iron_ore_vao = m_iron_ore_vertex_buffer = m_iron_ore_index_buffer = 0;
    m_iron_ore_vertex_count = m_iron_ore_index_count = 0;
    return;
  }
  initializeOpenGLFunctions();
  delete_prop_pipeline_impl(m_iron_ore_vao,
                            m_iron_ore_vertex_buffer,
                            m_iron_ore_index_buffer,
                            m_iron_ore_vertex_count,
                            m_iron_ore_index_count);
}

void VegetationPipeline::initialize_magic_shrine_pipeline() {
  initializeOpenGLFunctions();
  shutdown_magic_shrine_pipeline();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  // Base platform — flat stone foundation
  append_box(verts, idx, {-0.55F, -0.02F, -0.55F}, {0.55F, 0.06F, 0.55F});

  // Lower tier
  append_box(verts, idx, {-0.40F, 0.06F, -0.40F}, {0.40F, 0.14F, 0.40F});

  // Central pillar / altar block
  append_box(verts, idx, {-0.18F, 0.14F, -0.18F}, {0.18F, 0.72F, 0.18F});

  // Altar top cap
  append_box(verts, idx, {-0.22F, 0.72F, -0.22F}, {0.22F, 0.82F, 0.22F});

  // Four corner columns
  append_box(verts, idx, {-0.44F, 0.06F, -0.44F}, {-0.30F, 0.78F, -0.30F});
  append_box(verts, idx, { 0.30F, 0.06F, -0.44F}, { 0.44F, 0.78F, -0.30F});
  append_box(verts, idx, {-0.44F, 0.06F,  0.30F}, {-0.30F, 0.78F,  0.44F});
  append_box(verts, idx, { 0.30F, 0.06F,  0.30F}, { 0.44F, 0.78F,  0.44F});

  // Column capitals (top caps on columns)
  append_box(verts, idx, {-0.48F, 0.78F, -0.48F}, {-0.26F, 0.86F, -0.26F});
  append_box(verts, idx, { 0.26F, 0.78F, -0.48F}, { 0.48F, 0.86F, -0.26F});
  append_box(verts, idx, {-0.48F, 0.78F,  0.26F}, {-0.26F, 0.86F,  0.48F});
  append_box(verts, idx, { 0.26F, 0.78F,  0.26F}, { 0.48F, 0.86F,  0.48F});

  // Lintel beams connecting front and back pairs
  append_box(verts, idx, {-0.50F, 0.84F, -0.42F}, { 0.50F, 0.92F, -0.32F});
  append_box(verts, idx, {-0.50F, 0.84F,  0.32F}, { 0.50F, 0.92F,  0.42F});

  // Side lintels
  append_box(verts, idx, {-0.42F, 0.84F, -0.50F}, {-0.32F, 0.92F,  0.50F});
  append_box(verts, idx, { 0.32F, 0.84F, -0.50F}, { 0.42F, 0.92F,  0.50F});

  // Offering bowl atop altar
  append_box(verts, idx, {-0.12F, 0.82F, -0.12F}, { 0.12F, 0.88F,  0.12F});
  append_box(verts, idx, {-0.08F, 0.88F, -0.08F}, { 0.08F, 0.96F,  0.08F});

  // Small rune stones scattered around base
  append_box(verts, idx, {-0.64F, 0.00F, -0.10F}, {-0.52F, 0.22F,  0.04F});
  append_box(verts, idx, { 0.52F, 0.00F,  0.06F}, { 0.64F, 0.18F,  0.18F});
  append_box(verts, idx, {-0.08F, 0.00F,  0.52F}, { 0.06F, 0.20F,  0.64F});

  upload_prop_mesh_impl(verts,
                        idx,
                        m_magic_shrine_vao,
                        m_magic_shrine_vertex_buffer,
                        m_magic_shrine_index_buffer,
                        m_magic_shrine_vertex_count,
                        m_magic_shrine_index_count);
}

void VegetationPipeline::shutdown_magic_shrine_pipeline() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_magic_shrine_vao = m_magic_shrine_vertex_buffer = m_magic_shrine_index_buffer = 0;
    m_magic_shrine_vertex_count = m_magic_shrine_index_count = 0;
    return;
  }
  initializeOpenGLFunctions();
  delete_prop_pipeline_impl(m_magic_shrine_vao,
                            m_magic_shrine_vertex_buffer,
                            m_magic_shrine_index_buffer,
                            m_magic_shrine_vertex_count,
                            m_magic_shrine_index_count);
}

} // namespace Render::GL::BackendPipelines
