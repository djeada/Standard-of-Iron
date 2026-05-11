#include "vegetation_pipeline.h"
#include "../render_constants.h"
#include "gl/shader_cache.h"
#include <GL/gl.h>
#include <QDebug>
#include <QOpenGLContext>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <qglobal.h>
#include <qopenglcontext.h>
#include <qopenglext.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <vector>

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::Geometry;

VegetationPipeline::VegetationPipeline(ShaderCache *shader_cache)
    : m_shader_cache(shader_cache) {}

VegetationPipeline::~VegetationPipeline() { shutdown(); }

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
  m_initialized = false;
}

void VegetationPipeline::cache_uniforms() {
  if (m_stone_shader != nullptr) {
    m_stone_uniforms.view_proj =
        m_stone_shader->optional_uniform_handle("u_viewProj");
    m_stone_uniforms.light_direction =
        m_stone_shader->uniform_handle("uLightDirection");
  }

  if (m_plant_shader != nullptr) {
    m_plant_uniforms.view_proj =
        m_plant_shader->optional_uniform_handle("u_viewProj");
    m_plant_uniforms.time = m_plant_shader->uniform_handle("uTime");
    m_plant_uniforms.wind_strength =
        m_plant_shader->uniform_handle("uWindStrength");
    m_plant_uniforms.wind_speed = m_plant_shader->uniform_handle("uWindSpeed");
    m_plant_uniforms.light_direction =
        m_plant_shader->uniform_handle("uLightDirection");
  }

  if (m_pine_shader != nullptr) {
    m_pine_uniforms.view_proj =
        m_pine_shader->optional_uniform_handle("u_viewProj");
    m_pine_uniforms.time = m_pine_shader->uniform_handle("uTime");
    m_pine_uniforms.wind_strength =
        m_pine_shader->uniform_handle("uWindStrength");
    m_pine_uniforms.wind_speed = m_pine_shader->uniform_handle("uWindSpeed");
    m_pine_uniforms.light_direction =
        m_pine_shader->uniform_handle("uLightDirection");
  }

  if (m_olive_shader != nullptr) {
    m_olive_uniforms.view_proj =
        m_olive_shader->optional_uniform_handle("u_viewProj");
    m_olive_uniforms.time = m_olive_shader->uniform_handle("uTime");
    m_olive_uniforms.wind_strength =
        m_olive_shader->uniform_handle("uWindStrength");
    m_olive_uniforms.wind_speed = m_olive_shader->uniform_handle("uWindSpeed");
    m_olive_uniforms.light_direction =
        m_olive_shader->uniform_handle("uLightDirection");
  }

  if (m_firecamp_shader != nullptr) {
    m_firecamp_uniforms.view_proj =
        m_firecamp_shader->optional_uniform_handle("u_viewProj");
    m_firecamp_uniforms.time = m_firecamp_shader->uniform_handle("u_time");
    m_firecamp_uniforms.flicker_speed =
        m_firecamp_shader->uniform_handle("u_flickerSpeed");
    m_firecamp_uniforms.flicker_amount =
        m_firecamp_shader->uniform_handle("u_flickerAmount");
    m_firecamp_uniforms.glow_strength =
        m_firecamp_shader->uniform_handle("u_glowStrength");
    m_firecamp_uniforms.fire_texture =
        m_firecamp_shader->uniform_handle("fire_texture");
    m_firecamp_uniforms.camera_right =
        m_firecamp_shader->uniform_handle("u_cameraRight");
    m_firecamp_uniforms.camera_forward =
        m_firecamp_shader->uniform_handle("u_cameraForward");
  }

  auto cache_prop_uniforms = [&](PropUniforms &u, GL::Shader *shader) {
    if (shader == nullptr) {
      return;
    }
    u.view_proj = shader->optional_uniform_handle("u_viewProj");
    u.light_direction = shader->uniform_handle("uLightDirection");
  };

  cache_prop_uniforms(m_tent_uniforms, m_tent_shader);
  cache_prop_uniforms(m_supply_cart_uniforms, m_supply_cart_shader);
  cache_prop_uniforms(m_weapon_rack_uniforms, m_weapon_rack_shader);
  cache_prop_uniforms(m_ruins_uniforms, m_ruins_shader);
  cache_prop_uniforms(m_dead_tree_uniforms, m_dead_tree_shader);
}

void VegetationPipeline::initialize_stone_pipeline() {
  initializeOpenGLFunctions();
  shutdown_stone_pipeline();

  struct StoneVertex {
    QVector3D position;
    QVector3D normal;
  };

  const StoneVertex stone_vertices[] = {
      {{-0.5F, -0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, -0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, 0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}},
      {{-0.5F, 0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}},
      {{-0.5F, -0.5F, -0.5F}, {0.0F, 0.0F, -1.0F}},
      {{-0.5F, 0.5F, -0.5F}, {0.0F, 0.0F, -1.0F}},
      {{0.5F, 0.5F, -0.5F}, {0.0F, 0.0F, -1.0F}},
      {{0.5F, -0.5F, -0.5F}, {0.0F, 0.0F, -1.0F}},
      {{-0.5F, 0.5F, -0.5F}, {0.0F, 1.0F, 0.0F}},
      {{-0.5F, 0.5F, 0.5F}, {0.0F, 1.0F, 0.0F}},
      {{0.5F, 0.5F, 0.5F}, {0.0F, 1.0F, 0.0F}},
      {{0.5F, 0.5F, -0.5F}, {0.0F, 1.0F, 0.0F}},
      {{-0.5F, -0.5F, -0.5F}, {0.0F, -1.0F, 0.0F}},
      {{0.5F, -0.5F, -0.5F}, {0.0F, -1.0F, 0.0F}},
      {{0.5F, -0.5F, 0.5F}, {0.0F, -1.0F, 0.0F}},
      {{-0.5F, -0.5F, 0.5F}, {0.0F, -1.0F, 0.0F}},
      {{0.5F, -0.5F, -0.5F}, {1.0F, 0.0F, 0.0F}},
      {{0.5F, 0.5F, -0.5F}, {1.0F, 0.0F, 0.0F}},
      {{0.5F, 0.5F, 0.5F}, {1.0F, 0.0F, 0.0F}},
      {{0.5F, -0.5F, 0.5F}, {1.0F, 0.0F, 0.0F}},
      {{-0.5F, -0.5F, -0.5F}, {-1.0F, 0.0F, 0.0F}},
      {{-0.5F, -0.5F, 0.5F}, {-1.0F, 0.0F, 0.0F}},
      {{-0.5F, 0.5F, 0.5F}, {-1.0F, 0.0F, 0.0F}},
      {{-0.5F, 0.5F, -0.5F}, {-1.0F, 0.0F, 0.0F}},
  };

  const uint16_t stone_indices[] = {
      0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
      12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};

  glGenVertexArrays(1, &m_stone_vao);
  glBindVertexArray(m_stone_vao);

  glGenBuffers(1, &m_stone_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_stone_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(stone_vertices), stone_vertices,
               GL_STATIC_DRAW);
  m_stone_vertex_count = CubeVertexCount;

  glGenBuffers(1, &m_stone_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_stone_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(stone_indices), stone_indices,
               GL_STATIC_DRAW);
  constexpr int k_stone_index_count = 36;
  m_stone_index_count = k_stone_index_count;

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(
      Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(StoneVertex),
      reinterpret_cast<void *>(offsetof(StoneVertex, position)));
  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(
      Normal, Vec3, GL_FLOAT, GL_FALSE, sizeof(StoneVertex),
      reinterpret_cast<void *>(offsetof(StoneVertex, normal)));

  glEnableVertexAttribArray(TexCoord);
  glVertexAttribDivisor(TexCoord, 1);
  glEnableVertexAttribArray(InstancePosition);
  glVertexAttribDivisor(InstancePosition, 1);

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

  const PlantVertex plant_vertices[] = {
      {{-0.5F, 0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, 0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, 1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}},
      {{-0.5F, 1.0F, 0.0F}, {0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}},
      {{0.5F, 0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}},
      {{-0.5F, 0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}},
      {{-0.5F, 1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 0.0F, -1.0F}},
      {{0.5F, 1.0F, 0.0F}, {0.0F, 1.0F}, {0.0F, 0.0F, -1.0F}},
      {{0.0F, 0.0F, -0.5F}, {0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}},
      {{0.0F, 0.0F, 0.5F}, {1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}},
      {{0.0F, 1.0F, 0.5F}, {1.0F, 1.0F}, {1.0F, 0.0F, 0.0F}},
      {{0.0F, 1.0F, -0.5F}, {0.0F, 1.0F}, {1.0F, 0.0F, 0.0F}},
      {{0.0F, 0.0F, 0.5F}, {0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}},
      {{0.0F, 0.0F, -0.5F}, {1.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}},
      {{0.0F, 1.0F, -0.5F}, {1.0F, 1.0F}, {-1.0F, 0.0F, 0.0F}},
      {{0.0F, 1.0F, 0.5F}, {0.0F, 1.0F}, {-1.0F, 0.0F, 0.0F}},
  };

  const unsigned short plant_indices[] = {
      0, 1, 2,  0, 2,  3,  4,  5,  6,  4,  6,  7,
      8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15,
  };

  glGenVertexArrays(1, &m_plant_vao);
  glBindVertexArray(m_plant_vao);

  glGenBuffers(1, &m_plant_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_plant_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(plant_vertices), plant_vertices,
               GL_STATIC_DRAW);
  m_plant_vertex_count = PlantCrossQuadVertexCount;

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(
      Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(PlantVertex),
      reinterpret_cast<void *>(offsetof(PlantVertex, position)));

  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(
      Normal, Vec2, GL_FLOAT, GL_FALSE, sizeof(PlantVertex),
      reinterpret_cast<void *>(offsetof(PlantVertex, tex_coord)));

  glEnableVertexAttribArray(TexCoord);
  glVertexAttribPointer(
      TexCoord, Vec3, GL_FLOAT, GL_FALSE, sizeof(PlantVertex),
      reinterpret_cast<void *>(offsetof(PlantVertex, normal)));

  glGenBuffers(1, &m_plant_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_plant_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(plant_indices), plant_indices,
               GL_STATIC_DRAW);
  m_plant_index_count = PlantCrossQuadIndexCount;

  glEnableVertexAttribArray(InstancePosition);
  glVertexAttribDivisor(InstancePosition, 1);
  glEnableVertexAttribArray(InstanceScale);
  glVertexAttribDivisor(InstanceScale, 1);
  glEnableVertexAttribArray(InstanceColor);
  glVertexAttribDivisor(InstanceColor, 1);

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

  constexpr int k_segments = PineTreeSegments;
  constexpr float k_two_pi = 6.28318530718F;

  std::vector<PineVertex> vertices;
  vertices.reserve(16 * k_segments + 2);

  std::vector<unsigned short> indices;
  indices.reserve(16 * k_segments * 6);

  auto add_ring = [&](float radius, float y, float normal_up, float v_coord,
                      const QVector2D &center_offset =
                          QVector2D(0.0F, 0.0F)) -> int {
    const int start = static_cast<int>(vertices.size());
    for (int i = 0; i < k_segments; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(k_segments);
      const float angle = t * k_two_pi;
      const float nx = std::cos(angle);
      const float nz = std::sin(angle);
      QVector3D normal(nx, normal_up, nz);
      normal.normalize();
      QVector3D const position(radius * nx + center_offset.x(), y,
                               radius * nz + center_offset.y());
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

  const int trunk_bottom = add_ring(0.12F, 0.00F, 0.00F, 0.00F);
  const int trunk_kink =
      add_ring(0.11F, 0.22F, 0.00F, 0.09F, QVector2D(0.014F, 0.009F));
  const int trunk_mid =
      add_ring(0.10F, 0.40F, 0.02F, 0.17F, QVector2D(0.020F, 0.013F));
  const int trunk_top =
      add_ring(0.09F, 0.54F, 0.05F, 0.28F, QVector2D(0.018F, 0.015F));

  const QVector2D t1o(-0.040F, 0.055F);
  const int c1_base = add_ring(0.65F, 0.60F, 0.28F, 0.44F, t1o);
  const int c1_outer = add_ring(0.56F, 0.67F, 0.40F, 0.51F, t1o * 0.70F);
  const int c1_mid = add_ring(0.40F, 0.74F, 0.56F, 0.57F, t1o * 0.40F);
  const int c1_top = add_ring(0.22F, 0.79F, 0.72F, 0.63F, t1o * 0.20F);

  const QVector2D t2o(0.045F, -0.032F);
  const int c2_base = add_ring(0.48F, 0.81F, 0.22F, 0.64F, t2o);
  const int c2_outer = add_ring(0.34F, 0.88F, 0.50F, 0.71F, t2o * 0.65F);
  const int c2_top = add_ring(0.16F, 0.93F, 0.72F, 0.77F, t2o * 0.30F);

  const QVector2D t3o(-0.028F, -0.040F);
  const int c3_base = add_ring(0.30F, 0.95F, 0.28F, 0.78F, t3o);
  const int c3_outer = add_ring(0.20F, 1.01F, 0.58F, 0.84F, t3o * 0.55F);
  const int c3_top = add_ring(0.09F, 1.06F, 0.82F, 0.89F, t3o * 0.20F);

  const int tip_ring = add_ring(0.04F, 1.13F, 0.90F, 0.95F);

  connect_rings(trunk_bottom, trunk_kink);
  connect_rings(trunk_kink, trunk_mid);
  connect_rings(trunk_mid, trunk_top);
  connect_rings(trunk_top, c1_base);
  connect_rings(c1_base, c1_outer);
  connect_rings(c1_outer, c1_mid);
  connect_rings(c1_mid, c1_top);
  connect_rings(c1_top, c2_base);
  connect_rings(c2_base, c2_outer);
  connect_rings(c2_outer, c2_top);
  connect_rings(c2_top, c3_base);
  connect_rings(c3_base, c3_outer);
  connect_rings(c3_outer, c3_top);
  connect_rings(c3_top, tip_ring);

  const auto trunk_cap_index = static_cast<unsigned short>(vertices.size());
  vertices.push_back({QVector3D(0.0F, 0.0F, 0.0F), QVector2D(0.5F, 0.0F),
                      QVector3D(0.0F, -1.0F, 0.0F)});
  for (int i = 0; i < k_segments; ++i) {
    const int next = (i + 1) % k_segments;
    indices.push_back(static_cast<unsigned short>(trunk_bottom + next));
    indices.push_back(static_cast<unsigned short>(trunk_bottom + i));
    indices.push_back(trunk_cap_index);
  }

  const auto apex_index = static_cast<unsigned short>(vertices.size());
  vertices.push_back({QVector3D(0.0F, 1.18F, 0.0F), QVector2D(0.5F, 1.0F),
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
               vertices.data(), GL_STATIC_DRAW);
  m_pine_vertex_count = static_cast<GLsizei>(vertices.size());

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(
      Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(PineVertex),
      reinterpret_cast<void *>(offsetof(PineVertex, position)));

  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(
      Normal, Vec2, GL_FLOAT, GL_FALSE, sizeof(PineVertex),
      reinterpret_cast<void *>(offsetof(PineVertex, tex_coord)));

  glEnableVertexAttribArray(TexCoord);
  glVertexAttribPointer(TexCoord, Vec3, GL_FLOAT, GL_FALSE, sizeof(PineVertex),
                        reinterpret_cast<void *>(offsetof(PineVertex, normal)));

  glGenBuffers(1, &m_pine_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pine_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned short)),
               indices.data(), GL_STATIC_DRAW);
  m_pine_index_count = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(InstancePosition);
  glVertexAttribDivisor(InstancePosition, 1);
  glEnableVertexAttribArray(InstanceScale);
  glVertexAttribDivisor(InstanceScale, 1);
  glEnableVertexAttribArray(InstanceColor);
  glVertexAttribDivisor(InstanceColor, 1);

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

  constexpr int k_segments = OliveTreeSegments;
  constexpr float k_two_pi = 6.28318530718F;

  std::vector<OliveVertex> vertices;
  vertices.reserve(k_segments * 40);
  std::vector<unsigned short> indices;
  indices.reserve(k_segments * 6 * 40);

  auto add_ring = [&](float radius, float y, float normal_up, float v_coord,
                      const QVector2D &offset = QVector2D(0.0F, 0.0F)) -> int {
    const int start = static_cast<int>(vertices.size());
    for (int i = 0; i < k_segments; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(k_segments);
      const float angle = t * k_two_pi;
      const float nx = std::cos(angle);
      const float nz = std::sin(angle);
      QVector3D normal(nx, normal_up, nz);
      normal.normalize();
      QVector3D const position(radius * nx + offset.x(), y,
                               radius * nz + offset.y());
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

  auto add_cap = [&](int ring, float cap_y, const QVector2D &offset, float v) {
    const int top_idx = static_cast<int>(vertices.size());
    vertices.push_back({QVector3D(offset.x(), cap_y, offset.y()),
                        QVector2D(0.5F, v), QVector3D(0.0F, 1.0F, 0.0F)});
    for (int i = 0; i < k_segments; ++i) {
      const int next = (i + 1) % k_segments;
      indices.push_back(static_cast<unsigned short>(ring + i));
      indices.push_back(static_cast<unsigned short>(ring + next));
      indices.push_back(static_cast<unsigned short>(top_idx));
    }
  };

  int t0 = add_ring(0.14F, 0.00F, -0.2F, 0.00F);
  int t1 = add_ring(0.12F, 0.08F, 0.0F, 0.06F);
  int t2 = add_ring(0.09F, 0.15F, 0.1F, 0.12F);
  connect_rings(t0, t1);
  connect_rings(t1, t2);

  auto add_branch = [&](float dir_x, float dir_z, float base_y, float length,
                        float branch_r, float leaf_r, float v_start) {
    float len = std::sqrt(dir_x * dir_x + dir_z * dir_z);
    dir_x /= len;
    dir_z /= len;

    float rise = 0.5F;
    float dx = dir_x * std::cos(0.5F);
    float dz = dir_z * std::cos(0.5F);
    float dy = std::sin(0.4F);

    int b0 = add_ring(branch_r, base_y, 0.0F, v_start);

    float mid_dist = length * 0.5F;
    QVector2D mid_offset(dx * mid_dist, dz * mid_dist);
    int b1 = add_ring(branch_r * 0.6F, base_y + dy * mid_dist, 0.3F,
                      v_start + 0.1F, mid_offset);

    float tip_dist = length;
    QVector2D tip_offset(dx * tip_dist, dz * tip_dist);
    float tip_y = base_y + dy * tip_dist;
    int b2 = add_ring(branch_r * 0.3F, tip_y, 0.5F, v_start + 0.2F, tip_offset);

    connect_rings(b0, b1);
    connect_rings(b1, b2);

    float ly = tip_y - leaf_r * 0.2F;
    int l0 = add_ring(leaf_r * 0.6F, ly, -0.4F, 0.50F, tip_offset);
    int l1 =
        add_ring(leaf_r * 0.9F, ly + leaf_r * 0.35F, 0.0F, 0.65F, tip_offset);
    int l2 =
        add_ring(leaf_r * 0.85F, ly + leaf_r * 0.65F, 0.2F, 0.80F, tip_offset);
    int l3 =
        add_ring(leaf_r * 0.5F, ly + leaf_r * 0.90F, 0.6F, 0.92F, tip_offset);

    connect_rings(b2, l0);
    connect_rings(l0, l1);
    connect_rings(l1, l2);
    connect_rings(l2, l3);
    add_cap(l3, ly + leaf_r * 1.0F, tip_offset, 1.0F);
  };

  add_branch(0.8F, 0.3F, 0.14F, 0.30F, 0.025F, 0.18F, 0.18F);
  add_branch(-0.7F, 0.5F, 0.15F, 0.32F, 0.022F, 0.20F, 0.20F);
  add_branch(0.4F, -0.9F, 0.16F, 0.28F, 0.020F, 0.16F, 0.22F);
  add_branch(-0.5F, -0.7F, 0.14F, 0.34F, 0.024F, 0.19F, 0.19F);

  m_olive_vertex_count = static_cast<GLsizei>(vertices.size());
  m_olive_index_count = static_cast<GLsizei>(indices.size());

  glGenVertexArrays(1, &m_olive_vao);
  glBindVertexArray(m_olive_vao);

  glGenBuffers(1, &m_olive_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_olive_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(OliveVertex)),
               vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m_olive_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_olive_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned short)),
               indices.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      0, 3, GL_FLOAT, GL_FALSE, sizeof(OliveVertex),
      reinterpret_cast<void *>(offsetof(OliveVertex, position)));

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(
      1, 2, GL_FLOAT, GL_FALSE, sizeof(OliveVertex),
      reinterpret_cast<void *>(offsetof(OliveVertex, tex_coord)));

  glEnableVertexAttribArray(2);
  glVertexAttribPointer(
      2, 3, GL_FLOAT, GL_FALSE, sizeof(OliveVertex),
      reinterpret_cast<void *>(offsetof(OliveVertex, normal)));

  glEnableVertexAttribArray(InstancePosition);
  glVertexAttribDivisor(InstancePosition, 1);
  glEnableVertexAttribArray(InstanceScale);
  glVertexAttribDivisor(InstanceScale, 1);
  glEnableVertexAttribArray(InstanceColor);
  glVertexAttribDivisor(InstanceColor, 1);

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
    vertices.push_back(
        {QVector3D(-1.0F, 0.0F, plane_index), QVector2D(0.0F, 0.0F)});
    vertices.push_back(
        {QVector3D(1.0F, 0.0F, plane_index), QVector2D(1.0F, 0.0F)});
    vertices.push_back(
        {QVector3D(1.0F, 2.0F, plane_index), QVector2D(1.0F, 1.0F)});
    vertices.push_back(
        {QVector3D(-1.0F, 2.0F, plane_index), QVector2D(0.0F, 1.0F)});

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
  glBufferData(
      GL_ARRAY_BUFFER,
      static_cast<GLsizeiptr>(vertices.size() * sizeof(FireCampVertex)),
      vertices.data(), GL_STATIC_DRAW);
  m_firecamp_vertex_count = static_cast<GLsizei>(vertices.size());

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(Position, Vec3, GL_FLOAT, GL_FALSE,
                        sizeof(FireCampVertex), reinterpret_cast<void *>(0));

  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(
      Normal, Vec2, GL_FLOAT, GL_FALSE, sizeof(FireCampVertex),
      reinterpret_cast<void *>(offsetof(FireCampVertex, tex_coord)));

  glGenBuffers(1, &m_firecamp_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_firecamp_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned short)),
               indices.data(), GL_STATIC_DRAW);
  m_firecamp_index_count = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(InstancePosition);
  glVertexAttribDivisor(InstancePosition, 1);

  glEnableVertexAttribArray(InstanceScale);
  glVertexAttribDivisor(InstanceScale, 1);

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
    const std::vector<std::pair<QVector3D, QVector3D>> &verts,
    const std::vector<uint16_t> &idx,
    GLuint &vao, GLuint &vbo, GLuint &ibo,
    GLsizei &vert_count, GLsizei &idx_count) {

  using namespace Render::GL::VertexAttrib;
  using namespace Render::GL::ComponentCount;

  struct V { QVector3D pos; QVector3D nrm; };
  std::vector<V> flat;
  flat.reserve(verts.size());
  for (const auto &[p, n] : verts) {
    flat.push_back({p, n});
  }

  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(flat.size() * sizeof(V)),
               flat.data(), GL_STATIC_DRAW);
  vert_count = static_cast<GLsizei>(flat.size());

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(V),
                        reinterpret_cast<void *>(offsetof(V, pos)));
  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(Normal, Vec3, GL_FLOAT, GL_FALSE, sizeof(V),
                        reinterpret_cast<void *>(offsetof(V, nrm)));

  glGenBuffers(1, &ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(idx.size() * sizeof(uint16_t)),
               idx.data(), GL_STATIC_DRAW);
  idx_count = static_cast<GLsizei>(idx.size());

  glEnableVertexAttribArray(TexCoord);
  glVertexAttribDivisor(TexCoord, 1);
  glEnableVertexAttribArray(InstancePosition);
  glVertexAttribDivisor(InstancePosition, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void VegetationPipeline::delete_prop_pipeline_impl(
    GLuint &vao, GLuint &vbo, GLuint &ibo, GLsizei &vc, GLsizei &ic) {
  if (ibo != 0U) { glDeleteBuffers(1, &ibo); ibo = 0; }
  if (vbo != 0U) { glDeleteBuffers(1, &vbo); vbo = 0; }
  if (vao != 0U) { glDeleteVertexArrays(1, &vao); vao = 0; }
  vc = 0;
  ic = 0;
}

// ── Shared helpers ────────────────────────────────────────────────────────────
// Append a box from minP to maxP with face normals into vertex/index vectors.
static void append_box(
    std::vector<std::pair<QVector3D, QVector3D>> &verts,
    std::vector<uint16_t> &idx,
    const QVector3D &lo, const QVector3D &hi) {

  using F = std::pair<QVector3D, QVector3D>;
  const float x0 = lo.x(), y0 = lo.y(), z0 = lo.z();
  const float x1 = hi.x(), y1 = hi.y(), z1 = hi.z();

  // +Z
  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D n(0, 0, 1);
    verts.insert(verts.end(), {F{{x0,y0,z1},n}, F{{x1,y0,z1},n},
                                F{{x1,y1,z1},n}, F{{x0,y1,z1},n}});
    idx.insert(idx.end(), {b,uint16_t(b+1),uint16_t(b+2),
                            b,uint16_t(b+2),uint16_t(b+3)});
  }
  // -Z
  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D n(0, 0, -1);
    verts.insert(verts.end(), {F{{x1,y0,z0},n}, F{{x0,y0,z0},n},
                                F{{x0,y1,z0},n}, F{{x1,y1,z0},n}});
    idx.insert(idx.end(), {b,uint16_t(b+1),uint16_t(b+2),
                            b,uint16_t(b+2),uint16_t(b+3)});
  }
  // +X
  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D n(1, 0, 0);
    verts.insert(verts.end(), {F{{x1,y0,z0},n}, F{{x1,y0,z1},n},
                                F{{x1,y1,z1},n}, F{{x1,y1,z0},n}});
    idx.insert(idx.end(), {b,uint16_t(b+1),uint16_t(b+2),
                            b,uint16_t(b+2),uint16_t(b+3)});
  }
  // -X
  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D n(-1, 0, 0);
    verts.insert(verts.end(), {F{{x0,y0,z1},n}, F{{x0,y0,z0},n},
                                F{{x0,y1,z0},n}, F{{x0,y1,z1},n}});
    idx.insert(idx.end(), {b,uint16_t(b+1),uint16_t(b+2),
                            b,uint16_t(b+2),uint16_t(b+3)});
  }
  // +Y
  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D n(0, 1, 0);
    verts.insert(verts.end(), {F{{x0,y1,z0},n}, F{{x0,y1,z1},n},
                                F{{x1,y1,z1},n}, F{{x1,y1,z0},n}});
    idx.insert(idx.end(), {b,uint16_t(b+1),uint16_t(b+2),
                            b,uint16_t(b+2),uint16_t(b+3)});
  }
  // -Y
  {
    auto b = static_cast<uint16_t>(verts.size());
    QVector3D n(0, -1, 0);
    verts.insert(verts.end(), {F{{x0,y0,z1},n}, F{{x0,y0,z0},n},
                                F{{x1,y0,z0},n}, F{{x1,y0,z1},n}});
    idx.insert(idx.end(), {b,uint16_t(b+1),uint16_t(b+2),
                            b,uint16_t(b+2),uint16_t(b+3)});
  }
}

// Append a single quad (two CCW triangles) with an explicit normal.
static void append_quad(
    std::vector<std::pair<QVector3D, QVector3D>> &verts,
    std::vector<uint16_t> &idx,
    const QVector3D &p0, const QVector3D &p1,
    const QVector3D &p2, const QVector3D &p3,
    const QVector3D &n) {
  using F = std::pair<QVector3D, QVector3D>;
  auto b = static_cast<uint16_t>(verts.size());
  verts.insert(verts.end(), {F{p0,n}, F{p1,n}, F{p2,n}, F{p3,n}});
  idx.insert(idx.end(), {b, uint16_t(b+1), uint16_t(b+2),
                          b, uint16_t(b+2), uint16_t(b+3)});
}

// Append a thin disc (N-sided cylinder) with its rotation axis along X.
// Centre at (cx, cy, cz), disc radius r, half-thickness hthk.
static void append_disc_xaxis(
    std::vector<std::pair<QVector3D, QVector3D>> &verts,
    std::vector<uint16_t> &idx,
    float cx, float cy, float cz,
    float r, float hthk, int segs) {
  using F = std::pair<QVector3D, QVector3D>;
  constexpr float k_tau = 6.28318530F;
  const float xa = cx - hthk, xb = cx + hthk;
  for (int i = 0; i < segs; ++i) {
    float a0 = k_tau * i / segs;
    float a1 = k_tau * (i + 1) / segs;
    float y0 = cy + r * std::sin(a0), z0_ = cz + r * std::cos(a0);
    float y1 = cy + r * std::sin(a1), z1_ = cz + r * std::cos(a1);
    float amid = (a0 + a1) * 0.5F;
    QVector3D n(0.0F, std::sin(amid), std::cos(amid));
    append_quad(verts, idx, {xa,y0,z0_},{xa,y1,z1_},{xb,y1,z1_},{xb,y0,z0_}, n);
  }
  // front cap (+X)
  QVector3D ncf(1,0,0);
  QVector3D cf{xb, cy, cz};
  for (int i = 0; i < segs; ++i) {
    float a0 = k_tau * i / segs;
    float a1 = k_tau * (i + 1) / segs;
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {F{cf,ncf},
      F{{xb, cy+r*std::sin(a0), cz+r*std::cos(a0)},ncf},
      F{{xb, cy+r*std::sin(a1), cz+r*std::cos(a1)},ncf}});
    idx.insert(idx.end(), {b, uint16_t(b+1), uint16_t(b+2)});
  }
  // back cap (-X), reversed winding
  QVector3D ncb(-1,0,0);
  QVector3D cb{xa, cy, cz};
  for (int i = 0; i < segs; ++i) {
    float a0 = k_tau * i / segs;
    float a1 = k_tau * (i + 1) / segs;
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {F{cb,ncb},
      F{{xa, cy+r*std::sin(a1), cz+r*std::cos(a1)},ncb},
      F{{xa, cy+r*std::sin(a0), cz+r*std::cos(a0)},ncb}});
    idx.insert(idx.end(), {b, uint16_t(b+1), uint16_t(b+2)});
  }
}

// Append a vertical N-sided prism (axis along +Y) centred at (cx, y0, cz) with top cap.
static void append_vert_prism(
    std::vector<std::pair<QVector3D, QVector3D>> &verts,
    std::vector<uint16_t> &idx,
    float cx, float y0, float cz,
    float r, float height, int segs) {
  using F = std::pair<QVector3D, QVector3D>;
  constexpr float k_tau = 6.28318530F;
  const float y1 = y0 + height;
  for (int i = 0; i < segs; ++i) {
    float a0 = k_tau * i / segs;
    float a1 = k_tau * (i + 1) / segs;
    float x0 = cx + r*std::cos(a0), z0_ = cz + r*std::sin(a0);
    float x1 = cx + r*std::cos(a1), z1_ = cz + r*std::sin(a1);
    float amid = (a0 + a1) * 0.5F;
    QVector3D n(std::cos(amid), 0.0F, std::sin(amid));
    append_quad(verts, idx, {x0,y0,z0_},{x1,y0,z1_},{x1,y1,z1_},{x0,y1,z0_}, n);
  }
  // top cap
  QVector3D tn(0,1,0);
  QVector3D tc{cx,y1,cz};
  for (int i = 0; i < segs; ++i) {
    float a0 = k_tau * i / segs;
    float a1 = k_tau * (i + 1) / segs;
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {F{tc,tn},
      F{{cx+r*std::cos(a0),y1,cz+r*std::sin(a0)},tn},
      F{{cx+r*std::cos(a1),y1,cz+r*std::sin(a1)},tn}});
    idx.insert(idx.end(), {b, uint16_t(b+1), uint16_t(b+2)});
  }
}



// ── Tent pipeline ─────────────────────────────────────────────────────────────
void VegetationPipeline::initialize_tent_pipeline() {
  initializeOpenGLFunctions();
  shutdown_tent_pipeline();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  // Ridge tent: A=front-left, B=front-right, C=front-peak,
  //             D=back-left,  E=back-right,  F=back-peak
  constexpr float H = 0.72F;  // ridge height
  const QVector3D A(-0.50F, 0.0F, -0.50F);
  const QVector3D B( 0.50F, 0.0F, -0.50F);
  const QVector3D C( 0.0F,  H,    -0.50F);
  const QVector3D D(-0.50F, 0.0F,  0.50F);
  const QVector3D E( 0.50F, 0.0F,  0.50F);
  const QVector3D F( 0.0F,  H,     0.50F);

  constexpr float inv_sqrt2 = 0.70711F;
  const QVector3D nL(-inv_sqrt2, inv_sqrt2, 0.0F);
  const QVector3D nR( inv_sqrt2, inv_sqrt2, 0.0F);

  using P = std::pair<QVector3D, QVector3D>;

  // Left roof slope
  {
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{A,nL}, P{D,nL}, P{F,nL}, P{C,nL}});
    idx.insert(idx.end(), {b, uint16_t(b+1), uint16_t(b+2),
                            b, uint16_t(b+2), uint16_t(b+3)});
  }
  // Right roof slope
  {
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{B,nR}, P{C,nR}, P{F,nR}, P{E,nR}});
    idx.insert(idx.end(), {b, uint16_t(b+1), uint16_t(b+2),
                            b, uint16_t(b+2), uint16_t(b+3)});
  }
  // Front gable triangle
  {
    const QVector3D nF(0.0F, 0.0F, -1.0F);
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{A,nF}, P{C,nF}, P{B,nF}});
    idx.insert(idx.end(), {b, uint16_t(b+1), uint16_t(b+2)});
  }
  // Back gable triangle
  {
    const QVector3D nBk(0.0F, 0.0F, 1.0F);
    auto b = static_cast<uint16_t>(verts.size());
    verts.insert(verts.end(), {P{D,nBk}, P{E,nBk}, P{F,nBk}});
    idx.insert(idx.end(), {b, uint16_t(b+1), uint16_t(b+2)});
  }
  // Ground canvas / floor
  append_box(verts, idx, {-0.50F, -0.02F, -0.50F}, { 0.50F, 0.00F,  0.50F});
  // Center support pole
  append_box(verts, idx, {-0.025F, 0.00F, -0.03F},  { 0.025F, H*0.88F, 0.03F});
  // Front door frame: two vertical posts + horizontal lintel
  append_box(verts, idx, {-0.22F,  0.00F, -0.51F},  {-0.16F, 0.40F, -0.48F});
  append_box(verts, idx, { 0.16F,  0.00F, -0.51F},  { 0.22F, 0.40F, -0.48F});
  append_box(verts, idx, {-0.22F,  0.37F, -0.51F},  { 0.22F, 0.43F, -0.48F});
  // Four corner ground stakes
  append_box(verts, idx, {-0.62F, 0.00F, -0.62F},   {-0.55F, 0.07F, -0.55F});
  append_box(verts, idx, { 0.55F, 0.00F, -0.62F},   { 0.62F, 0.07F, -0.55F});
  append_box(verts, idx, {-0.62F, 0.00F,  0.55F},   {-0.55F, 0.07F,  0.62F});
  append_box(verts, idx, { 0.55F, 0.00F,  0.55F},   { 0.62F, 0.07F,  0.62F});

  upload_prop_mesh_impl(verts, idx,
                   m_tent_vao, m_tent_vertex_buffer, m_tent_index_buffer,
                   m_tent_vertex_count, m_tent_index_count);
}

void VegetationPipeline::shutdown_tent_pipeline() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_tent_vao = m_tent_vertex_buffer = m_tent_index_buffer = 0;
    m_tent_vertex_count = m_tent_index_count = 0;
    return;
  }
  initializeOpenGLFunctions();
  delete_prop_pipeline_impl(m_tent_vao, m_tent_vertex_buffer,
                       m_tent_index_buffer, m_tent_vertex_count, m_tent_index_count);
}

// ── Supply Cart pipeline ───────────────────────────────────────────────────────
void VegetationPipeline::initialize_supply_cart_pipeline() {
  initializeOpenGLFunctions();
  shutdown_supply_cart_pipeline();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  // Cart floor board
  append_box(verts, idx, {-0.48F, 0.22F, -0.28F}, { 0.48F, 0.28F,  0.28F});
  // Long side boards
  append_box(verts, idx, {-0.48F, 0.22F, -0.30F}, { 0.48F, 0.44F, -0.25F});
  append_box(verts, idx, {-0.48F, 0.22F,  0.25F}, { 0.48F, 0.44F,  0.30F});
  // Short end boards
  append_box(verts, idx, {-0.50F, 0.22F, -0.28F}, {-0.44F, 0.44F,  0.28F});
  append_box(verts, idx, { 0.44F, 0.22F, -0.28F}, { 0.50F, 0.44F,  0.28F});
  // Axle
  append_box(verts, idx, {-0.56F, 0.18F, -0.04F}, { 0.56F, 0.24F,  0.04F});
  // Left wheel: 8-sided disc rotating around X axis, centre y=0.20
  append_disc_xaxis(verts, idx, -0.56F, 0.20F, 0.0F, 0.20F, 0.05F, 8);
  // Right wheel
  append_disc_xaxis(verts, idx,  0.56F, 0.20F, 0.0F, 0.20F, 0.05F, 8);
  // Two forward handles / shaft poles
  append_box(verts, idx, {-0.18F, 0.18F, -0.28F}, {-0.12F, 0.24F, -0.82F});
  append_box(verts, idx, { 0.12F, 0.18F, -0.28F}, { 0.18F, 0.24F, -0.82F});
  // Handle crossbar
  append_box(verts, idx, {-0.18F, 0.18F, -0.78F}, { 0.18F, 0.24F, -0.74F});
  // Cargo mound (sacks/crates)
  append_box(verts, idx, {-0.34F, 0.44F, -0.22F}, { 0.34F, 0.66F,  0.22F});

  upload_prop_mesh_impl(verts, idx,
                   m_supply_cart_vao, m_supply_cart_vertex_buffer,
                   m_supply_cart_index_buffer,
                   m_supply_cart_vertex_count, m_supply_cart_index_count);
}

void VegetationPipeline::shutdown_supply_cart_pipeline() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_supply_cart_vao = m_supply_cart_vertex_buffer = m_supply_cart_index_buffer = 0;
    m_supply_cart_vertex_count = m_supply_cart_index_count = 0;
    return;
  }
  initializeOpenGLFunctions();
  delete_prop_pipeline_impl(m_supply_cart_vao, m_supply_cart_vertex_buffer,
                       m_supply_cart_index_buffer,
                       m_supply_cart_vertex_count, m_supply_cart_index_count);
}

// ── Weapon Rack pipeline ───────────────────────────────────────────────────────
void VegetationPipeline::initialize_weapon_rack_pipeline() {
  initializeOpenGLFunctions();
  shutdown_weapon_rack_pipeline();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  // Left post
  append_box(verts, idx, {-0.38F, 0.00F, -0.05F}, {-0.30F, 0.72F,  0.05F});
  // Right post
  append_box(verts, idx, { 0.30F, 0.00F, -0.05F}, { 0.38F, 0.72F,  0.05F});
  // Upper crossbeam
  append_box(verts, idx, {-0.38F, 0.52F, -0.04F}, { 0.38F, 0.59F,  0.04F});
  // Lower stabiliser bar
  append_box(verts, idx, {-0.38F, 0.16F, -0.04F}, { 0.38F, 0.21F,  0.04F});

  // Three spears leaning rearward on the crossbeam (tilted in XY plane using append_quad).
  // Each spear is a thin parallelogram: base near floor, tip above crossbeam.
  constexpr float T = 0.025F;   // half-thickness of shaft
  // Spear 1 (left)
  {
    QVector3D bL(-0.20F, 0.0F,  T), bR(-0.20F, 0.0F, -T);
    QVector3D eL(-0.24F, 0.98F, T-0.09F), eR(-0.24F, 0.98F, -T-0.09F);
    QVector3D n = QVector3D::crossProduct(eL - bL, bR - bL).normalized();
    append_quad(verts, idx, bL, bR, eR, eL, n);
    append_quad(verts, idx, eL, eR, bR, bL, -n);
    // spear tip box
    append_box(verts, idx, {-0.26F, 0.96F, -0.10F}, {-0.22F, 1.06F, -0.06F});
  }
  // Spear 2 (centre)
  {
    QVector3D bL(0.00F, 0.0F,  T), bR(0.00F, 0.0F, -T);
    QVector3D eL(0.00F, 1.00F, T-0.08F), eR(0.00F, 1.00F, -T-0.08F);
    QVector3D n = QVector3D::crossProduct(eL - bL, bR - bL).normalized();
    append_quad(verts, idx, bL, bR, eR, eL, n);
    append_quad(verts, idx, eL, eR, bR, bL, -n);
    append_box(verts, idx, {-0.02F, 0.98F, -0.10F}, { 0.02F, 1.08F, -0.06F});
  }
  // Spear 3 (right)
  {
    QVector3D bL( 0.20F, 0.0F,  T), bR( 0.20F, 0.0F, -T);
    QVector3D eL( 0.24F, 0.98F, T-0.09F), eR( 0.24F, 0.98F, -T-0.09F);
    QVector3D n = QVector3D::crossProduct(eL - bL, bR - bL).normalized();
    append_quad(verts, idx, bL, bR, eR, eL, n);
    append_quad(verts, idx, eL, eR, bR, bL, -n);
    append_box(verts, idx, { 0.22F, 0.96F, -0.10F}, { 0.26F, 1.06F, -0.06F});
  }

  upload_prop_mesh_impl(verts, idx,
                   m_weapon_rack_vao, m_weapon_rack_vertex_buffer,
                   m_weapon_rack_index_buffer,
                   m_weapon_rack_vertex_count, m_weapon_rack_index_count);
}

void VegetationPipeline::shutdown_weapon_rack_pipeline() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_weapon_rack_vao = m_weapon_rack_vertex_buffer = m_weapon_rack_index_buffer = 0;
    m_weapon_rack_vertex_count = m_weapon_rack_index_count = 0;
    return;
  }
  initializeOpenGLFunctions();
  delete_prop_pipeline_impl(m_weapon_rack_vao, m_weapon_rack_vertex_buffer,
                       m_weapon_rack_index_buffer,
                       m_weapon_rack_vertex_count, m_weapon_rack_index_count);
}

// ── Ruins pipeline ─────────────────────────────────────────────────────────────
void VegetationPipeline::initialize_ruins_pipeline() {
  initializeOpenGLFunctions();
  shutdown_ruins_pipeline();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  // Column 1 (left, tallest): stone base + 6-sided shaft + broken capital
  append_box(verts, idx, {-0.27F, 0.00F, -0.11F}, {-0.07F, 0.05F,  0.11F});
  append_vert_prism(verts, idx, -0.17F, 0.05F, 0.00F, 0.08F, 0.48F, 6);
  append_box(verts, idx, {-0.27F, 0.53F, -0.11F}, {-0.07F, 0.62F,  0.11F});

  // Column 2 (right, shorter, no capital – snapped off)
  append_box(verts, idx, { 0.07F, 0.00F, -0.11F}, { 0.27F, 0.05F,  0.11F});
  append_vert_prism(verts, idx,  0.17F, 0.05F, 0.00F, 0.08F, 0.32F, 6);

  // Column 3 (rear-centre, tallest, with capital)
  append_box(verts, idx, {-0.10F, 0.00F,  0.07F}, { 0.10F, 0.05F,  0.28F});
  append_vert_prism(verts, idx,  0.00F, 0.05F, 0.175F, 0.09F, 0.60F, 6);
  append_box(verts, idx, {-0.12F, 0.65F,  0.055F}, { 0.12F, 0.74F,  0.295F});

  // Fallen lintel (thick slab lying on the ground at a slight rise)
  append_box(verts, idx, {-0.32F, 0.05F, -0.06F}, { 0.10F, 0.12F,  0.18F});

  // Rubble chunks
  append_box(verts, idx, { 0.10F, 0.00F, -0.12F}, { 0.28F, 0.09F,  0.04F});
  append_box(verts, idx, {-0.30F, 0.00F,  0.04F}, {-0.14F, 0.07F,  0.20F});

  upload_prop_mesh_impl(verts, idx,
                   m_ruins_vao, m_ruins_vertex_buffer,
                   m_ruins_index_buffer,
                   m_ruins_vertex_count, m_ruins_index_count);
}

void VegetationPipeline::shutdown_ruins_pipeline() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_ruins_vao = m_ruins_vertex_buffer = m_ruins_index_buffer = 0;
    m_ruins_vertex_count = m_ruins_index_count = 0;
    return;
  }
  initializeOpenGLFunctions();
  delete_prop_pipeline_impl(m_ruins_vao, m_ruins_vertex_buffer,
                       m_ruins_index_buffer,
                       m_ruins_vertex_count, m_ruins_index_count);
}

// ── Dead Tree pipeline ─────────────────────────────────────────────────────────
void VegetationPipeline::initialize_dead_tree_pipeline() {
  initializeOpenGLFunctions();
  shutdown_dead_tree_pipeline();

  std::vector<std::pair<QVector3D, QVector3D>> verts;
  std::vector<uint16_t> idx;

  // Tapered trunk: three stacked boxes narrowing with height
  append_box(verts, idx, {-0.10F, 0.00F, -0.10F}, { 0.10F, 0.44F,  0.10F});
  append_box(verts, idx, {-0.07F, 0.40F, -0.07F}, { 0.07F, 0.80F,  0.07F});
  append_box(verts, idx, {-0.05F, 0.76F, -0.05F}, { 0.05F, 1.02F,  0.05F});

  // Root flares spreading out near the ground
  append_box(verts, idx, {-0.18F, 0.00F, -0.14F}, {-0.08F, 0.14F, -0.07F});
  append_box(verts, idx, { 0.08F, 0.00F, -0.14F}, { 0.18F, 0.14F, -0.07F});
  append_box(verts, idx, {-0.14F, 0.00F,  0.07F}, { 0.14F, 0.09F,  0.16F});

  // Main left branch: rises from y=0.55 and reaches left-up using append_quad.
  {
    constexpr float T = 0.030F;
    QVector3D bL(-0.05F, 0.55F,  T), bR(-0.05F, 0.55F, -T);
    QVector3D eL(-0.52F, 0.84F,  T), eR(-0.52F, 0.84F, -T);
    QVector3D n = QVector3D::crossProduct(eL - bL, bR - bL).normalized();
    append_quad(verts, idx, bL, bR, eR, eL,  n);
    append_quad(verts, idx, eL, eR, bR, bL, -n);
    // small end cap
    append_box(verts, idx, {-0.54F, 0.82F, -T}, {-0.50F, 0.86F, T});
  }

  // Main right branch: rises from y=0.62 reaching right-up
  {
    constexpr float T = 0.030F;
    QVector3D bL( 0.05F, 0.62F,  T), bR( 0.05F, 0.62F, -T);
    QVector3D eL( 0.50F, 0.88F,  T), eR( 0.50F, 0.88F, -T);
    QVector3D n = QVector3D::crossProduct(eL - bL, bR - bL).normalized();
    append_quad(verts, idx, bL, bR, eR, eL,  n);
    append_quad(verts, idx, eL, eR, bR, bL, -n);
    append_box(verts, idx, { 0.48F, 0.86F, -T}, { 0.52F, 0.90F, T});
  }

  // Rear branch: rises backward and up from y=0.72
  {
    constexpr float T = 0.025F;
    QVector3D bL(-T, 0.72F, -0.05F), bR( T, 0.72F, -0.05F);
    QVector3D eL(-T, 0.94F,  0.38F), eR( T, 0.94F,  0.38F);
    QVector3D n = QVector3D::crossProduct(eL - bL, bR - bL).normalized();
    append_quad(verts, idx, bL, bR, eR, eL,  n);
    append_quad(verts, idx, eL, eR, bR, bL, -n);
  }

  // Sub-branch off the left main branch
  {
    constexpr float T = 0.018F;
    QVector3D bL(-0.30F, 0.68F,  T), bR(-0.30F, 0.68F, -T);
    QVector3D eL(-0.30F, 0.88F, -0.24F+T), eR(-0.30F, 0.88F, -0.24F-T);
    QVector3D n = QVector3D::crossProduct(eL - bL, bR - bL).normalized();
    append_quad(verts, idx, bL, bR, eR, eL,  n);
    append_quad(verts, idx, eL, eR, bR, bL, -n);
  }

  // Top stub
  append_box(verts, idx, {-0.04F, 0.98F, -0.04F}, { 0.04F, 1.16F,  0.04F});

  upload_prop_mesh_impl(verts, idx,
                   m_dead_tree_vao, m_dead_tree_vertex_buffer,
                   m_dead_tree_index_buffer,
                   m_dead_tree_vertex_count, m_dead_tree_index_count);
}

void VegetationPipeline::shutdown_dead_tree_pipeline() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_dead_tree_vao = m_dead_tree_vertex_buffer = m_dead_tree_index_buffer = 0;
    m_dead_tree_vertex_count = m_dead_tree_index_count = 0;
    return;
  }
  initializeOpenGLFunctions();
  delete_prop_pipeline_impl(m_dead_tree_vao, m_dead_tree_vertex_buffer,
                       m_dead_tree_index_buffer,
                       m_dead_tree_vertex_count, m_dead_tree_index_count);
}

} // namespace Render::GL::BackendPipelines
