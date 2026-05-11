#include "combat_dust_pipeline.h"
#include "../../../game/core/component.h"
#include "../../../game/core/world.h"
#include "../backend.h"
#include "../camera.h"
#include "../render_constants.h"
#include "../shader_cache.h"
#include <QDebug>
#include <QMatrix4x4>
#include <QOpenGLContext>
#include <cmath>
#include <numbers>

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {
constexpr float k_min_dust_intensity = 0.01F;
constexpr float k_default_dust_radius = 2.0F;
constexpr float k_default_dust_intensity = 0.6F;
constexpr float k_dust_color_r = 0.6F;
constexpr float k_dust_color_g = 0.55F;
constexpr float k_dust_color_b = 0.45F;
constexpr float k_dust_y_offset = 0.05F;

constexpr float k_default_flame_radius = 3.0F;
constexpr float k_default_flame_intensity = 0.8F;
constexpr float k_flame_color_r = 1.0F;
constexpr float k_flame_color_g = 0.4F;
constexpr float k_flame_color_b = 0.1F;
constexpr float k_flame_y_offset = 0.5F;
constexpr float k_building_health_threshold = 0.5F;
constexpr float k_default_blood_radius = 0.6F;
constexpr float k_blood_y_offset = 0.02F;
constexpr float k_blood_alpha_scale = 1.0F;

void clear_gl_errors() {
#ifndef NDEBUG
  while (glGetError() != GL_NO_ERROR) {
  }
#endif
}

auto check_gl_error(const char *operation) -> bool {
#ifndef NDEBUG
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "CombatDustPipeline GL error in" << operation << ":" << err;
    return false;
  }
#else
  Q_UNUSED(operation);
#endif
  return true;
}
} // namespace

auto CombatDustPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "CombatDustPipeline::initialize: null ShaderCache";
    return false;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  m_dust_shader = m_shader_cache->get("combat_dust");
  if (m_dust_shader == nullptr) {
    m_dust_shader = m_shader_cache->load(
        "combat_dust", QStringLiteral(":/assets/shaders/combat_dust.vert"),
        QStringLiteral(":/assets/shaders/combat_dust.frag"));
  }
  if (m_dust_shader == nullptr) {
    qWarning() << "CombatDustPipeline: Failed to get combat_dust shader";
    return false;
  }

  m_blood_shader = m_shader_cache->get("blood_pool");
  if (m_blood_shader == nullptr) {
    m_blood_shader = m_shader_cache->load(
        "blood_pool", QStringLiteral(":/assets/shaders/blood_pool.vert"),
        QStringLiteral(":/assets/shaders/blood_pool.frag"));
    if (m_blood_shader == nullptr) {
      qWarning() << "CombatDustPipeline: Blood pools disabled; failed to load "
                    "blood_pool shader";
    }
  }

  cache_uniforms();

  if (!create_dust_geometry()) {
    qWarning() << "CombatDustPipeline: Failed to create dust geometry";
    return false;
  }

  if (m_blood_shader != nullptr && !create_blood_geometry()) {
    qWarning() << "CombatDustPipeline: Blood pools disabled; failed to create "
                  "blood geometry";
    shutdown_blood_geometry();
    m_blood_shader = nullptr;
    m_blood_uniforms = {};
  }

  qInfo() << "CombatDustPipeline initialized successfully";
  return is_initialized();
}

void CombatDustPipeline::shutdown() {
  shutdown_blood_geometry();
  shutdown_geometry();
  m_blood_shader = nullptr;
  m_dust_shader = nullptr;
  m_blood_data.clear();
  m_dust_data.clear();
}

void CombatDustPipeline::shutdown_geometry() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_vao = 0;
    m_vertex_buffer = 0;
    m_index_buffer = 0;
    m_index_count = 0;
    return;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  if (m_vao != 0) {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
  }
  if (m_vertex_buffer != 0) {
    glDeleteBuffers(1, &m_vertex_buffer);
    m_vertex_buffer = 0;
  }
  if (m_index_buffer != 0) {
    glDeleteBuffers(1, &m_index_buffer);
    m_index_buffer = 0;
  }
  m_index_count = 0;
}

void CombatDustPipeline::shutdown_blood_geometry() {
  if (QOpenGLContext::currentContext() == nullptr) {
    m_blood_vao = 0;
    m_blood_vertex_buffer = 0;
    m_blood_index_buffer = 0;
    m_blood_index_count = 0;
    return;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  if (m_blood_vao != 0) {
    glDeleteVertexArrays(1, &m_blood_vao);
    m_blood_vao = 0;
  }
  if (m_blood_vertex_buffer != 0) {
    glDeleteBuffers(1, &m_blood_vertex_buffer);
    m_blood_vertex_buffer = 0;
  }
  if (m_blood_index_buffer != 0) {
    glDeleteBuffers(1, &m_blood_index_buffer);
    m_blood_index_buffer = 0;
  }
  m_blood_index_count = 0;
}

void CombatDustPipeline::cache_uniforms() {
  m_uniforms = {};
  m_blood_uniforms = {};

  if (m_dust_shader != nullptr) {
    m_uniforms.mvp = m_dust_shader->uniform_handle("u_mvp");
    m_uniforms.model = m_dust_shader->uniform_handle("u_model");
    m_uniforms.time = m_dust_shader->uniform_handle("u_time");
    m_uniforms.center = m_dust_shader->uniform_handle("u_center");
    m_uniforms.radius = m_dust_shader->uniform_handle("u_radius");
    m_uniforms.intensity = m_dust_shader->uniform_handle("u_intensity");
    m_uniforms.dust_color = m_dust_shader->uniform_handle("u_dust_color");
    m_uniforms.effect_type = m_dust_shader->uniform_handle("u_effect_type");
  }

  if (m_blood_shader != nullptr) {
    m_blood_uniforms.mvp = m_blood_shader->uniform_handle("u_mvp");
    m_blood_uniforms.radius = m_blood_shader->uniform_handle("u_radius");
    m_blood_uniforms.alpha_scale =
        m_blood_shader->uniform_handle("u_alpha_scale");
  }
}

auto CombatDustPipeline::is_initialized() const -> bool {
  return m_dust_shader != nullptr && m_vao != 0 && m_index_count > 0;
}

struct DustVertex {
  float position[3];
  float normal[3];
  float tex_coord[2];
};

auto CombatDustPipeline::create_dust_geometry() -> bool {
  initializeOpenGLFunctions();
  shutdown_geometry();
  clear_gl_errors();

  std::vector<DustVertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int rings = 6;
  constexpr int segments = 16;
  constexpr float pi = std::numbers::pi_v<float>;

  constexpr int height_levels = 8;
  constexpr int angle_segments = 12;
  constexpr float max_height = 1.0F;

  vertices.reserve(
      static_cast<size_t>((height_levels + 1) * (angle_segments + 1)));

  for (int h = 0; h <= height_levels; ++h) {
    float height_t = static_cast<float>(h) / static_cast<float>(height_levels);
    float y = height_t * max_height;

    float radius_at_height = 1.0F - height_t * 0.3F;

    for (int a = 0; a <= angle_segments; ++a) {
      float angle_t =
          static_cast<float>(a) / static_cast<float>(angle_segments);
      float theta = angle_t * pi * 2.0F;
      float x = radius_at_height * std::cos(theta);
      float z = radius_at_height * std::sin(theta);

      DustVertex v;
      v.position[0] = x;
      v.position[1] = y;
      v.position[2] = z;

      v.normal[0] = std::cos(theta);
      v.normal[1] = 0.3F;
      v.normal[2] = std::sin(theta);

      v.tex_coord[0] = angle_t;
      v.tex_coord[1] = height_t;
      vertices.push_back(v);
    }
  }

  indices.reserve(static_cast<size_t>(height_levels * angle_segments * 6));
  for (int h = 0; h < height_levels; ++h) {
    for (int a = 0; a < angle_segments; ++a) {
      unsigned int curr =
          static_cast<unsigned int>(h * (angle_segments + 1) + a);
      unsigned int next = curr + static_cast<unsigned int>(angle_segments + 1);

      indices.push_back(curr);
      indices.push_back(next);
      indices.push_back(curr + 1);

      indices.push_back(curr + 1);
      indices.push_back(next);
      indices.push_back(next + 1);
    }
  }

  glGenVertexArrays(1, &m_vao);
  if (!check_gl_error("glGenVertexArrays") || m_vao == 0) {
    return false;
  }

  glBindVertexArray(m_vao);
  if (!check_gl_error("glBindVertexArray")) {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
    return false;
  }

  glGenBuffers(1, &m_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(DustVertex)),
               vertices.data(), GL_STATIC_DRAW);
  if (!check_gl_error("vertex buffer")) {
    shutdown_geometry();
    return false;
  }

  glGenBuffers(1, &m_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
               indices.data(), GL_STATIC_DRAW);
  if (!check_gl_error("index buffer")) {
    shutdown_geometry();
    return false;
  }

  m_index_count = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(VertexAttrib::Position);
  glVertexAttribPointer(
      VertexAttrib::Position, ComponentCount::Vec3, GL_FLOAT, GL_FALSE,
      sizeof(DustVertex),
      reinterpret_cast<void *>(offsetof(DustVertex, position)));

  glEnableVertexAttribArray(VertexAttrib::Normal);
  glVertexAttribPointer(VertexAttrib::Normal, ComponentCount::Vec3, GL_FLOAT,
                        GL_FALSE, sizeof(DustVertex),
                        reinterpret_cast<void *>(offsetof(DustVertex, normal)));

  glEnableVertexAttribArray(VertexAttrib::TexCoord);
  glVertexAttribPointer(
      VertexAttrib::TexCoord, ComponentCount::Vec2, GL_FLOAT, GL_FALSE,
      sizeof(DustVertex),
      reinterpret_cast<void *>(offsetof(DustVertex, tex_coord)));

  glBindVertexArray(0);

  if (!check_gl_error("vertex attributes")) {
    shutdown_geometry();
    return false;
  }

  return true;
}

auto CombatDustPipeline::create_blood_geometry() -> bool {
  initializeOpenGLFunctions();
  shutdown_blood_geometry();
  clear_gl_errors();

  std::vector<DustVertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int segments = 16;
  constexpr float pi = std::numbers::pi_v<float>;

  vertices.reserve(static_cast<size_t>(segments + 1));
  indices.reserve(static_cast<size_t>(segments * 3));

  DustVertex center{};
  center.position[0] = 0.0F;
  center.position[1] = 0.0F;
  center.position[2] = 0.0F;
  center.normal[0] = 0.0F;
  center.normal[1] = 1.0F;
  center.normal[2] = 0.0F;
  center.tex_coord[0] = 0.5F;
  center.tex_coord[1] = 0.5F;
  vertices.push_back(center);

  for (int i = 0; i < segments; ++i) {
    float angle =
        static_cast<float>(i) / static_cast<float>(segments) * pi * 2.0F;
    float x = std::cos(angle);
    float z = std::sin(angle);

    DustVertex vertex{};
    vertex.position[0] = x;
    vertex.position[1] = 0.0F;
    vertex.position[2] = z;
    vertex.normal[0] = 0.0F;
    vertex.normal[1] = 1.0F;
    vertex.normal[2] = 0.0F;
    vertex.tex_coord[0] = 0.5F + 0.5F * x;
    vertex.tex_coord[1] = 0.5F + 0.5F * z;
    vertices.push_back(vertex);
  }

  for (int i = 1; i <= segments; ++i) {
    unsigned int next = static_cast<unsigned int>((i % segments) + 1);
    indices.push_back(0);
    indices.push_back(static_cast<unsigned int>(i));
    indices.push_back(next);
  }

  glGenVertexArrays(1, &m_blood_vao);
  if (!check_gl_error("glGenVertexArrays blood") || m_blood_vao == 0) {
    return false;
  }

  glBindVertexArray(m_blood_vao);
  if (!check_gl_error("glBindVertexArray blood")) {
    glDeleteVertexArrays(1, &m_blood_vao);
    m_blood_vao = 0;
    return false;
  }

  glGenBuffers(1, &m_blood_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_blood_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(DustVertex)),
               vertices.data(), GL_STATIC_DRAW);
  if (!check_gl_error("blood vertex buffer")) {
    shutdown_blood_geometry();
    return false;
  }

  glGenBuffers(1, &m_blood_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_blood_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
               indices.data(), GL_STATIC_DRAW);
  if (!check_gl_error("blood index buffer")) {
    shutdown_blood_geometry();
    return false;
  }

  m_blood_index_count = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(VertexAttrib::Position);
  glVertexAttribPointer(
      VertexAttrib::Position, ComponentCount::Vec3, GL_FLOAT, GL_FALSE,
      sizeof(DustVertex),
      reinterpret_cast<void *>(offsetof(DustVertex, position)));

  glEnableVertexAttribArray(VertexAttrib::Normal);
  glVertexAttribPointer(VertexAttrib::Normal, ComponentCount::Vec3, GL_FLOAT,
                        GL_FALSE, sizeof(DustVertex),
                        reinterpret_cast<void *>(offsetof(DustVertex, normal)));

  glEnableVertexAttribArray(VertexAttrib::TexCoord);
  glVertexAttribPointer(
      VertexAttrib::TexCoord, ComponentCount::Vec2, GL_FLOAT, GL_FALSE,
      sizeof(DustVertex),
      reinterpret_cast<void *>(offsetof(DustVertex, tex_coord)));

  glBindVertexArray(0);

  if (!check_gl_error("blood vertex attributes")) {
    shutdown_blood_geometry();
    return false;
  }

  return true;
}

void CombatDustPipeline::collect_combat_zones(Engine::Core::World *world,
                                              float animation_time) {
  if (world == nullptr) {
    return;
  }

  auto units = world->get_entities_with<Engine::Core::UnitComponent>();

  for (auto *unit : units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *unit_comp = unit->get_component<Engine::Core::UnitComponent>();
    auto *transform = unit->get_component<Engine::Core::TransformComponent>();
    auto *attack = unit->get_component<Engine::Core::AttackComponent>();

    if (transform == nullptr || unit_comp == nullptr) {
      continue;
    }

    if (unit_comp->health <= 0) {
      continue;
    }

    if (attack == nullptr || !attack->in_melee_lock) {
      continue;
    }

    CombatDustData data;
    data.position = QVector3D(transform->position.x, k_dust_y_offset,
                              transform->position.z);
    data.radius = k_default_dust_radius;
    data.intensity = k_default_dust_intensity;
    data.color = QVector3D(k_dust_color_r, k_dust_color_g, k_dust_color_b);
    data.time = animation_time;
    data.effect_type = EffectType::Dust;

    m_dust_data.push_back(data);
  }
}

void CombatDustPipeline::collect_building_flames(Engine::Core::World *world,
                                                 float animation_time) {
  if (world == nullptr) {
    return;
  }

  auto buildings = world->get_entities_with<Engine::Core::BuildingComponent>();

  for (auto *building : buildings) {
    if (building->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *unit_comp = building->get_component<Engine::Core::UnitComponent>();
    auto *transform =
        building->get_component<Engine::Core::TransformComponent>();

    if (transform == nullptr || unit_comp == nullptr) {
      continue;
    }

    if (unit_comp->health <= 0) {
      continue;
    }

    float health_ratio = static_cast<float>(unit_comp->health) /
                         static_cast<float>(unit_comp->max_health);

    if (health_ratio > k_building_health_threshold) {
      continue;
    }

    float base_intensity = k_default_flame_intensity * (1.0F - health_ratio);

    float cx = transform->position.x;
    float cz = transform->position.z;

    constexpr float k_building_half_width = 1.5F;
    constexpr float k_building_half_depth = 1.2F;
    constexpr float k_flame_spacing = 0.8F;

    struct FlamePoint {
      float dx, dz, height_offset, intensity_mult, radius_mult;
    };

    FlamePoint flame_points[] = {

        {-k_building_half_width * 0.7F, -k_building_half_depth * 0.7F, 0.8F,
         1.0F, 0.9F},
        {k_building_half_width * 0.7F, -k_building_half_depth * 0.7F, 0.7F,
         0.95F, 0.85F},
        {-k_building_half_width * 0.7F, k_building_half_depth * 0.7F, 0.6F,
         0.9F, 0.8F},
        {k_building_half_width * 0.7F, k_building_half_depth * 0.7F, 0.75F,
         1.0F, 0.9F},

        {0.0F, -k_building_half_depth * 0.8F, 0.9F, 0.85F, 0.7F},
        {0.0F, k_building_half_depth * 0.8F, 0.7F, 0.8F, 0.65F},
        {-k_building_half_width * 0.8F, 0.0F, 0.65F, 0.75F, 0.7F},
        {k_building_half_width * 0.8F, 0.0F, 0.8F, 0.85F, 0.75F},

        {0.0F, 0.0F, 1.0F, 1.1F, 1.0F},
    };

    for (const auto &fp : flame_points) {
      CombatDustData data;
      data.position = QVector3D(cx + fp.dx, k_flame_y_offset + fp.height_offset,
                                cz + fp.dz);
      data.radius = k_default_flame_radius * fp.radius_mult;
      data.intensity = base_intensity * fp.intensity_mult;
      data.color = QVector3D(k_flame_color_r, k_flame_color_g, k_flame_color_b);
      data.time = animation_time;
      data.effect_type = EffectType::Flame;
      m_dust_data.push_back(data);
    }
  }
}

void CombatDustPipeline::collect_blood_pools(Engine::Core::World *world) {
  if (world == nullptr) {
    return;
  }

  auto units = world->get_entities_with<Engine::Core::UnitComponent>();

  for (auto *unit : units) {
    if (unit->has_component<Engine::Core::PendingRemovalComponent>() ||
        unit->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    auto *unit_comp = unit->get_component<Engine::Core::UnitComponent>();
    auto *transform = unit->get_component<Engine::Core::TransformComponent>();
    if (unit_comp == nullptr || transform == nullptr || unit_comp->health > 0) {
      continue;
    }

    BloodPoolData data;
    data.position = QVector3D(transform->position.x, k_blood_y_offset,
                              transform->position.z);
    data.radius = k_default_blood_radius;
    m_blood_data.push_back(data);
  }
}

void CombatDustPipeline::collect_all_effects(Engine::Core::World *world,
                                             float animation_time) {
  m_dust_data.clear();
  m_blood_data.clear();
  collect_combat_zones(world, animation_time);
  collect_building_flames(world, animation_time);
  collect_blood_pools(world);
}

void CombatDustPipeline::add_dust_zone(const QVector3D &position, float radius,
                                       float intensity, const QVector3D &color,
                                       float time) {
  CombatDustData data;
  data.position = position;
  data.radius = radius;
  data.intensity = intensity;
  data.color = color;
  data.time = time;
  data.effect_type = EffectType::Dust;
  m_dust_data.push_back(data);
}

void CombatDustPipeline::add_flame_zone(const QVector3D &position, float radius,
                                        float intensity, const QVector3D &color,
                                        float time) {
  CombatDustData data;
  data.position = position;
  data.radius = radius;
  data.intensity = intensity;
  data.color = color;
  data.time = time;
  data.effect_type = EffectType::Flame;
  m_dust_data.push_back(data);
}

void CombatDustPipeline::render(const Camera &cam, float animation_time) {
  if (!is_initialized() || (m_dust_data.empty() && m_blood_data.empty())) {
    return;
  }

  clear_gl_errors();

  if (!m_dust_data.empty()) {
    GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);
    GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blend_enabled = glIsEnabled(GL_BLEND);
    GLboolean depth_mask_enabled = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask_enabled);

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_dust_shader->use();
    glBindVertexArray(m_vao);

    for (const auto &data : m_dust_data) {
      CombatDustData render_data = data;
      render_data.time = animation_time;
      render_dust(render_data, cam);
    }

    glBindVertexArray(0);

    glDepthMask(depth_mask_enabled);
    if (!blend_enabled) {
      glDisable(GL_BLEND);
    }
    if (depth_test_enabled) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
    if (cull_enabled) {
      glEnable(GL_CULL_FACE);
    }
  }

  if (!m_blood_data.empty()) {
    render_blood_pools(cam);
  }
}

void CombatDustPipeline::render_blood_pools(const Camera &cam) {
  if (m_blood_shader == nullptr || m_blood_vao == 0 ||
      m_blood_index_count <= 0 || m_blood_data.empty()) {
    return;
  }

  QMatrix4x4 view_proj = cam.get_projection_matrix() * cam.get_view_matrix();

  GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);
  GLboolean depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
  GLboolean blend_enabled = glIsEnabled(GL_BLEND);
  GLboolean polygon_offset_enabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
  GLboolean depth_mask_enabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask_enabled);

  GLfloat polygon_offset_factor = 0.0F;
  GLfloat polygon_offset_units = 0.0F;
  glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygon_offset_factor);
  glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygon_offset_units);

  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-1.0F, -1.0F);

  m_blood_shader->use();
  glBindVertexArray(m_blood_vao);

  for (const auto &blood : m_blood_data) {
    QMatrix4x4 model;
    model.setToIdentity();
    model.translate(blood.position);
    model.scale(blood.radius, 1.0F, blood.radius);

    QMatrix4x4 mvp = view_proj * model;
    m_blood_shader->set_uniform(m_blood_uniforms.mvp, mvp);
    m_blood_shader->set_uniform(m_blood_uniforms.radius, blood.radius);
    m_blood_shader->set_uniform(m_blood_uniforms.alpha_scale,
                                k_blood_alpha_scale);

    glDrawElements(GL_TRIANGLES, m_blood_index_count, GL_UNSIGNED_INT, nullptr);
  }

  glBindVertexArray(0);

  glPolygonOffset(polygon_offset_factor, polygon_offset_units);
  if (!polygon_offset_enabled) {
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
  glDepthMask(depth_mask_enabled);
  if (!blend_enabled) {
    glDisable(GL_BLEND);
  }
  if (depth_test_enabled) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  if (cull_enabled) {
    glEnable(GL_CULL_FACE);
  }
}

void CombatDustPipeline::render_dust(const CombatDustData &data,
                                     const Camera &cam) {
  QMatrix4x4 model;
  model.setToIdentity();
  model.translate(data.position);
  model.scale(data.radius);

  QMatrix4x4 vp = cam.get_projection_matrix() * cam.get_view_matrix();
  QMatrix4x4 mvp = vp * model;

  m_dust_shader->set_uniform(m_uniforms.mvp, mvp);
  m_dust_shader->set_uniform(m_uniforms.model, model);
  m_dust_shader->set_uniform(m_uniforms.time, data.time);
  m_dust_shader->set_uniform(m_uniforms.center, data.position);
  m_dust_shader->set_uniform(m_uniforms.radius, data.radius);
  m_dust_shader->set_uniform(m_uniforms.intensity, data.intensity);
  m_dust_shader->set_uniform(m_uniforms.dust_color, data.color);
  m_dust_shader->set_uniform(m_uniforms.effect_type,
                             static_cast<int>(data.effect_type));

  glDrawElements(GL_TRIANGLES, m_index_count, GL_UNSIGNED_INT, nullptr);
}

void CombatDustPipeline::render_single_dust(const QVector3D &position,
                                            const QVector3D &color,
                                            float radius, float intensity,
                                            float time,
                                            const QMatrix4x4 &view_proj) {
  if (!is_initialized()) {
    return;
  }
  if (intensity < k_min_dust_intensity) {
    return;
  }

  GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);
  GLboolean depth_mask_enabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask_enabled);

  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_dust_shader->use();
  glBindVertexArray(m_vao);

  QMatrix4x4 model;
  model.setToIdentity();
  model.translate(position);
  model.scale(radius);

  QMatrix4x4 mvp = view_proj * model;

  m_dust_shader->set_uniform(m_uniforms.mvp, mvp);
  m_dust_shader->set_uniform(m_uniforms.model, model);
  m_dust_shader->set_uniform(m_uniforms.time, time);
  m_dust_shader->set_uniform(m_uniforms.center, position);
  m_dust_shader->set_uniform(m_uniforms.radius, radius);
  m_dust_shader->set_uniform(m_uniforms.intensity, intensity);
  m_dust_shader->set_uniform(m_uniforms.dust_color, color);
  m_dust_shader->set_uniform(m_uniforms.effect_type,
                             static_cast<int>(EffectType::Dust));

  glDrawElements(GL_TRIANGLES, m_index_count, GL_UNSIGNED_INT, nullptr);

  glBindVertexArray(0);

  glDepthMask(depth_mask_enabled);
  if (cull_enabled) {
    glEnable(GL_CULL_FACE);
  }
}

void CombatDustPipeline::render_single_flame(const QVector3D &position,
                                             const QVector3D &color,
                                             float radius, float intensity,
                                             float time,
                                             const QMatrix4x4 &view_proj) {
  if (!is_initialized()) {
    return;
  }
  if (intensity < k_min_dust_intensity) {
    return;
  }

  GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);
  GLboolean depth_mask_enabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask_enabled);

  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_dust_shader->use();
  glBindVertexArray(m_vao);

  QMatrix4x4 model;
  model.setToIdentity();
  model.translate(position);
  model.scale(radius);

  QMatrix4x4 mvp = view_proj * model;

  m_dust_shader->set_uniform(m_uniforms.mvp, mvp);
  m_dust_shader->set_uniform(m_uniforms.model, model);
  m_dust_shader->set_uniform(m_uniforms.time, time);
  m_dust_shader->set_uniform(m_uniforms.center, position);
  m_dust_shader->set_uniform(m_uniforms.radius, radius);
  m_dust_shader->set_uniform(m_uniforms.intensity, intensity);
  m_dust_shader->set_uniform(m_uniforms.dust_color, color);
  m_dust_shader->set_uniform(m_uniforms.effect_type,
                             static_cast<int>(EffectType::Flame));

  glDrawElements(GL_TRIANGLES, m_index_count, GL_UNSIGNED_INT, nullptr);

  glBindVertexArray(0);

  glDepthMask(depth_mask_enabled);
  if (cull_enabled) {
    glEnable(GL_CULL_FACE);
  }
}

void CombatDustPipeline::render_single_stone_impact(
    const QVector3D &position, const QVector3D &color, float radius,
    float intensity, float time, const QMatrix4x4 &view_proj) {
  if (!is_initialized()) {
    return;
  }
  if (intensity < k_min_dust_intensity) {
    return;
  }

  GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);
  GLboolean depth_mask_enabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask_enabled);

  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_dust_shader->use();
  glBindVertexArray(m_vao);

  QMatrix4x4 model;
  model.setToIdentity();
  model.translate(position);
  model.scale(radius);

  QMatrix4x4 mvp = view_proj * model;

  m_dust_shader->set_uniform(m_uniforms.mvp, mvp);
  m_dust_shader->set_uniform(m_uniforms.model, model);
  m_dust_shader->set_uniform(m_uniforms.time, time);
  m_dust_shader->set_uniform(m_uniforms.center, position);
  m_dust_shader->set_uniform(m_uniforms.radius, radius);
  m_dust_shader->set_uniform(m_uniforms.intensity, intensity);
  m_dust_shader->set_uniform(m_uniforms.dust_color, color);
  m_dust_shader->set_uniform(m_uniforms.effect_type,
                             static_cast<int>(EffectType::StoneImpact));

  glDrawElements(GL_TRIANGLES, m_index_count, GL_UNSIGNED_INT, nullptr);

  glBindVertexArray(0);

  glDepthMask(depth_mask_enabled);
  if (cull_enabled) {
    glEnable(GL_CULL_FACE);
  }
}

void CombatDustPipeline::render_dust_batch(const DustInstanceData *instances,
                                           std::size_t count,
                                           const QMatrix4x4 &view_proj) {
  if (!is_initialized() || instances == nullptr || count == 0) {
    return;
  }

  GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);
  GLboolean depth_mask_enabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask_enabled);

  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_dust_shader->use();
  glBindVertexArray(m_vao);

  for (std::size_t idx = 0; idx < count; ++idx) {
    const DustInstanceData &inst = instances[idx];
    if (inst.intensity < k_min_dust_intensity) {
      continue;
    }

    QMatrix4x4 model;
    model.setToIdentity();
    model.translate(inst.position);
    model.scale(inst.radius);

    QMatrix4x4 mvp = view_proj * model;

    m_dust_shader->set_uniform(m_uniforms.mvp, mvp);
    m_dust_shader->set_uniform(m_uniforms.model, model);
    m_dust_shader->set_uniform(m_uniforms.time, inst.time);
    m_dust_shader->set_uniform(m_uniforms.center, inst.position);
    m_dust_shader->set_uniform(m_uniforms.radius, inst.radius);
    m_dust_shader->set_uniform(m_uniforms.intensity, inst.intensity);
    m_dust_shader->set_uniform(m_uniforms.dust_color, inst.color);
    m_dust_shader->set_uniform(m_uniforms.effect_type,
                               static_cast<int>(inst.effect_type));

    glDrawElements(GL_TRIANGLES, m_index_count, GL_UNSIGNED_INT, nullptr);
  }

  glBindVertexArray(0);

  glDepthMask(depth_mask_enabled);
  if (cull_enabled) {
    glEnable(GL_CULL_FACE);
  }
}

} // namespace Render::GL::BackendPipelines
