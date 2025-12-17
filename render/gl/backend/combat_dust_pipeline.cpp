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
constexpr float kMinDustIntensity = 0.01F;
constexpr float kDefaultDustRadius = 2.0F;
constexpr float kDefaultDustIntensity = 0.6F;
constexpr float kDustColorR = 0.6F;
constexpr float kDustColorG = 0.55F;
constexpr float kDustColorB = 0.45F;
constexpr float kDustYOffset = 0.05F;

constexpr float kDefaultFlameRadius = 3.0F;
constexpr float kDefaultFlameIntensity = 0.8F;
constexpr float kFlameColorR = 1.0F;
constexpr float kFlameColorG = 0.4F;
constexpr float kFlameColorB = 0.1F;
constexpr float kFlameYOffset = 0.5F;
constexpr float kBuildingHealthThreshold = 0.5F;

void clear_gl_errors() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

auto check_gl_error(const char *operation) -> bool {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "CombatDustPipeline GL error in" << operation << ":" << err;
    return false;
  }
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

  cache_uniforms();

  if (!create_dust_geometry()) {
    qWarning() << "CombatDustPipeline: Failed to create dust geometry";
    return false;
  }

  qInfo() << "CombatDustPipeline initialized successfully";
  return is_initialized();
}

void CombatDustPipeline::shutdown() {
  shutdown_geometry();
  m_dust_shader = nullptr;
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

void CombatDustPipeline::cache_uniforms() {
  if (m_dust_shader == nullptr) {
    return;
  }

  m_uniforms.mvp = m_dust_shader->uniform_handle("u_mvp");
  m_uniforms.model = m_dust_shader->uniform_handle("u_model");
  m_uniforms.time = m_dust_shader->uniform_handle("u_time");
  m_uniforms.center = m_dust_shader->uniform_handle("u_center");
  m_uniforms.radius = m_dust_shader->uniform_handle("u_radius");
  m_uniforms.intensity = m_dust_shader->uniform_handle("u_intensity");
  m_uniforms.dust_color = m_dust_shader->uniform_handle("u_dust_color");
  m_uniforms.effect_type = m_dust_shader->uniform_handle("u_effect_type");
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
    data.position =
        QVector3D(transform->position.x, kDustYOffset, transform->position.z);
    data.radius = kDefaultDustRadius;
    data.intensity = kDefaultDustIntensity;
    data.color = QVector3D(kDustColorR, kDustColorG, kDustColorB);
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

    if (health_ratio > kBuildingHealthThreshold) {
      continue;
    }

    float base_intensity = kDefaultFlameIntensity * (1.0F - health_ratio);

    float cx = transform->position.x;
    float cz = transform->position.z;

    constexpr float kBuildingHalfWidth = 1.5F;
    constexpr float kBuildingHalfDepth = 1.2F;
    constexpr float kFlameSpacing = 0.8F;

    struct FlamePoint {
      float dx, dz, height_offset, intensity_mult, radius_mult;
    };

    FlamePoint flame_points[] = {

        {-kBuildingHalfWidth * 0.7F, -kBuildingHalfDepth * 0.7F, 0.8F, 1.0F,
         0.9F},
        {kBuildingHalfWidth * 0.7F, -kBuildingHalfDepth * 0.7F, 0.7F, 0.95F,
         0.85F},
        {-kBuildingHalfWidth * 0.7F, kBuildingHalfDepth * 0.7F, 0.6F, 0.9F,
         0.8F},
        {kBuildingHalfWidth * 0.7F, kBuildingHalfDepth * 0.7F, 0.75F, 1.0F,
         0.9F},

        {0.0F, -kBuildingHalfDepth * 0.8F, 0.9F, 0.85F, 0.7F},
        {0.0F, kBuildingHalfDepth * 0.8F, 0.7F, 0.8F, 0.65F},
        {-kBuildingHalfWidth * 0.8F, 0.0F, 0.65F, 0.75F, 0.7F},
        {kBuildingHalfWidth * 0.8F, 0.0F, 0.8F, 0.85F, 0.75F},

        {0.0F, 0.0F, 1.0F, 1.1F, 1.0F},
    };

    for (const auto &fp : flame_points) {
      CombatDustData data;
      data.position =
          QVector3D(cx + fp.dx, kFlameYOffset + fp.height_offset, cz + fp.dz);
      data.radius = kDefaultFlameRadius * fp.radius_mult;
      data.intensity = base_intensity * fp.intensity_mult;
      data.color = QVector3D(kFlameColorR, kFlameColorG, kFlameColorB);
      data.time = animation_time;
      data.effect_type = EffectType::Flame;
      m_dust_data.push_back(data);
    }
  }
}

void CombatDustPipeline::collect_all_effects(Engine::Core::World *world,
                                             float animation_time) {
  m_dust_data.clear();
  collect_combat_zones(world, animation_time);
  collect_building_flames(world, animation_time);
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
  if (!is_initialized() || m_dust_data.empty()) {
    return;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

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
  if (intensity < kMinDustIntensity) {
    return;
  }

  initializeOpenGLFunctions();

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
  if (intensity < kMinDustIntensity) {
    return;
  }

  initializeOpenGLFunctions();

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

} // namespace Render::GL::BackendPipelines
