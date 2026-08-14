#include "backend.h"

#include <QDebug>
#include <QOpenGLContext>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qopenglcontext.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "backend/banner_pipeline.h"
#include "backend/combat_dust_pipeline.h"
#include "backend/cylinder_pipeline.h"
#include "backend/effects_pipeline.h"
#include "backend/fog_range.h"
#include "backend/ground_marker_pipeline.h"
#include "backend/healer_aura_pipeline.h"
#include "backend/healing_beam_pipeline.h"
#include "backend/mesh_instancing_pipeline.h"
#include "backend/mode_indicator_pipeline.h"
#include "backend/post_process_pipeline.h"
#include "backend/primitive_batch_pipeline.h"
#include "backend/rain_pipeline.h"
#include "backend/rigged_character_pipeline.h"
#include "backend/rigged_cull_pipeline.h"
#include "backend/shader_uniform_cache.h"
#include "backend/terrain_pipeline.h"
#include "backend/vegetation_pipeline.h"
#include "backend/water_pipeline.h"
#include "buffer.h"
#include "decoration_gpu.h"
#include "directional_shadow_block.h"
#include "gl/resources.h"
#include "mesh.h"
#include "platform_gl.h"
#include "render/draw_queue.h"
#include "render/geom/mode_indicator.h"
#include "render/geom/selection_disc.h"
#include "render/gl/shared_geometry_cache.h"
#include "render/graphics_settings.h"
#include "render/local_lighting.h"
#include "render/material.h"
#include "render/primitive_batch.h"
#include "render/rain_gpu.h"
#include "render/rigged_mesh.h"
#include "render_constants.h"
#include "scene/camera.h"
#include "shader.h"
#include "state_scopes.h"
#include "texture.h"
#include "ubo_bindings.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {

const QVector3D k_grid_line_color(0.22F, 0.25F, 0.22F);

} // namespace

Backend::Backend() = default;
Backend::Backend(ShaderQuality quality)
    : m_shader_quality(quality) {
}

Backend::~Backend() {
  if (QOpenGLContext::currentContext() == nullptr) {

    for_each_pipeline_slot([](auto& slot) { (void)slot.release(); });
    return;
  }

  if (m_frame_ubo != 0) {
    glDeleteBuffers(1, &m_frame_ubo);
    m_frame_ubo = 0;
  }
  if (m_environment_lighting_ubo != 0) {
    glDeleteBuffers(1, &m_environment_lighting_ubo);
    m_environment_lighting_ubo = 0;
  }
  if (m_local_lighting_ubo != 0) {
    glDeleteBuffers(1, &m_local_lighting_ubo);
    m_local_lighting_ubo = 0;
  }
  release_directional_shadow_resources();
  if (m_directional_shadow_ubo != 0) {
    glDeleteBuffers(1, &m_directional_shadow_ubo);
    m_directional_shadow_ubo = 0;
  }

  SharedGeometryCache::instance().release_all();
  for_each_pipeline_slot([](auto& slot) { slot.reset(); });
}

auto Backend::initialize() -> bool {
  qInfo() << "Backend::initialize() - Starting...";

  qInfo() << "Backend: Initializing OpenGL functions...";
  if (!initializeOpenGLFunctions()) {
    qCritical() << "Backend::initialize() FAILED: QOpenGLFunctions_3_3_Core could not"
                   " be initialized. The current OpenGL context does not support"
                   " OpenGL 3.3 Core Profile. Check that GPU drivers are up to date"
                   " and that the application window has a valid Core Profile context.";
    return false;
  }
  glGenBuffers(1, &m_frame_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, m_frame_ubo);
  glBufferData(GL_UNIFORM_BUFFER, 64, nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, k_frame_data_binding_point, m_frame_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  qInfo() << "Backend: Frame UBO created at binding 0";
  glGenBuffers(1, &m_environment_lighting_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, m_environment_lighting_ubo);
  constexpr GLsizeiptr environment_ubo_size =
      sizeof(float) * EnvironmentLightingState::k_packed_float_count;
  glBufferData(GL_UNIFORM_BUFFER, environment_ubo_size, nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER,
                   k_environment_lighting_binding_point,
                   m_environment_lighting_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  qInfo() << "Backend: Environment lighting UBO created at binding 1";
  glGenBuffers(1, &m_local_lighting_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, m_local_lighting_ubo);
  constexpr GLsizeiptr local_lighting_ubo_size =
      sizeof(float) * Render::LocalLightingBlock::k_float_count;
  glBufferData(GL_UNIFORM_BUFFER, local_lighting_ubo_size, nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(
      GL_UNIFORM_BUFFER, k_local_lighting_binding_point, m_local_lighting_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  qInfo() << "Backend: Local lighting UBO created at binding 2";
  glGenBuffers(1, &m_directional_shadow_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, m_directional_shadow_ubo);
  glBufferData(GL_UNIFORM_BUFFER,
               sizeof(float) * DirectionalShadowBlock::k_float_count,
               nullptr,
               GL_DYNAMIC_DRAW);
  glBindBufferBase(
      GL_UNIFORM_BUFFER, k_directional_shadow_binding_point, m_directional_shadow_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  qInfo() << "Backend: Directional shadow UBO created at binding 3";
  qInfo() << "Backend: OpenGL functions initialized";

  qInfo() << "Backend: Setting up depth test...";
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthRange(0.0, 1.0);
  glDepthMask(GL_TRUE);

  qInfo() << "Backend: Setting up blending...";
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  qInfo() << "Backend: Creating ResourceManager...";
  m_resources = std::make_unique<ResourceManager>();
  if (!m_resources->initialize()) {
    qCritical()
        << "Backend::initialize() FAILED: ResourceManager initialization failed";
    return false;
  }
  qInfo() << "Backend: ResourceManager created";

  qInfo() << "Backend: Creating ShaderCache...";
  m_shader_cache = std::make_unique<ShaderCache>();
  m_shader_cache->initialize_defaults();
  qInfo() << "Backend: ShaderCache created";

  if (!create_subsystem(
          m_cylinder_pipeline, "CylinderPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_vegetation_pipeline, "VegetationPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(m_terrain_pipeline, "TerrainPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_shader_uniform_cache, "ShaderUniformCache", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(m_rigged_character_pipeline,
                        "RiggedCharacterPipeline",
                        this,
                        m_shader_cache.get())) {
    return false;
  }
  m_rigged_cull_pipeline = std::make_unique<BackendPipelines::RiggedCullPipeline>();
  m_rigged_cull_pipeline->set_shader_cache(m_shader_cache.get());
  if (!m_rigged_cull_pipeline->initialize()) {
    m_rigged_cull_pipeline.reset();
  }

  if (!create_subsystem(m_water_pipeline, "WaterPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(m_effects_pipeline, "EffectsPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_primitive_batch_pipeline, "PrimitiveBatchPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(m_banner_pipeline, "BannerPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_healing_beam_pipeline, "HealingBeamPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_healer_aura_pipeline, "HealerAuraPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_combat_dust_pipeline, "CombatDustPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(m_rain_pipeline, "RainPipeline", this, m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_ground_marker_pipeline, "GroundMarkerPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_mode_indicator_pipeline, "ModeIndicatorPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_mesh_instancing_pipeline, "MeshInstancingPipeline", m_shader_cache.get())) {
    return false;
  }
  if (!create_subsystem(
          m_post_process_pipeline, "PostProcessPipeline", m_shader_cache.get())) {
    return false;
  }

  qInfo() << "Backend: Loading basic shaders...";
  m_basic_shader = m_shader_cache->get(QStringLiteral("basic"));
  m_grid_shader = m_shader_cache->get(QStringLiteral("grid"));
  m_shadow_shader = m_shader_cache->get(QStringLiteral("troop_shadow"));
  m_directional_shadow_depth_shader = m_shader_cache->get_or_load(
      QStringLiteral(":/assets/shaders/directional_shadow_depth.vert"),
      QStringLiteral(":/assets/shaders/directional_shadow_depth.frag"));
  m_directional_shadow_rigged_shader = m_shader_cache->get_or_load(
      QStringLiteral(":/assets/shaders/directional_shadow_rigged.vert"),
      QStringLiteral(":/assets/shaders/directional_shadow_depth.frag"));
  if (m_directional_shadow_rigged_shader != nullptr) {
    (void)m_directional_shadow_rigged_shader->bind_uniform_block(
        "BonePalette", k_bone_palette_binding_point);
  }
  if (m_basic_shader == nullptr) {
    qCritical() << "Backend::initialize() FAILED: required basic shader missing";
    return false;
  }
  if (m_grid_shader == nullptr) {
    qCritical() << "Backend::initialize() FAILED: required grid shader missing";
    return false;
  }
  if (m_shadow_shader == nullptr) {
    qCritical() << "Backend::initialize() FAILED: required troop shadow shader missing";
    return false;
  }

  MaterialRegistry::instance().init(m_basic_shader, m_shadow_shader);
  qInfo() << "Backend::initialize() - Complete!";
  return true;
}

auto Backend::banner_mesh() const -> Mesh* {
  if (m_banner_pipeline != nullptr) {
    return m_banner_pipeline->get_banner_mesh();
  }
  return nullptr;
}

auto Backend::banner_shader() const -> Shader* {
  if (m_banner_pipeline != nullptr) {
    return m_banner_pipeline->m_banner_shader;
  }
  return nullptr;
}

void Backend::begin_frame() {
  if (m_viewport_width > 0 && m_viewport_height > 0) {
    glViewport(0, 0, m_viewport_width, m_viewport_height);
  }
  if (m_post_process_pipeline != nullptr) {
    (void)m_post_process_pipeline->begin_scene(m_viewport_width, m_viewport_height);
  }
  glClearColor(m_clear_color[red],
               m_clear_color[green],
               m_clear_color[blue],
               m_clear_color[alpha]);

  glClearDepth(1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glBindVertexArray(0);
  glUseProgram(0);
  glActiveTexture(GL_TEXTURE0);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  Platform::set_blend_equation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  if (m_cylinder_pipeline) {
    m_cylinder_pipeline->begin_frame();
  }
  if (m_mesh_instancing_pipeline) {
    m_mesh_instancing_pipeline->begin_frame();
  }
  if (m_rigged_character_pipeline) {
    m_rigged_character_pipeline->begin_frame();
  }
}

void Backend::set_viewport(int w, int h) {
  m_viewport_width = w;
  m_viewport_height = h;
}

void Backend::set_clear_color(float r, float g, float b, float a) {
  m_clear_color[red] = r;
  m_clear_color[green] = g;
  m_clear_color[blue] = b;
  m_clear_color[alpha] = a;
}

void Backend::release_directional_shadow_resources() {
  if (m_directional_shadow_texture != 0) {
    glDeleteTextures(1, &m_directional_shadow_texture);
    m_directional_shadow_texture = 0;
  }
  if (m_directional_shadow_fbo != 0) {
    glDeleteFramebuffers(1, &m_directional_shadow_fbo);
    m_directional_shadow_fbo = 0;
  }
  m_directional_shadow_resolution = 0;
  m_directional_shadow_cascades = 0;
}

void Backend::ensure_directional_shadow_resources(int resolution, int cascades) {
  resolution = std::clamp(resolution, 512, 4096);
  cascades = std::clamp(cascades, 1, 4);
  if (m_directional_shadow_texture != 0 &&
      m_directional_shadow_resolution == resolution &&
      m_directional_shadow_cascades == cascades) {
    return;
  }

  release_directional_shadow_resources();
  glGenTextures(1, &m_directional_shadow_texture);
  glBindTexture(GL_TEXTURE_2D_ARRAY, m_directional_shadow_texture);
  glTexImage3D(GL_TEXTURE_2D_ARRAY,
               0,
               GL_DEPTH_COMPONENT24,
               resolution,
               resolution,
               cascades,
               0,
               GL_DEPTH_COMPONENT,
               GL_FLOAT,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  const GLfloat border[] = {1.0F, 1.0F, 1.0F, 1.0F};
  glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

  glGenFramebuffers(1, &m_directional_shadow_fbo);
  m_directional_shadow_resolution = resolution;
  m_directional_shadow_cascades = cascades;
}

void Backend::render_directional_shadows(const DrawQueue& queue, const Camera& cam) {
  const auto& settings = Render::GraphicsSettings::instance().directional_shadows();
  const bool enabled = settings.enabled &&
                       m_environment_lighting.shadow_strength > 0.0F &&
                       m_directional_shadow_depth_shader != nullptr &&
                       m_directional_shadow_rigged_shader != nullptr;

  const auto upload_shadow_block = [this](const DirectionalShadowBlock& block) {
    if (m_directional_shadow_ubo == 0) {
      return;
    }
    const auto packed = block.packed_std140();
    glBindBuffer(GL_UNIFORM_BUFFER, m_directional_shadow_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(sizeof(float) * packed.size()),
                    packed.data());
    glBindBufferBase(GL_UNIFORM_BUFFER,
                     k_directional_shadow_binding_point,
                     m_directional_shadow_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
  };

  if (!enabled) {
    upload_shadow_block(DirectionalShadowBlock{});
    return;
  }

  const int cascade_count =
      std::clamp(settings.cascade_count, 1, k_max_shadow_cascades);
  ensure_directional_shadow_resources(settings.resolution, cascade_count);
  if (m_directional_shadow_fbo == 0 || m_directional_shadow_texture == 0) {
    return;
  }

  upload_shadow_block(DirectionalShadowBlock{});

  const float near_distance = std::max(cam.get_near(), 0.05F);
  const float far_distance =
      std::max(near_distance + 1.0F, std::min(settings.distance, cam.get_far()));
  constexpr float split_lambda = 0.68F;
  for (int cascade = 0; cascade < cascade_count; ++cascade) {
    const float p = static_cast<float>(cascade + 1) / static_cast<float>(cascade_count);
    const float logarithmic = near_distance * std::pow(far_distance / near_distance, p);
    const float uniform = near_distance + (far_distance - near_distance) * p;
    m_directional_shadow_splits[cascade] =
        logarithmic * split_lambda + uniform * (1.0F - split_lambda);
  }
  for (int cascade = cascade_count; cascade < 4; ++cascade) {
    m_directional_shadow_splits[cascade] = far_distance;
  }

  bool invertible = false;
  const QMatrix4x4 inverse_view_projection =
      cam.get_view_projection_matrix().inverted(&invertible);
  if (!invertible) {
    return;
  }
  std::array<QVector3D, 4> full_near{};
  std::array<QVector3D, 4> full_far{};
  int corner = 0;
  for (int y : {-1, 1}) {
    for (int x : {-1, 1}) {
      const QVector4D near_h = inverse_view_projection * QVector4D(x, y, -1.0F, 1.0F);
      const QVector4D far_h = inverse_view_projection * QVector4D(x, y, 1.0F, 1.0F);
      full_near[corner] = near_h.toVector3DAffine();
      full_far[corner] = far_h.toVector3DAffine();
      ++corner;
    }
  }

  QVector3D light_direction = m_environment_lighting.primary_direction.normalized();
  const QVector3D light_up =
      std::abs(QVector3D::dotProduct(light_direction, QVector3D(0, 1, 0))) > 0.96F
          ? QVector3D(0, 0, 1)
          : QVector3D(0, 1, 0);
  const QVector3D light_right =
      QVector3D::crossProduct(light_up, light_direction).normalized();
  const QVector3D stable_up =
      QVector3D::crossProduct(light_direction, light_right).normalized();

  GLint previous_fbo = 0;
  GLint previous_viewport[4]{};
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);
  glGetIntegerv(GL_VIEWPORT, previous_viewport);
  const GLboolean previous_blend = glIsEnabled(GL_BLEND);
  const GLboolean previous_cull = glIsEnabled(GL_CULL_FACE);
  glBindFramebuffer(GL_FRAMEBUFFER, m_directional_shadow_fbo);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glViewport(0, 0, m_directional_shadow_resolution, m_directional_shadow_resolution);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(2.0F, 4.0F);

  const auto render_static_mesh = [&](Mesh* mesh,
                                      const QMatrix4x4& model,
                                      const QMatrix4x4& light_vp,
                                      float cascade_near_distance,
                                      float cascade_distance) {
    if (mesh == nullptr) {
      return;
    }
    const QVector3D world_center = model.map(mesh->bounds_center());
    const float scale = std::max({model.column(0).toVector3D().length(),
                                  model.column(1).toVector3D().length(),
                                  model.column(2).toVector3D().length()});
    const float world_radius = mesh->bounds_radius() * scale;
    const float camera_distance = (world_center - cam.get_position()).length();
    if (camera_distance > cascade_distance + world_radius ||
        camera_distance + world_radius < cascade_near_distance) {
      return;
    }
    m_directional_shadow_depth_shader->use();
    m_directional_shadow_depth_shader->set_uniform("u_light_vp", light_vp);
    m_directional_shadow_depth_shader->set_uniform("u_model", model);
    mesh->draw();
  };

  m_shadow_static_casters.clear();
  for (const auto& item : queue.items()) {
    if (const auto* mesh = std::get_if<MeshCmd>(&item)) {
      if (mesh->alpha >= k_opaque_threshold && mesh->shader != m_shadow_shader) {
        m_shadow_static_casters.push_back({mesh->mesh, &mesh->model});
      }
    } else if (const auto* part = std::get_if<DrawPartCmd>(&item)) {
      if (part->alpha >= k_opaque_threshold) {
        m_shadow_static_casters.push_back({part->mesh, &part->world});
      }
    } else if (const auto* terrain = std::get_if<TerrainSurfaceCmd>(&item)) {
      m_shadow_static_casters.push_back({terrain->mesh, &terrain->model});
    }
  }

  float cascade_near = near_distance;
  for (int cascade = 0; cascade < cascade_count; ++cascade) {
    const float cascade_far = m_directional_shadow_splits[cascade];
    const float near_ratio =
        (cascade_near - near_distance) / (far_distance - near_distance);
    const float far_ratio =
        (cascade_far - near_distance) / (far_distance - near_distance);
    std::array<QVector3D, 8> corners{};
    QVector3D center;
    for (int i = 0; i < 4; ++i) {
      const QVector3D ray = full_far[i] - full_near[i];
      corners[i] = full_near[i] + ray * near_ratio;
      corners[i + 4] = full_near[i] + ray * far_ratio;
      center += corners[i] + corners[i + 4];
    }
    center /= 8.0F;
    float radius = 1.0F;
    for (const QVector3D& value : corners) {
      radius = std::max(radius, (value - center).length());
    }
    radius = std::ceil(radius * 16.0F) / 16.0F;
    const float world_texel =
        (radius * 2.0F) / static_cast<float>(m_directional_shadow_resolution);
    center -= light_right *
              std::fmod(QVector3D::dotProduct(center, light_right), world_texel);
    center -=
        stable_up * std::fmod(QVector3D::dotProduct(center, stable_up), world_texel);

    QMatrix4x4 light_view;
    light_view.lookAt(center + light_direction * (radius * 2.5F), center, stable_up);
    QMatrix4x4 light_projection;
    light_projection.ortho(-radius, radius, -radius, radius, 0.1F, radius * 6.0F);
    const QMatrix4x4 light_vp = light_projection * light_view;
    m_directional_shadow_matrices[cascade] = light_vp;

    glFramebufferTextureLayer(
        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_directional_shadow_texture, 0, cascade);
    glClear(GL_DEPTH_BUFFER_BIT);

    for (const auto& caster : m_shadow_static_casters) {
      render_static_mesh(
          caster.mesh, *caster.model, light_vp, cascade_near, cascade_far);
    }

    const auto draw_single_rigged_shadow = [&](const RiggedCreatureCmd& rigged) {
      glBindBufferRange(GL_UNIFORM_BUFFER,
                        k_bone_palette_binding_point,
                        rigged.palette_ubo,
                        static_cast<GLintptr>(rigged.palette_offset),
                        static_cast<GLsizeiptr>(64U * sizeof(QMatrix4x4)));
      m_directional_shadow_rigged_shader->use();
      m_directional_shadow_rigged_shader->set_uniform("u_light_vp", light_vp);
      m_directional_shadow_rigged_shader->set_uniform("u_model", rigged.world);
      m_directional_shadow_rigged_shader->set_uniform("u_variation_scale",
                                                      rigged.variation_scale);
      RiggedMesh* const cast_mesh =
          rigged.shadow_mesh != nullptr ? rigged.shadow_mesh : rigged.mesh;
      cast_mesh->draw();
      ++m_last_playback_stats.shadow_rigged_single_draws;
    };

    thread_local std::vector<const RiggedCreatureCmd*> visible_rigged;
    const std::size_t rigged_cap =
        m_rigged_character_pipeline != nullptr
            ? std::max<std::size_t>(
                  1U, m_rigged_character_pipeline->max_instances_per_batch())
            : 1U;
    for (const PreparedBatch& prepared : queue.prepared_batches()) {
      if (prepared.count == 0U || prepared.type != DrawCmdType::RiggedCreature) {
        continue;
      }
      visible_rigged.clear();
      for (std::size_t index = prepared.start; index < prepared.end(); ++index) {
        const auto& rigged = std::get<RiggedCreatureCmdIndex>(queue.get_sorted(index));
        const float camera_distance =
            (rigged.world.column(3).toVector3D() - cam.get_position()).length();
        if (rigged.mesh == nullptr || rigged.bone_palette == nullptr ||
            rigged.alpha < k_opaque_threshold || camera_distance > cascade_far + 8.0F ||
            camera_distance + 8.0F < cascade_near) {
          continue;
        }
        visible_rigged.push_back(&rigged);
      }

      if (m_rigged_cull_pipeline != nullptr &&
          m_rigged_cull_pipeline->has_shadow_path() && visible_rigged.size() >= 2U) {
        if (m_rigged_cull_pipeline->draw_full_mesh_shadow(
                visible_rigged.data(), visible_rigged.size(), light_vp)) {
          m_last_playback_stats.shadow_rigged_instanced_instances +=
              visible_rigged.size();
          m_last_playback_stats.shadow_rigged_instanced_draws +=
              m_rigged_cull_pipeline->last_stats().draw_calls;
          continue;
        }
        auto const extent = static_cast<float>(m_directional_shadow_resolution);
        if (visible_rigged.size() >=
                BackendPipelines::RiggedCullPipeline::minimum_instances() &&
            m_rigged_cull_pipeline->draw_shadow(visible_rigged.data(),
                                                visible_rigged.size(),
                                                light_vp,
                                                QVector2D(extent, extent))) {
          m_last_playback_stats.shadow_rigged_instanced_instances +=
              visible_rigged.size();
          ++m_last_playback_stats.shadow_rigged_instanced_draws;
          continue;
        }
      }

      std::size_t start = 0U;
      while (start < visible_rigged.size()) {
        const std::size_t count = std::min(rigged_cap, visible_rigged.size() - start);
        bool const instanced = m_rigged_character_pipeline != nullptr &&
                               m_rigged_character_pipeline->draw_shadow_instanced(
                                   visible_rigged.data() + start, count, light_vp);
        if (instanced) {
          if (count > 1U) {
            ++m_last_playback_stats.shadow_rigged_instanced_draws;
            m_last_playback_stats.shadow_rigged_instanced_instances += count;
          } else {
            ++m_last_playback_stats.shadow_rigged_single_draws;
          }
        } else {
          for (std::size_t offset = 0; offset < count; ++offset) {
            if (visible_rigged[start + offset]->palette_ubo != 0U) {
              draw_single_rigged_shadow(*visible_rigged[start + offset]);
            }
          }
        }
        start += count;
      }
    }

    bool shadow_polygon_offset_enabled = true;
    CommandExecutionContext shadow_context{queue,
                                           cam,
                                           light_view,
                                           light_projection,
                                           light_vp,
                                           0.0F,
                                           shadow_polygon_offset_enabled};
    m_last_bound_shader = nullptr;
    m_last_bound_texture = nullptr;
    for (const PreparedBatch& prepared : queue.prepared_batches()) {
      if (prepared.count == 0) {
        continue;
      }
      const auto* scatter =
          std::get_if<TerrainScatterCmd>(&queue.get_sorted(prepared.start));
      if (scatter == nullptr ||
          scatter->species == TerrainScatterCmd::Species::FireCamp ||
          scatter->species == TerrainScatterCmd::Species::Stone) {
        continue;
      }
      execute_scatter_commands(prepared, shadow_context);
    }
    cascade_near = cascade_far;
  }

  glDisable(GL_POLYGON_OFFSET_FILL);
  glCullFace(GL_BACK);
  if (previous_cull == 0U) {
    glDisable(GL_CULL_FACE);
  }
  if (previous_blend != 0U) {
    glEnable(GL_BLEND);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_fbo));
  glViewport(previous_viewport[0],
             previous_viewport[1],
             previous_viewport[2],
             previous_viewport[3]);

  DirectionalShadowBlock block;
  for (int cascade = 0; cascade < k_max_shadow_cascades; ++cascade) {
    const auto slot = static_cast<std::size_t>(cascade);
    block.light_view_projection[slot] = m_directional_shadow_matrices[slot];
    block.split_distances[slot] = m_directional_shadow_splits[slot];
  }
  block.enabled = 1.0F;
  block.cascade_count = static_cast<float>(cascade_count);
  block.shadow_map_texel_size =
      1.0F / static_cast<float>(m_directional_shadow_resolution);
  const int weather_softening = m_environment_lighting.shadow_softness > 0.65F ? 1 : 0;
  block.pcf_radius =
      static_cast<float>(std::clamp(settings.pcf_radius + weather_softening, 1, 3));
  block.camera_position = cam.get_position();
  block.depth_bias = settings.depth_bias;
  block.normal_bias = settings.normal_bias;
  block.cascade_blend = settings.cascade_blend;

  const auto complete = block.packed_std140();
  glBindBuffer(GL_UNIFORM_BUFFER, m_directional_shadow_ubo);
  glBufferData(GL_UNIFORM_BUFFER,
               static_cast<GLsizeiptr>(sizeof(float) * complete.size()),
               nullptr,
               GL_DYNAMIC_DRAW);
  glBufferSubData(GL_UNIFORM_BUFFER,
                  0,
                  static_cast<GLsizeiptr>(sizeof(float) * complete.size()),
                  complete.data());
  glBindBufferBase(
      GL_UNIFORM_BUFFER, k_directional_shadow_binding_point, m_directional_shadow_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  glActiveTexture(GL_TEXTURE0 + TextureUnit::directional_shadow_map);
  glBindTexture(GL_TEXTURE_2D_ARRAY, m_directional_shadow_texture);
  glActiveTexture(GL_TEXTURE0);
}

void Backend::upload_frame_uniform_buffers(const QMatrix4x4& view_proj,
                                           const DrawQueue& queue,
                                           const Camera& cam) {
  if (m_frame_ubo != 0) {
    glBindBuffer(GL_UNIFORM_BUFFER, m_frame_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, view_proj.constData());

    glBindBufferBase(GL_UNIFORM_BUFFER, k_frame_data_binding_point, m_frame_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
  }
  if (m_environment_lighting_ubo != 0) {
    const auto packed = m_environment_lighting.packed_std140();
    glBindBuffer(GL_UNIFORM_BUFFER, m_environment_lighting_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(packed.size() * sizeof(float)),
                    packed.data());
    glBindBufferBase(GL_UNIFORM_BUFFER,
                     k_environment_lighting_binding_point,
                     m_environment_lighting_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
  }
  if (m_local_lighting_ubo != 0) {
    std::vector<Render::LocalLight> candidates;
    for (const auto& item : queue.items()) {
      const auto* effect = std::get_if<EffectBatchCmd>(&item);
      if (effect == nullptr) {
        continue;
      }
      if (effect->kind != EffectBatchCmd::Kind::BuildingFlame &&
          effect->kind != EffectBatchCmd::Kind::BurningFlame &&
          effect->kind != EffectBatchCmd::Kind::Fireball) {
        continue;
      }
      Render::LocalLight light;
      light.position = effect->position + QVector3D(0.0F, 0.8F, 0.0F);
      light.color =
          effect->color.isNull() ? QVector3D(1.0F, 0.48F, 0.16F) : effect->color;
      light.radius = std::clamp(effect->radius * 5.0F, 3.0F, 12.0F);
      light.intensity = std::clamp(effect->intensity, 0.0F, 2.5F);
      candidates.push_back(light);
    }
    const auto& submitted_lights = queue.local_lights();
    candidates.insert(
        candidates.end(), submitted_lights.begin(), submitted_lights.end());
    const auto selected =
        m_local_light_fader.update(candidates, cam.get_position(), m_animation_time);
    const auto packed = Render::pack_local_lights_std140(selected);
    glBindBuffer(GL_UNIFORM_BUFFER, m_local_lighting_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER,
                    0,
                    static_cast<GLsizeiptr>(packed.size() * sizeof(float)),
                    packed.data());
    glBindBufferBase(
        GL_UNIFORM_BUFFER, k_local_lighting_binding_point, m_local_lighting_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
  }
}

void Backend::execute_scene(const DrawQueue& queue, const Camera& cam) {
  m_last_playback_stats = {};
  m_last_playback_stats.submitted_commands = queue.size();
  m_last_playback_stats.prepared_batches = queue.prepared_batches().size();
  m_frame_tracker.begin_frame();

  if (m_basic_shader == nullptr) {
    m_frame_tracker.mark_complete();
    m_frame_tracker.end_frame();
    return;
  }

  const QMatrix4x4 view = cam.get_view_matrix();
  const QMatrix4x4 projection = cam.get_projection_matrix();
  const QMatrix4x4 view_proj = projection * view;
  upload_frame_uniform_buffers(view_proj, queue, cam);
  render_directional_shadows(queue, cam);
  if (m_post_process_pipeline != nullptr && m_post_process_pipeline->is_capturing()) {
    m_post_process_pipeline->set_depth_range(cam.get_near(), cam.get_far());
    const auto [fog_start, fog_end] = fog_range_for_camera(cam);
    m_post_process_pipeline->set_atmosphere(
        view_proj.inverted(), cam.get_position(), fog_start, fog_end, m_animation_time);
    if (m_mist_volumes_dirty) {
      m_post_process_pipeline->set_mist(m_mist_volumes);
      m_mist_volumes_dirty = false;
    }
    m_post_process_pipeline->draw_sky(view_proj, cam.get_position());
  }
  const float banner_wind_strength = 0.8F + 0.2F * std::sin(m_animation_time * 0.5F);

  m_last_bound_shader = nullptr;
  m_last_bound_texture = nullptr;

  bool polygon_offset_enabled = (glIsEnabled(GL_POLYGON_OFFSET_FILL) != 0U);

  const auto& prepared_batches = queue.prepared_batches();
  CommandExecutionContext context{queue,
                                  cam,
                                  view,
                                  projection,
                                  view_proj,
                                  banner_wind_strength,
                                  polygon_offset_enabled};

  std::size_t frame_rigged_commands = 0;
  m_rigged_drawn_this_frame = 0;

  std::size_t batch_index = 0;
  while (batch_index < prepared_batches.size()) {
    const PreparedBatch& prepared = prepared_batches[batch_index];
    const std::size_t i = prepared.start;
    const std::size_t batch_end = prepared.end();
    const auto& cmd = queue.get_sorted(i);
    if (prepared.type == DrawCmdType::RiggedCreature) {
      frame_rigged_commands += prepared.count;
    }
    switch (draw_cmd_type(cmd)) {
    case DrawCmdType::Cylinder:
    case DrawCmdType::FogBatch:
      execute_cylinder_commands(prepared, context);
      break;
    case DrawCmdType::TerrainScatter:
      execute_scatter_commands(prepared, context);
      break;
    case DrawCmdType::TerrainSurface:
      execute_terrain_commands(prepared, context);
      break;
    case DrawCmdType::TerrainFeature:
      execute_water_linear_commands(prepared, context);
      break;
    case DrawCmdType::Mesh:
    case DrawCmdType::DrawPart:
      execute_mesh_commands(prepared, context);
      break;
    case DrawCmdType::RainBatch:
    case DrawCmdType::Grid:
    case DrawCmdType::GroundMarker:
    case DrawCmdType::SelectionSmoke:
    case DrawCmdType::PrimitiveBatch:
    case DrawCmdType::EffectBatch:
    case DrawCmdType::ModeIndicator:
      execute_effects_commands(prepared, context);
      break;
    case DrawCmdType::RiggedCreature:
      execute_rigged_commands(prepared, context);
      break;
    }

    for (std::size_t executed = 0; executed < prepared.count; ++executed) {
      m_frame_tracker.record_executed();
    }
    if (m_frame_budget_config.allow_partial_render &&
        m_frame_tracker.should_defer(m_frame_budget_config)) {
      const std::size_t next_batch = batch_index + 1;
      if (next_batch < prepared_batches.size()) {
        const std::size_t next_i = prepared_batches[next_batch].start;
        auto next_prio = extract_cmd_priority(queue.get_sorted(next_i));
        if (next_prio >= CommandPriority::Low) {

          bool only_low_remaining = true;
          for (std::size_t b = next_batch; b < prepared_batches.size(); ++b) {
            const PreparedBatch& remaining = prepared_batches[b];
            for (std::size_t j = remaining.start; j < remaining.end(); ++j) {
              if (extract_cmd_priority(queue.get_sorted(j)) < CommandPriority::Low) {
                only_low_remaining = false;
                break;
              }
            }
            if (!only_low_remaining) {
              break;
            }
          }
          if (only_low_remaining) {
            for (std::size_t b = next_batch; b < prepared_batches.size(); ++b) {
              const PreparedBatch& remaining = prepared_batches[b];
              for (std::size_t j = remaining.start; j < remaining.end(); ++j) {
                m_frame_tracker.record_deferred();
              }
            }
            break;
          }
        }
      }
    }

    ++batch_index;
  }
  if (m_last_bound_shader != nullptr) {
    m_last_bound_shader->release();
    m_last_bound_shader = nullptr;
  }

  if (m_rigged_character_pipeline) {
    m_rigged_character_pipeline->end_frame();
  }

  m_frame_tracker.mark_complete();
  m_frame_tracker.end_frame();
}

void Backend::execute(const DrawQueue& queue, const Camera& cam) {
  execute_scene(queue, cam);
  if (m_post_process_pipeline != nullptr) {
    m_post_process_pipeline->resolve_scene();
  }
}

} // namespace Render::GL
