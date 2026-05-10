#include "backend.h"
#include "../draw_queue.h"
#include "../geom/mode_indicator.h"
#include "../geom/selection_disc.h"
#include "../geom/selection_ring.h"
#include "../graphics_settings.h"
#include "../material.h"
#include "../primitive_batch.h"
#include "../rain_gpu.h"
#include "backend/banner_pipeline.h"
#include "backend/character_pipeline.h"
#include "backend/combat_dust_pipeline.h"
#include "backend/cylinder_pipeline.h"
#include "backend/effects_pipeline.h"
#include "backend/healer_aura_pipeline.h"
#include "backend/healing_beam_pipeline.h"
#include "backend/mesh_instancing_pipeline.h"
#include "backend/mode_indicator_pipeline.h"
#include "backend/primitive_batch_pipeline.h"
#include "backend/rain_pipeline.h"
#include "backend/rigged_character_pipeline.h"
#include "backend/terrain_pipeline.h"
#include "backend/vegetation_pipeline.h"
#include "backend/water_pipeline.h"
#include "buffer.h"
#include "decoration_gpu.h"
#include "gl/camera.h"
#include "gl/resources.h"
#include "mesh.h"
#include "render_constants.h"
#include "shader.h"
#include "state_scopes.h"
#include "texture.h"
#include <GL/gl.h>
#include <QDebug>
#include <QOpenGLContext>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qopenglcontext.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <string>
#include <unordered_set>
#include <vector>

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {

const QVector3D k_grid_line_color(0.22F, 0.25F, 0.22F);

auto render_stage_logging_enabled() -> bool {
  return qEnvironmentVariableIsSet("SOI_RENDER_STAGE_LOG");
}

void log_render_first_use_once(const char *stage, const QString &detail) {
  if (!render_stage_logging_enabled()) {
    return;
  }

  static std::mutex mutex;
  static std::unordered_set<std::string> emitted_stages;

  std::lock_guard<std::mutex> const lock(mutex);
  if (!emitted_stages.emplace(stage).second) {
    return;
  }

  qInfo().noquote() << QStringLiteral("SOI render first-use [%1]: %2")
                           .arg(QString::fromLatin1(stage), detail);
}

} // namespace

Backend::Backend() = default;
Backend::Backend(ShaderQuality quality) : m_shader_quality(quality) {}

Backend::~Backend() {
  if (shader_bind_audit_enabled()) {
    qInfo() << "Shader bind audit:";
    for (const QString &line : format_shader_bind_audit()) {
      qInfo().noquote() << line;
    }
  }

  if (QOpenGLContext::currentContext() == nullptr) {

    (void)m_cylinder_pipeline.release();
    (void)m_vegetation_pipeline.release();
    (void)m_terrain_pipeline.release();
    (void)m_character_pipeline.release();
    (void)m_rigged_character_pipeline.release();
    (void)m_water_pipeline.release();
    (void)m_effects_pipeline.release();
    (void)m_mesh_instancing_pipeline.release();
  } else {

    m_cylinder_pipeline.reset();
    m_vegetation_pipeline.reset();
    m_terrain_pipeline.reset();
    m_character_pipeline.reset();
    m_rigged_character_pipeline.reset();
    m_water_pipeline.reset();
    m_effects_pipeline.reset();
    m_mesh_instancing_pipeline.reset();
  }
}

void Backend::initialize() {
  qInfo() << "Backend::initialize() - Starting...";

  qInfo() << "Backend: Initializing OpenGL functions...";
  initializeOpenGLFunctions();
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
    qWarning() << "Backend: failed to initialize ResourceManager";
  }
  qInfo() << "Backend: ResourceManager created";

  qInfo() << "Backend: Creating ShaderCache...";
  m_shader_cache = std::make_unique<ShaderCache>();
  m_shader_cache->initialize_defaults();
  qInfo() << "Backend: ShaderCache created";

  qInfo() << "Backend: Creating CylinderPipeline...";
  m_cylinder_pipeline =
      std::make_unique<BackendPipelines::CylinderPipeline>(m_shader_cache.get());
  m_cylinder_pipeline->initialize();
  qInfo() << "Backend: CylinderPipeline initialized";

  qInfo() << "Backend: Creating VegetationPipeline...";
  m_vegetation_pipeline = std::make_unique<BackendPipelines::VegetationPipeline>(
      m_shader_cache.get());
  m_vegetation_pipeline->initialize();
  qInfo() << "Backend: VegetationPipeline initialized";

  qInfo() << "Backend: Creating TerrainPipeline...";
  m_terrain_pipeline = std::make_unique<BackendPipelines::TerrainPipeline>(
      this, m_shader_cache.get());
  m_terrain_pipeline->initialize();
  qInfo() << "Backend: TerrainPipeline initialized";

  qInfo() << "Backend: Creating CharacterPipeline...";
  m_character_pipeline = std::make_unique<BackendPipelines::CharacterPipeline>(
      this, m_shader_cache.get());
  m_character_pipeline->initialize();
  qInfo() << "Backend: CharacterPipeline initialized";

  qInfo() << "Backend: Creating RiggedCharacterPipeline...";
  m_rigged_character_pipeline =
      std::make_unique<BackendPipelines::RiggedCharacterPipeline>(
          this, m_shader_cache.get());
  m_rigged_character_pipeline->initialize();
  qInfo() << "Backend: RiggedCharacterPipeline initialized";

  qInfo() << "Backend: Creating WaterPipeline...";
  m_water_pipeline = std::make_unique<BackendPipelines::WaterPipeline>(
      this, m_shader_cache.get());
  m_water_pipeline->initialize();
  qInfo() << "Backend: WaterPipeline initialized";

  qInfo() << "Backend: Creating EffectsPipeline...";
  m_effects_pipeline = std::make_unique<BackendPipelines::EffectsPipeline>(
      this, m_shader_cache.get());
  m_effects_pipeline->initialize();
  qInfo() << "Backend: EffectsPipeline initialized";

  qInfo() << "Backend: Creating PrimitiveBatchPipeline...";
  m_primitive_batch_pipeline =
      std::make_unique<BackendPipelines::PrimitiveBatchPipeline>(
          m_shader_cache.get());
  m_primitive_batch_pipeline->initialize();
  qInfo() << "Backend: PrimitiveBatchPipeline initialized";

  qInfo() << "Backend: Creating BannerPipeline...";
  m_banner_pipeline = std::make_unique<BackendPipelines::BannerPipeline>(
      this, m_shader_cache.get());
  m_banner_pipeline->initialize();
  qInfo() << "Backend: BannerPipeline initialized";

  qInfo() << "Backend: Creating HealingBeamPipeline...";
  m_healing_beam_pipeline =
      std::make_unique<BackendPipelines::HealingBeamPipeline>(
          this, m_shader_cache.get());
  m_healing_beam_pipeline->initialize();
  qInfo() << "Backend: HealingBeamPipeline initialized";

  qInfo() << "Backend: Creating HealerAuraPipeline...";
  m_healer_aura_pipeline = std::make_unique<BackendPipelines::HealerAuraPipeline>(
      this, m_shader_cache.get());
  m_healer_aura_pipeline->initialize();
  qInfo() << "Backend: HealerAuraPipeline initialized";

  qInfo() << "Backend: Creating CombatDustPipeline...";
  m_combat_dust_pipeline = std::make_unique<BackendPipelines::CombatDustPipeline>(
      this, m_shader_cache.get());
  m_combat_dust_pipeline->initialize();
  qInfo() << "Backend: CombatDustPipeline initialized";

  qInfo() << "Backend: Creating RainPipeline...";
  m_rain_pipeline = std::make_unique<BackendPipelines::RainPipeline>(
      this, m_shader_cache.get());
  m_rain_pipeline->initialize();
  qInfo() << "Backend: RainPipeline initialized";

  qInfo() << "Backend: Creating ModeIndicatorPipeline...";
  m_mode_indicator_pipeline =
      std::make_unique<BackendPipelines::ModeIndicatorPipeline>(
          this, m_shader_cache.get());
  m_mode_indicator_pipeline->initialize();
  qInfo() << "Backend: ModeIndicatorPipeline initialized";

  qInfo() << "Backend: Creating MeshInstancingPipeline...";
  m_mesh_instancing_pipeline =
      std::make_unique<BackendPipelines::MeshInstancingPipeline>(
          this, m_shader_cache.get());
  m_mesh_instancing_pipeline->initialize();
  qInfo() << "Backend: MeshInstancingPipeline initialized";

  qInfo() << "Backend: Loading basic shaders...";
  m_basic_shader = m_shader_cache->get(QStringLiteral("basic"));
  m_grid_shader = m_shader_cache->get(QStringLiteral("grid"));
  m_shadow_shader = m_shader_cache->get(QStringLiteral("troop_shadow"));
  if (m_basic_shader == nullptr) {
    qWarning() << "Backend: basic shader missing";
  }
  if (m_grid_shader == nullptr) {
    qWarning() << "Backend: grid shader missing";
  }

  MaterialRegistry::instance().init(m_basic_shader, m_shadow_shader);
  qInfo() << "Backend::initialize() - Complete!";
}

auto Backend::banner_mesh() const -> Mesh * {
  if (m_banner_pipeline != nullptr) {
    return m_banner_pipeline->get_banner_mesh();
  }
  return nullptr;
}

auto Backend::banner_shader() const -> Shader * {
  if (m_banner_pipeline != nullptr) {
    return m_banner_pipeline->m_banner_shader;
  }
  return nullptr;
}

void Backend::begin_frame() {
  if (m_viewport_width > 0 && m_viewport_height > 0) {
    glViewport(0, 0, m_viewport_width, m_viewport_height);
  }
  glClearColor(m_clear_color[Red], m_clear_color[Green], m_clear_color[Blue],
               m_clear_color[Alpha]);

  glClearDepth(1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);

  if (m_cylinder_pipeline) {
    m_cylinder_pipeline->begin_frame();
  }
  if (m_mesh_instancing_pipeline) {
    m_mesh_instancing_pipeline->begin_frame();
  }
}

void Backend::set_viewport(int w, int h) {
  m_viewport_width = w;
  m_viewport_height = h;
}

void Backend::set_clear_color(float r, float g, float b, float a) {
  m_clear_color[Red] = r;
  m_clear_color[Green] = g;
  m_clear_color[Blue] = b;
  m_clear_color[Alpha] = a;
}

void Backend::execute(const DrawQueue &queue, const Camera &cam) {
  m_frame_tracker.begin_frame();

  if (m_basic_shader == nullptr) {
    m_frame_tracker.mark_complete();
    m_frame_tracker.end_frame();
    return;
  }

  const QMatrix4x4 view = cam.get_view_matrix();
  const QMatrix4x4 projection = cam.get_projection_matrix();
  const QMatrix4x4 view_proj = projection * view;
  const float banner_wind_strength =
      0.8F + 0.2F * std::sin(m_animation_time * 0.5F);

  m_last_bound_shader = nullptr;
  m_last_bound_texture = nullptr;

  bool polygon_offset_enabled = (glIsEnabled(GL_POLYGON_OFFSET_FILL) != 0U);

  const auto &prepared_batches = queue.prepared_batches();
  log_render_first_use_once(
      "backend-execute",
      QStringLiteral("first playback has %1 commands and %2 prepared batches")
          .arg(queue.size())
          .arg(prepared_batches.size()));
  const bool rigged_instancing_enabled =
      !qEnvironmentVariableIsSet("SOI_RENDER_DISABLE_RIGGED_INSTANCING");
  const bool debug_rigged =
      qEnvironmentVariableIsSet("SOI_RENDER_DEBUG_RIGGED");
  std::size_t debug_rigged_batches = 0;
  std::size_t debug_rigged_cmds = 0;
  std::size_t debug_rigged_instanced_attempts = 0;
  std::size_t debug_rigged_instanced_successes = 0;
  std::size_t debug_rigged_instanced_failures = 0;
  std::size_t debug_rigged_single_draws = 0;
  std::size_t batch_index = 0;
  while (batch_index < prepared_batches.size()) {
    const PreparedBatch &prepared = prepared_batches[batch_index];
    const std::size_t i = prepared.start;
    const std::size_t batch_end = prepared.end();
    const auto &cmd = queue.get_sorted(i);
    switch (cmd.index()) {
    case CylinderCmdIndex: {
      if (!m_cylinder_pipeline) {
        break;
      }
      m_cylinder_pipeline->m_cylinder_scratch.clear();
      for (std::size_t j = i; j < batch_end; ++j) {
        const auto &cy = std::get<CylinderCmdIndex>(queue.get_sorted(j));
        BackendPipelines::CylinderPipeline::CylinderInstanceGpu gpu{};
        gpu.start = cy.start;
        gpu.end = cy.end;
        gpu.radius = cy.radius;
        gpu.alpha = cy.alpha;
        gpu.color = cy.color;
        m_cylinder_pipeline->m_cylinder_scratch.emplace_back(gpu);
      }

      const std::size_t instance_count =
          m_cylinder_pipeline->m_cylinder_scratch.size();
      if (instance_count > 0 &&
          (m_cylinder_pipeline->cylinder_shader() != nullptr)) {
        glDepthMask(GL_TRUE);
        if (polygon_offset_enabled) {
          glDisable(GL_POLYGON_OFFSET_FILL);
          polygon_offset_enabled = false;
        }
        Shader *cylinder_shader = m_cylinder_pipeline->cylinder_shader();
        if (m_last_bound_shader != cylinder_shader) {
          cylinder_shader->use();
          m_last_bound_shader = cylinder_shader;
          m_last_bound_texture = nullptr;
        }
        if (m_cylinder_pipeline->m_cylinder_uniforms.view_proj !=
            Shader::InvalidUniform) {
          cylinder_shader->set_uniform(
              m_cylinder_pipeline->m_cylinder_uniforms.view_proj, view_proj);
        }
        m_cylinder_pipeline->upload_cylinder_instances(instance_count);
        m_cylinder_pipeline->draw_cylinders(instance_count);
      }
      break;
    }
    case FogBatchCmdIndex: {
      if (!m_cylinder_pipeline) {
        break;
      }
      const auto &batch = std::get<FogBatchCmdIndex>(cmd);
      const FogInstanceData *instances = batch.instances;
      const std::size_t instance_count = batch.count;
      if (((instances != nullptr) || (batch.instance_buffer != nullptr)) &&
          instance_count > 0 && (m_cylinder_pipeline->fog_shader() != nullptr)) {
        DepthMaskScope const depth_mask(false);
        BlendScope const blend(true);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (polygon_offset_enabled) {
          glDisable(GL_POLYGON_OFFSET_FILL);
          polygon_offset_enabled = false;
        }
        Shader *fog_shader = m_cylinder_pipeline->fog_shader();
        if (m_last_bound_shader != fog_shader) {
          fog_shader->use();
          m_last_bound_shader = fog_shader;
          m_last_bound_texture = nullptr;
        }
        if (m_cylinder_pipeline->m_fog_uniforms.view_proj !=
            Shader::InvalidUniform) {
          fog_shader->set_uniform(m_cylinder_pipeline->m_fog_uniforms.view_proj,
                                  view_proj);
        }
        if (m_cylinder_pipeline->m_fog_uniforms.time != Shader::InvalidUniform) {
          fog_shader->set_uniform(m_cylinder_pipeline->m_fog_uniforms.time,
                                  m_animation_time);
        }
        if (batch.instance_buffer != nullptr) {
          m_cylinder_pipeline->bind_fog_instance_buffer(batch.instance_buffer);
        } else {
          m_cylinder_pipeline->m_fog_scratch.resize(instance_count);
          for (std::size_t idx = 0; idx < instance_count; ++idx) {
            BackendPipelines::CylinderPipeline::FogInstanceGpu gpu{};
            gpu.center = instances[idx].center;
            gpu.size = instances[idx].size;
            gpu.color = instances[idx].color;
            gpu.alpha = instances[idx].alpha;
            m_cylinder_pipeline->m_fog_scratch[idx] = gpu;
          }
          m_cylinder_pipeline->upload_fog_instances(instance_count);
        }
        m_cylinder_pipeline->draw_fog(instance_count);
      }
      break;
    }
    case TerrainScatterCmdIndex: {
      const auto &deco_cmd_ = std::get<TerrainScatterCmdIndex>(cmd);
      switch (deco_cmd_.species) {
      case TerrainScatterCmd::Species::Grass: {
        struct GrassView {
          Buffer *instance_buffer;
          std::size_t instance_count;
          const GrassBatchParams &params;
        };
        const GrassView grass{deco_cmd_.instance_buffer,
                              deco_cmd_.instance_count, deco_cmd_.grass};
        if ((grass.instance_buffer == nullptr) || grass.instance_count == 0 ||
            (m_terrain_pipeline->m_grass_shader == nullptr) ||
            (m_terrain_pipeline->m_grass_vao == 0U) ||
            m_terrain_pipeline->m_grass_vertex_count == 0) {
          break;
        }

        DepthMaskScope const depth_mask(false);
        BlendScope const blend(true);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
        if (prev_cull != 0U) {
          glDisable(GL_CULL_FACE);
        }

        if (m_last_bound_shader != m_terrain_pipeline->m_grass_shader) {
          m_terrain_pipeline->m_grass_shader->use();
          m_last_bound_shader = m_terrain_pipeline->m_grass_shader;
          m_last_bound_texture = nullptr;
        }

        if (m_terrain_pipeline->m_grass_uniforms.view_proj !=
            Shader::InvalidUniform) {
          m_terrain_pipeline->m_grass_shader->set_uniform(
              m_terrain_pipeline->m_grass_uniforms.view_proj, view_proj);
        }
        if (m_terrain_pipeline->m_grass_uniforms.time != Shader::InvalidUniform) {
          m_terrain_pipeline->m_grass_shader->set_uniform(
              m_terrain_pipeline->m_grass_uniforms.time, grass.params.time);
        }
        if (m_terrain_pipeline->m_grass_uniforms.wind_strength !=
            Shader::InvalidUniform) {
          m_terrain_pipeline->m_grass_shader->set_uniform(
              m_terrain_pipeline->m_grass_uniforms.wind_strength,
              grass.params.wind_strength);
        }
        if (m_terrain_pipeline->m_grass_uniforms.wind_speed !=
            Shader::InvalidUniform) {
          m_terrain_pipeline->m_grass_shader->set_uniform(
              m_terrain_pipeline->m_grass_uniforms.wind_speed,
              grass.params.wind_speed);
        }
        if (m_terrain_pipeline->m_grass_uniforms.soil_color !=
            Shader::InvalidUniform) {
          m_terrain_pipeline->m_grass_shader->set_uniform(
              m_terrain_pipeline->m_grass_uniforms.soil_color,
              grass.params.soil_color);
        }
        if (m_terrain_pipeline->m_grass_uniforms.light_dir !=
            Shader::InvalidUniform) {
          QVector3D light_dir = grass.params.light_direction;
          if (!light_dir.isNull()) {
            light_dir.normalize();
          }
          m_terrain_pipeline->m_grass_shader->set_uniform(
              m_terrain_pipeline->m_grass_uniforms.light_dir, light_dir);
        }

        glBindVertexArray(m_terrain_pipeline->m_grass_vao);
        grass.instance_buffer->bind();
        const auto stride = static_cast<GLsizei>(sizeof(GrassInstanceGpu));
        glVertexAttribPointer(
            TexCoord, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(GrassInstanceGpu, pos_height)));
        glVertexAttribPointer(
            InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(GrassInstanceGpu, color_width)));
        glVertexAttribPointer(
            InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(GrassInstanceGpu, sway_params)));
        grass.instance_buffer->unbind();

        glDrawArraysInstanced(GL_TRIANGLES, 0,
                              m_terrain_pipeline->m_grass_vertex_count,
                              static_cast<GLsizei>(grass.instance_count));
        glBindVertexArray(0);

        if (prev_cull != 0U) {
          glEnable(GL_CULL_FACE);
        }

        break;
      }
      case TerrainScatterCmd::Species::Stone: {
        if (!m_vegetation_pipeline) {
          break;
        }
        struct StoneView {
          Buffer *instance_buffer;
          std::size_t instance_count;
          const StoneBatchParams &params;
        };
        const StoneView stone{deco_cmd_.instance_buffer,
                              deco_cmd_.instance_count, deco_cmd_.stone};
        if ((stone.instance_buffer == nullptr) || stone.instance_count == 0 ||
            (m_vegetation_pipeline->stone_shader() == nullptr) ||
            (m_vegetation_pipeline->m_stone_vao == 0U) ||
            m_vegetation_pipeline->m_stone_index_count == 0) {
          break;
        }

        DepthMaskScope const depth_mask(true);
        BlendScope const blend(false);

        Shader *stone_shader = m_vegetation_pipeline->stone_shader();
        if (m_last_bound_shader != stone_shader) {
          stone_shader->use();
          m_last_bound_shader = stone_shader;
          m_last_bound_texture = nullptr;
        }

        if (m_vegetation_pipeline->m_stone_uniforms.view_proj !=
            Shader::InvalidUniform) {
          stone_shader->set_uniform(
              m_vegetation_pipeline->m_stone_uniforms.view_proj, view_proj);
        }
        if (m_vegetation_pipeline->m_stone_uniforms.light_direction !=
            Shader::InvalidUniform) {
          QVector3D light_dir = stone.params.light_direction;
          if (!light_dir.isNull()) {
            light_dir.normalize();
          }
          stone_shader->set_uniform(
              m_vegetation_pipeline->m_stone_uniforms.light_direction, light_dir);
        }

        glBindVertexArray(m_vegetation_pipeline->m_stone_vao);
        stone.instance_buffer->bind();
        const auto stride = static_cast<GLsizei>(sizeof(StoneInstanceGpu));
        glVertexAttribPointer(
            TexCoord, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(StoneInstanceGpu, pos_scale)));
        glVertexAttribPointer(
            InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(StoneInstanceGpu, color_rot)));
        stone.instance_buffer->unbind();

        glDrawElementsInstanced(GL_TRIANGLES,
                                m_vegetation_pipeline->m_stone_index_count,
                                GL_UNSIGNED_SHORT, nullptr,
                                static_cast<GLsizei>(stone.instance_count));
        glBindVertexArray(0);

        break;
      }
      case TerrainScatterCmd::Species::Plant: {
        if (!m_vegetation_pipeline) {
          break;
        }
        struct PlantView {
          Buffer *instance_buffer;
          std::size_t instance_count;
          const PlantBatchParams &params;
        };
        const PlantView plant{deco_cmd_.instance_buffer,
                              deco_cmd_.instance_count, deco_cmd_.plant};

        if ((plant.instance_buffer == nullptr) || plant.instance_count == 0 ||
            (m_vegetation_pipeline->plant_shader() == nullptr) ||
            (m_vegetation_pipeline->m_plant_vao == 0U) ||
            m_vegetation_pipeline->m_plant_index_count == 0) {
          break;
        }

        DepthMaskScope const depth_mask(true);

        glEnable(GL_DEPTH_TEST);
        BlendScope const blend(true);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
        if (prev_cull != 0U) {
          glDisable(GL_CULL_FACE);
        }

        Shader *plant_shader = m_vegetation_pipeline->plant_shader();
        if (m_last_bound_shader != plant_shader) {
          plant_shader->use();
          m_last_bound_shader = plant_shader;
          m_last_bound_texture = nullptr;
        }

        if (m_vegetation_pipeline->m_plant_uniforms.view_proj !=
            Shader::InvalidUniform) {
          plant_shader->set_uniform(
              m_vegetation_pipeline->m_plant_uniforms.view_proj, view_proj);
        }
        if (m_vegetation_pipeline->m_plant_uniforms.time !=
            Shader::InvalidUniform) {
          plant_shader->set_uniform(m_vegetation_pipeline->m_plant_uniforms.time,
                                    plant.params.time);
        }
        if (m_vegetation_pipeline->m_plant_uniforms.wind_strength !=
            Shader::InvalidUniform) {
          plant_shader->set_uniform(
              m_vegetation_pipeline->m_plant_uniforms.wind_strength,
              plant.params.wind_strength);
        }
        if (m_vegetation_pipeline->m_plant_uniforms.wind_speed !=
            Shader::InvalidUniform) {
          plant_shader->set_uniform(
              m_vegetation_pipeline->m_plant_uniforms.wind_speed,
              plant.params.wind_speed);
        }
        if (m_vegetation_pipeline->m_plant_uniforms.light_direction !=
            Shader::InvalidUniform) {
          QVector3D light_dir = plant.params.light_direction;
          if (!light_dir.isNull()) {
            light_dir.normalize();
          }
          plant_shader->set_uniform(
              m_vegetation_pipeline->m_plant_uniforms.light_direction, light_dir);
        }

        glBindVertexArray(m_vegetation_pipeline->m_plant_vao);
        plant.instance_buffer->bind();
        const auto stride = static_cast<GLsizei>(sizeof(PlantInstanceGpu));
        glVertexAttribPointer(
            InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(PlantInstanceGpu, pos_scale)));
        glVertexAttribPointer(
            InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(PlantInstanceGpu, color_sway)));
        glVertexAttribPointer(
            InstanceColor, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(PlantInstanceGpu, type_params)));
        plant.instance_buffer->unbind();

        glDrawElementsInstanced(GL_TRIANGLES,
                                m_vegetation_pipeline->m_plant_index_count,
                                GL_UNSIGNED_SHORT, nullptr,
                                static_cast<GLsizei>(plant.instance_count));
        glBindVertexArray(0);

        if (prev_cull != 0U) {
          glEnable(GL_CULL_FACE);
        }

        break;
      }
      case TerrainScatterCmd::Species::Pine: {
        if (!m_vegetation_pipeline) {
          break;
        }
        struct PineView {
          Buffer *instance_buffer;
          std::size_t instance_count;
          const PineBatchParams &params;
        };
        const PineView pine{deco_cmd_.instance_buffer, deco_cmd_.instance_count,
                            deco_cmd_.pine};

        if ((pine.instance_buffer == nullptr) || pine.instance_count == 0 ||
            (m_vegetation_pipeline->pine_shader() == nullptr) ||
            (m_vegetation_pipeline->m_pine_vao == 0U) ||
            m_vegetation_pipeline->m_pine_index_count == 0) {
          break;
        }

        DepthMaskScope const depth_mask(true);
        glEnable(GL_DEPTH_TEST);
        BlendScope const blend(true);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
        if (prev_cull != 0U) {
          glDisable(GL_CULL_FACE);
        }

        Shader *pine_shader = m_vegetation_pipeline->pine_shader();
        if (m_last_bound_shader != pine_shader) {
          pine_shader->use();
          m_last_bound_shader = pine_shader;
          m_last_bound_texture = nullptr;
        }

        if (m_vegetation_pipeline->m_pine_uniforms.view_proj !=
            Shader::InvalidUniform) {
          pine_shader->set_uniform(
              m_vegetation_pipeline->m_pine_uniforms.view_proj, view_proj);
        }
        if (m_vegetation_pipeline->m_pine_uniforms.time !=
            Shader::InvalidUniform) {
          pine_shader->set_uniform(m_vegetation_pipeline->m_pine_uniforms.time,
                                   pine.params.time);
        }
        if (m_vegetation_pipeline->m_pine_uniforms.wind_strength !=
            Shader::InvalidUniform) {
          pine_shader->set_uniform(
              m_vegetation_pipeline->m_pine_uniforms.wind_strength,
              pine.params.wind_strength);
        }
        if (m_vegetation_pipeline->m_pine_uniforms.wind_speed !=
            Shader::InvalidUniform) {
          pine_shader->set_uniform(
              m_vegetation_pipeline->m_pine_uniforms.wind_speed,
              pine.params.wind_speed);
        }
        if (m_vegetation_pipeline->m_pine_uniforms.light_direction !=
            Shader::InvalidUniform) {
          QVector3D light_dir = pine.params.light_direction;
          if (!light_dir.isNull()) {
            light_dir.normalize();
          }
          pine_shader->set_uniform(
              m_vegetation_pipeline->m_pine_uniforms.light_direction, light_dir);
        }

        glBindVertexArray(m_vegetation_pipeline->m_pine_vao);
        pine.instance_buffer->bind();
        const auto stride = static_cast<GLsizei>(sizeof(PineInstanceGpu));
        glVertexAttribPointer(
            InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(PineInstanceGpu, pos_scale)));
        glVertexAttribPointer(
            InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(PineInstanceGpu, color_sway)));
        glVertexAttribPointer(
            InstanceColor, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(PineInstanceGpu, rotation)));
        pine.instance_buffer->unbind();

        glDrawElementsInstanced(GL_TRIANGLES,
                                m_vegetation_pipeline->m_pine_index_count,
                                GL_UNSIGNED_SHORT, nullptr,
                                static_cast<GLsizei>(pine.instance_count));
        glBindVertexArray(0);

        if (prev_cull != 0U) {
          glEnable(GL_CULL_FACE);
        }

        break;
      }
      case TerrainScatterCmd::Species::Olive: {
        if (!m_vegetation_pipeline) {
          break;
        }
        struct OliveView {
          Buffer *instance_buffer;
          std::size_t instance_count;
          const OliveBatchParams &params;
        };
        const OliveView olive{deco_cmd_.instance_buffer,
                              deco_cmd_.instance_count, deco_cmd_.olive};

        if ((olive.instance_buffer == nullptr) || olive.instance_count == 0 ||
            (m_vegetation_pipeline->olive_shader() == nullptr) ||
            (m_vegetation_pipeline->m_olive_vao == 0U) ||
            m_vegetation_pipeline->m_olive_index_count == 0) {
          break;
        }

        DepthMaskScope const depth_mask(true);
        glEnable(GL_DEPTH_TEST);
        BlendScope const blend(true);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
        if (prev_cull != 0U) {
          glDisable(GL_CULL_FACE);
        }

        Shader *olive_shader = m_vegetation_pipeline->olive_shader();
        if (m_last_bound_shader != olive_shader) {
          olive_shader->use();
          m_last_bound_shader = olive_shader;
          m_last_bound_texture = nullptr;
        }

        if (m_vegetation_pipeline->m_olive_uniforms.view_proj !=
            Shader::InvalidUniform) {
          olive_shader->set_uniform(
              m_vegetation_pipeline->m_olive_uniforms.view_proj, view_proj);
        }
        if (m_vegetation_pipeline->m_olive_uniforms.time !=
            Shader::InvalidUniform) {
          olive_shader->set_uniform(m_vegetation_pipeline->m_olive_uniforms.time,
                                    olive.params.time);
        }
        if (m_vegetation_pipeline->m_olive_uniforms.wind_strength !=
            Shader::InvalidUniform) {
          olive_shader->set_uniform(
              m_vegetation_pipeline->m_olive_uniforms.wind_strength,
              olive.params.wind_strength);
        }
        if (m_vegetation_pipeline->m_olive_uniforms.wind_speed !=
            Shader::InvalidUniform) {
          olive_shader->set_uniform(
              m_vegetation_pipeline->m_olive_uniforms.wind_speed,
              olive.params.wind_speed);
        }
        if (m_vegetation_pipeline->m_olive_uniforms.light_direction !=
            Shader::InvalidUniform) {
          QVector3D light_dir = olive.params.light_direction;
          if (!light_dir.isNull()) {
            light_dir.normalize();
          }
          olive_shader->set_uniform(
              m_vegetation_pipeline->m_olive_uniforms.light_direction, light_dir);
        }

        glBindVertexArray(m_vegetation_pipeline->m_olive_vao);
        olive.instance_buffer->bind();
        const auto stride = static_cast<GLsizei>(sizeof(OliveInstanceGpu));
        glVertexAttribPointer(
            InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(OliveInstanceGpu, pos_scale)));
        glVertexAttribPointer(
            InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(OliveInstanceGpu, color_sway)));
        glVertexAttribPointer(
            InstanceColor, Vec4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void *>(offsetof(OliveInstanceGpu, rotation)));
        olive.instance_buffer->unbind();

        glDrawElementsInstanced(GL_TRIANGLES,
                                m_vegetation_pipeline->m_olive_index_count,
                                GL_UNSIGNED_SHORT, nullptr,
                                static_cast<GLsizei>(olive.instance_count));
        glBindVertexArray(0);

        if (prev_cull != 0U) {
          glEnable(GL_CULL_FACE);
        }

        break;
      }
      case TerrainScatterCmd::Species::FireCamp: {
        if (!m_vegetation_pipeline) {
          break;
        }
        struct FireCampView {
          Buffer *instance_buffer;
          std::size_t instance_count;
          const FireCampBatchParams &params;
        };
        const FireCampView firecamp{deco_cmd_.instance_buffer,
                                    deco_cmd_.instance_count,
                                    deco_cmd_.firecamp};

        if ((firecamp.instance_buffer == nullptr) ||
            firecamp.instance_count == 0 ||
            (m_vegetation_pipeline->firecamp_shader() == nullptr) ||
            (m_vegetation_pipeline->m_firecamp_vao == 0U) ||
            m_vegetation_pipeline->m_firecamp_index_count == 0) {
          break;
        }

        DepthMaskScope const depth_mask(true);
        glEnable(GL_DEPTH_TEST);
        BlendScope const blend(true);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
        if (prev_cull != 0U) {
          glDisable(GL_CULL_FACE);
        }

        Shader *firecamp_shader = m_vegetation_pipeline->firecamp_shader();
        if (m_last_bound_shader != firecamp_shader) {
          firecamp_shader->use();
          m_last_bound_shader = firecamp_shader;
          m_last_bound_texture = nullptr;
        }

        if (m_vegetation_pipeline->m_firecamp_uniforms.view_proj !=
            Shader::InvalidUniform) {
          firecamp_shader->set_uniform(
              m_vegetation_pipeline->m_firecamp_uniforms.view_proj, view_proj);
        }
        if (m_vegetation_pipeline->m_firecamp_uniforms.time !=
            Shader::InvalidUniform) {
          firecamp_shader->set_uniform(
              m_vegetation_pipeline->m_firecamp_uniforms.time,
              firecamp.params.time);
        }
        if (m_vegetation_pipeline->m_firecamp_uniforms.flicker_speed !=
            Shader::InvalidUniform) {
          firecamp_shader->set_uniform(
              m_vegetation_pipeline->m_firecamp_uniforms.flicker_speed,
              firecamp.params.flicker_speed);
        }
        if (m_vegetation_pipeline->m_firecamp_uniforms.flicker_amount !=
            Shader::InvalidUniform) {
          firecamp_shader->set_uniform(
              m_vegetation_pipeline->m_firecamp_uniforms.flicker_amount,
              firecamp.params.flicker_amount);
        }
        if (m_vegetation_pipeline->m_firecamp_uniforms.glow_strength !=
            Shader::InvalidUniform) {
          firecamp_shader->set_uniform(
              m_vegetation_pipeline->m_firecamp_uniforms.glow_strength,
              firecamp.params.glow_strength);
        }
        if (m_vegetation_pipeline->m_firecamp_uniforms.camera_right !=
            Shader::InvalidUniform) {
          QVector3D camera_right = cam.get_right_vector();
          if (camera_right.lengthSquared() < 1e-6F) {
            camera_right = QVector3D(1.0F, 0.0F, 0.0F);
          } else {
            camera_right.normalize();
          }
          firecamp_shader->set_uniform(
              m_vegetation_pipeline->m_firecamp_uniforms.camera_right,
              camera_right);
        }
        if (m_vegetation_pipeline->m_firecamp_uniforms.camera_forward !=
            Shader::InvalidUniform) {
          QVector3D camera_forward = cam.get_forward_vector();
          if (camera_forward.lengthSquared() < 1e-6F) {
            camera_forward = QVector3D(0.0F, 0.0F, -1.0F);
          } else {
            camera_forward.normalize();
          }
          firecamp_shader->set_uniform(
              m_vegetation_pipeline->m_firecamp_uniforms.camera_forward,
              camera_forward);
        }

        if (m_vegetation_pipeline->m_firecamp_uniforms.fire_texture !=
            Shader::InvalidUniform) {
          if (m_resources && (m_resources->white() != nullptr)) {
            m_resources->white()->bind(0);
            firecamp_shader->set_uniform(
                m_vegetation_pipeline->m_firecamp_uniforms.fire_texture, 0);
          }
        }

        glBindVertexArray(m_vegetation_pipeline->m_firecamp_vao);
        firecamp.instance_buffer->bind();
        const auto stride = static_cast<GLsizei>(sizeof(FireCampInstanceGpu));
        glVertexAttribPointer(InstancePosition, Vec4, GL_FLOAT, GL_FALSE,
                              stride,
                              reinterpret_cast<void *>(offsetof(
                                  FireCampInstanceGpu, pos_intensity)));
        glVertexAttribPointer(InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<void *>(
                                  offsetof(FireCampInstanceGpu, radius_phase)));
        firecamp.instance_buffer->unbind();

        glDrawElementsInstanced(GL_TRIANGLES,
                                m_vegetation_pipeline->m_firecamp_index_count,
                                GL_UNSIGNED_SHORT, nullptr,
                                static_cast<GLsizei>(firecamp.instance_count));
        glBindVertexArray(0);

        if (prev_cull != 0U) {
          glEnable(GL_CULL_FACE);
        }

        break;
      }
      }
      break;
    }
    case RainBatchCmdIndex: {
      const auto &rain = std::get<RainBatchCmdIndex>(cmd);
      if (m_rain_pipeline == nullptr || !m_rain_pipeline->is_initialized()) {
        break;
      }
      m_rain_pipeline->render(cam, rain.params);
      break;
    }
    case TerrainSurfaceCmdIndex: {
      const auto &terrain = std::get<TerrainSurfaceCmdIndex>(cmd);

      Shader *active_shader = terrain.params.is_ground_plane
                                  ? m_terrain_pipeline->m_ground_shader
                                  : m_terrain_pipeline->m_terrain_shader;

      if ((terrain.mesh == nullptr) || (active_shader == nullptr)) {
        break;
      }

      if (m_last_bound_shader != active_shader) {
        active_shader->use();
        m_last_bound_shader = active_shader;
        m_last_bound_texture = nullptr;
      }

      QVector3D const fog_color(m_clear_color[Red], m_clear_color[Green],
                                m_clear_color[Blue]);
      QVector3D const camera_position = cam.get_position();
      float const fog_start =
          std::max(cam.get_near() + 8.0F, cam.get_far() * 0.30F);
      float const fog_end = std::max(fog_start + 1.0F, cam.get_far() * 0.78F);

      auto draw_surface = [&](const TerrainSurfaceCmd &single) {
        const QMatrix4x4 mvp = view_proj * single.model;
        auto const set_fog_uniforms = [&](const auto &uniforms) {
          if (uniforms.camera_position != Shader::InvalidUniform) {
            active_shader->set_uniform(uniforms.camera_position,
                                       camera_position);
          }
          if (uniforms.fog_color != Shader::InvalidUniform) {
            active_shader->set_uniform(uniforms.fog_color, fog_color);
          }
          if (uniforms.fog_start != Shader::InvalidUniform) {
            active_shader->set_uniform(uniforms.fog_start, fog_start);
          }
          if (uniforms.fog_end != Shader::InvalidUniform) {
            active_shader->set_uniform(uniforms.fog_end, fog_end);
          }
        };

        if (single.params.is_ground_plane) {
          if (m_terrain_pipeline->m_ground_uniforms.mvp !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.mvp,
                                       mvp);
          }
          if (m_terrain_pipeline->m_ground_uniforms.model !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.model, single.model);
          }
          if (m_terrain_pipeline->m_ground_uniforms.grass_primary !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.grass_primary,
                single.params.grass_primary);
          }
          if (m_terrain_pipeline->m_ground_uniforms.grass_secondary !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.grass_secondary,
                single.params.grass_secondary);
          }
          if (m_terrain_pipeline->m_ground_uniforms.grass_dry !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.grass_dry,
                single.params.grass_dry);
          }
          if (m_terrain_pipeline->m_ground_uniforms.soil_color !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.soil_color,
                single.params.soil_color);
          }
          if (m_terrain_pipeline->m_ground_uniforms.rock_low !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.rock_low,
                single.params.rock_low);
          }
          if (m_terrain_pipeline->m_ground_uniforms.rock_high !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.rock_high,
                single.params.rock_high);
          }
          if (m_terrain_pipeline->m_ground_uniforms.tint !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(m_terrain_pipeline->m_ground_uniforms.tint,
                                       single.params.tint);
          }
          if (m_terrain_pipeline->m_ground_uniforms.noise_offset !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.noise_offset,
                single.params.noise_offset);
          }
          if (m_terrain_pipeline->m_ground_uniforms.noise_angle !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.noise_angle,
                single.params.noise_angle);
          }
          if (m_terrain_pipeline->m_ground_uniforms.tile_size !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.tile_size,
                single.params.tile_size);
          }
          if (m_terrain_pipeline->m_ground_uniforms.macro_noise_scale !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.macro_noise_scale,
                single.params.macro_noise_scale);
          }
          if (m_terrain_pipeline->m_ground_uniforms.detail_noise_scale !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.detail_noise_scale,
                single.params.detail_noise_scale);
          }
          if (m_terrain_pipeline->m_ground_uniforms.soil_blend_height !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.soil_blend_height,
                single.params.soil_blend_height);
          }
          if (m_terrain_pipeline->m_ground_uniforms.soil_blend_sharpness !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.soil_blend_sharpness,
                single.params.soil_blend_sharpness);
          }
          if (m_terrain_pipeline->m_ground_uniforms.height_noise_strength !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.height_noise_strength,
                single.params.height_noise_strength);
          }
          if (m_terrain_pipeline->m_ground_uniforms.height_noise_frequency !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.height_noise_frequency,
                single.params.height_noise_frequency);
          }
          if (m_terrain_pipeline->m_ground_uniforms.ambient_boost !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.ambient_boost,
                single.params.ambient_boost);
          }
          if (m_terrain_pipeline->m_ground_uniforms.light_dir !=
              Shader::InvalidUniform) {
            QVector3D light_dir = single.params.light_direction;
            if (!light_dir.isNull()) {
              light_dir.normalize();
            }
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.light_dir, light_dir);
          }
          if (m_terrain_pipeline->m_ground_uniforms.snow_coverage !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.snow_coverage,
                single.params.snow_coverage);
          }
          if (m_terrain_pipeline->m_ground_uniforms.moisture_level !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.moisture_level,
                single.params.moisture_level);
          }
          if (m_terrain_pipeline->m_ground_uniforms.crack_intensity !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.crack_intensity,
                single.params.crack_intensity);
          }
          if (m_terrain_pipeline->m_ground_uniforms.rock_exposure !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.rock_exposure,
                single.params.rock_exposure);
          }
          if (m_terrain_pipeline->m_ground_uniforms.grass_saturation !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.grass_saturation,
                single.params.grass_saturation);
          }
          if (m_terrain_pipeline->m_ground_uniforms.soil_roughness !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.soil_roughness,
                single.params.soil_roughness);
          }
          if (m_terrain_pipeline->m_ground_uniforms.micro_bump_amp !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.micro_bump_amp,
                single.params.micro_bump_amp);
          }
          if (m_terrain_pipeline->m_ground_uniforms.micro_bump_freq !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.micro_bump_freq,
                single.params.micro_bump_freq);
          }
          if (m_terrain_pipeline->m_ground_uniforms.micro_normal_weight !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.micro_normal_weight,
                single.params.micro_normal_weight);
          }
          if (m_terrain_pipeline->m_ground_uniforms.albedo_jitter !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.albedo_jitter,
                single.params.albedo_jitter);
          }
          if (m_terrain_pipeline->m_ground_uniforms.snow_color !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_ground_uniforms.snow_color,
                single.params.snow_color);
          }
          set_fog_uniforms(m_terrain_pipeline->m_ground_uniforms);
        } else {
          if (m_terrain_pipeline->m_terrain_uniforms.mvp !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(m_terrain_pipeline->m_terrain_uniforms.mvp,
                                       mvp);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.model !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.model, single.model);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.grass_primary !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.grass_primary,
                single.params.grass_primary);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.grass_secondary !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.grass_secondary,
                single.params.grass_secondary);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.grass_dry !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.grass_dry,
                single.params.grass_dry);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.soil_color !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.soil_color,
                single.params.soil_color);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.rock_low !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.rock_low,
                single.params.rock_low);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.rock_high !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.rock_high,
                single.params.rock_high);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.tint !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.tint, single.params.tint);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.noise_offset !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.noise_offset,
                single.params.noise_offset);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.tile_size !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.tile_size,
                single.params.tile_size);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.macro_noise_scale !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.macro_noise_scale,
                single.params.macro_noise_scale);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.detail_noise_scale !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.detail_noise_scale,
                single.params.detail_noise_scale);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.slope_rock_threshold !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.slope_rock_threshold,
                single.params.slope_rock_threshold);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.slope_rock_sharpness !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.slope_rock_sharpness,
                single.params.slope_rock_sharpness);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.soil_blend_height !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.soil_blend_height,
                single.params.soil_blend_height);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.soil_blend_sharpness !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.soil_blend_sharpness,
                single.params.soil_blend_sharpness);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.height_noise_strength !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.height_noise_strength,
                single.params.height_noise_strength);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.height_noise_frequency !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.height_noise_frequency,
                single.params.height_noise_frequency);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.ambient_boost !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.ambient_boost,
                single.params.ambient_boost);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.rock_detail_strength !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.rock_detail_strength,
                single.params.rock_detail_strength);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.light_dir !=
              Shader::InvalidUniform) {
            QVector3D light_dir = single.params.light_direction;
            if (!light_dir.isNull()) {
              light_dir.normalize();
            }
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.light_dir, light_dir);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.snow_coverage !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.snow_coverage,
                single.params.snow_coverage);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.moisture_level !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.moisture_level,
                single.params.moisture_level);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.crack_intensity !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.crack_intensity,
                single.params.crack_intensity);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.rock_exposure !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.rock_exposure,
                single.params.rock_exposure);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.grass_saturation !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.grass_saturation,
                single.params.grass_saturation);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.soil_roughness !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.soil_roughness,
                single.params.soil_roughness);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.curvature_response !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.curvature_response,
                single.params.curvature_response);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.ridge_response !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.ridge_response,
                single.params.ridge_response);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.gully_response !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.gully_response,
                single.params.gully_response);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.snow_color !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.snow_color,
                single.params.snow_color);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.soil_foot_height !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.soil_foot_height,
                single.params.soil_foot_height);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.screen_toe_mul !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.screen_toe_mul,
                single.params.screen_toe_mul);
          }
          if (m_terrain_pipeline->m_terrain_uniforms.screen_toe_clamp !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_terrain_pipeline->m_terrain_uniforms.screen_toe_clamp,
                single.params.screen_toe_clamp);
          }
          set_fog_uniforms(m_terrain_pipeline->m_terrain_uniforms);
        }

        if (single.depth_bias != 0.0F) {
          PolygonOffsetScope const poly_scope(single.depth_bias,
                                              single.depth_bias);
          single.mesh->draw();
        } else {
          single.mesh->draw();
        }
      };

      DepthMaskScope const depth_mask(terrain.depth_write);
      std::optional<PolygonModeScope> polygon_mode_scope;
      if (terrain.wireframe) {
        polygon_mode_scope.emplace(GL_LINE);
      }
      for (std::size_t j = i; j < batch_end; ++j) {
        const auto &single =
            std::get<TerrainSurfaceCmdIndex>(queue.get_sorted(j));
        draw_surface(single);
      }
      break;
    }
    case TerrainFeatureCmdIndex: {
      const auto &feature = std::get<TerrainFeatureCmdIndex>(cmd);
      if (feature.mesh == nullptr || m_water_pipeline == nullptr) {
        break;
      }

      if (polygon_offset_enabled) {
        glDisable(GL_POLYGON_OFFSET_FILL);
        polygon_offset_enabled = false;
      }

      const bool is_transparent = feature.alpha < 0.999F;
      std::optional<DepthMaskScope> transparent_depth_scope;
      std::optional<BlendScope> transparent_blend_scope;
      if (is_transparent) {
        transparent_depth_scope.emplace(false);
        transparent_blend_scope.emplace(true);
        glDepthFunc(GL_LEQUAL);
      } else {
        glDepthMask(GL_TRUE);
      }

      auto draw_feature_model = [&](Shader *shader, GLint model_uniform,
                                    GLint mvp_uniform, GLint color_uniform,
                                    GLint alpha_uniform =
                                        Shader::InvalidUniform) {
        if (shader == nullptr) {
          return;
        }
        for (std::size_t j = i; j < batch_end; ++j) {
          const auto &single =
              std::get<TerrainFeatureCmdIndex>(queue.get_sorted(j));
          if (mvp_uniform != Shader::InvalidUniform) {
            shader->set_uniform(mvp_uniform, view_proj * single.model);
          }
          if (model_uniform != Shader::InvalidUniform) {
            shader->set_uniform(model_uniform, single.model);
          }
          if (color_uniform != Shader::InvalidUniform) {
            shader->set_uniform(color_uniform, single.color);
          }
          if (alpha_uniform != Shader::InvalidUniform) {
            shader->set_uniform(alpha_uniform, single.alpha);
          }
          single.mesh->draw();
        }
      };

      switch (feature.kind) {
      case LinearFeatureKind::River: {
        Shader *river_shader = m_water_pipeline->m_river_shader;
        if (river_shader == nullptr) {
          break;
        }
        if (m_last_bound_shader != river_shader) {
          river_shader->use();
          river_shader->set_uniform(m_water_pipeline->m_river_uniforms.view,
                                    view);
          river_shader->set_uniform(m_water_pipeline->m_river_uniforms.projection,
                                    projection);
          river_shader->set_uniform(m_water_pipeline->m_river_uniforms.time,
                                    m_animation_time);
          m_last_bound_shader = river_shader;
          m_last_bound_texture = nullptr;
        }
        draw_feature_model(river_shader, m_water_pipeline->m_river_uniforms.model,
                           Shader::InvalidUniform, Shader::InvalidUniform);
        break;
      }
      case LinearFeatureKind::Riverbank: {
        Shader *riverbank_shader = m_water_pipeline->m_riverbank_shader;
        if (riverbank_shader == nullptr) {
          break;
        }
        const auto &visibility = feature.visibility;
        if (m_last_bound_shader != riverbank_shader) {
          riverbank_shader->use();
          riverbank_shader->set_uniform(
              m_water_pipeline->m_riverbank_uniforms.view, view);
          riverbank_shader->set_uniform(
              m_water_pipeline->m_riverbank_uniforms.projection, projection);
          riverbank_shader->set_uniform(
              m_water_pipeline->m_riverbank_uniforms.time, m_animation_time);
          m_last_bound_shader = riverbank_shader;
        }
        if (m_water_pipeline->m_riverbank_uniforms.has_visibility !=
            Shader::InvalidUniform) {
          int const has_vis =
              visibility.enabled && (visibility.texture != nullptr) ? 1 : 0;
          riverbank_shader->set_uniform(
              m_water_pipeline->m_riverbank_uniforms.has_visibility, has_vis);
        }
        if (visibility.enabled && visibility.texture != nullptr) {
          if (m_water_pipeline->m_riverbank_uniforms.visibility_size !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.visibility_size,
                visibility.size);
          }
          if (m_water_pipeline->m_riverbank_uniforms.visibility_tile_size !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.visibility_tile_size,
                visibility.tile_size);
          }
          if (m_water_pipeline->m_riverbank_uniforms.explored_alpha !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.explored_alpha,
                visibility.explored_alpha);
          }
          constexpr int k_riverbank_vis_texture_unit = 7;
          visibility.texture->bind(k_riverbank_vis_texture_unit);
          if (m_water_pipeline->m_riverbank_uniforms.visibility_texture !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.visibility_texture,
                k_riverbank_vis_texture_unit);
          }
          m_last_bound_texture = visibility.texture;
        }
        for (std::size_t j = i; j < batch_end; ++j) {
          const auto &single =
              std::get<TerrainFeatureCmdIndex>(queue.get_sorted(j));
          riverbank_shader->set_uniform(
              m_water_pipeline->m_riverbank_uniforms.model, single.model);
          if (m_water_pipeline->m_riverbank_uniforms.segment_visibility !=
              Shader::InvalidUniform) {
            riverbank_shader->set_uniform(
                m_water_pipeline->m_riverbank_uniforms.segment_visibility,
                single.alpha);
          }
          single.mesh->draw();
        }
        break;
      }
      case LinearFeatureKind::Bridge: {
        Shader *bridge_shader = m_water_pipeline->m_bridge_shader;
        if (bridge_shader == nullptr) {
          break;
        }
        if (m_last_bound_shader != bridge_shader) {
          bridge_shader->use();
          QVector3D const light_dir(0.35F, 0.8F, 0.45F);
          bridge_shader->set_uniform(
              m_water_pipeline->m_bridge_uniforms.light_direction, light_dir);
          m_last_bound_shader = bridge_shader;
          m_last_bound_texture = nullptr;
        }
        draw_feature_model(bridge_shader,
                           m_water_pipeline->m_bridge_uniforms.model,
                           m_water_pipeline->m_bridge_uniforms.mvp,
                           m_water_pipeline->m_bridge_uniforms.color);
        break;
      }
      case LinearFeatureKind::Road: {
        Shader *road_shader = m_water_pipeline->m_road_shader;
        if (road_shader == nullptr) {
          break;
        }
        if (m_last_bound_shader != road_shader) {
          road_shader->use();
          QVector3D const light_dir(0.35F, 0.8F, 0.45F);
          road_shader->set_uniform(
              m_water_pipeline->m_road_uniforms.light_direction, light_dir);
          m_last_bound_shader = road_shader;
          m_last_bound_texture = nullptr;
        }
        draw_feature_model(road_shader, m_water_pipeline->m_road_uniforms.model,
                           m_water_pipeline->m_road_uniforms.mvp,
                           m_water_pipeline->m_road_uniforms.color,
                           m_water_pipeline->m_road_uniforms.alpha);
        break;
      }
      }
      break;
    }
    case MeshCmdIndex: {
      const auto &it = std::get<MeshCmdIndex>(cmd);
      if (it.mesh == nullptr) {
        break;
      }

      Shader *active_shader =
          (it.shader != nullptr) ? it.shader : m_basic_shader;
      if (active_shader == nullptr) {
        break;
      }

      if (polygon_offset_enabled) {
        glDisable(GL_POLYGON_OFFSET_FILL);
        polygon_offset_enabled = false;
      }

      bool const is_shadow_shader = (active_shader == m_shadow_shader);
      std::optional<DepthMaskScope> shadow_depth_scope;
      std::optional<BlendScope> shadow_blend_scope;
      if (is_shadow_shader) {
        shadow_depth_scope.emplace(false);
        shadow_blend_scope.emplace(true);
      } else {
        glDepthMask(GL_TRUE);
      }

      bool const is_transparent = (!is_shadow_shader) && (it.alpha < 0.999F);
      std::optional<DepthMaskScope> transparent_depth_scope;
      std::optional<BlendScope> transparent_blend_scope;
      if (is_transparent) {
        transparent_depth_scope.emplace(false);
        transparent_blend_scope.emplace(true);
        glDepthFunc(GL_LEQUAL);
      }

      if (m_banner_pipeline != nullptr &&
          active_shader == m_banner_pipeline->m_banner_shader) {
        if (m_last_bound_shader != active_shader) {
          active_shader->use();
          active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.time,
                                     m_animation_time);
          active_shader->set_uniform(
              m_banner_pipeline->m_banner_uniforms.wind_strength,
              banner_wind_strength);
          m_last_bound_shader = active_shader;
        }

        for (std::size_t j = i; j < batch_end; ++j) {
          const auto &single = std::get<MeshCmdIndex>(queue.get_sorted(j));
          QMatrix4x4 mvp = view_proj * single.model;
          active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.mvp,
                                     mvp);
          active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.model,
                                     single.model);
          active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.color,
                                     single.color);

          QVector3D const trim_color =
              single.has_trim_color ? single.trim_color : (single.color * 0.7F);
          active_shader->set_uniform(
              m_banner_pipeline->m_banner_uniforms.trim_color, trim_color);
          active_shader->set_uniform(m_banner_pipeline->m_banner_uniforms.alpha,
                                     single.alpha);
          active_shader->set_uniform(
              m_banner_pipeline->m_banner_uniforms.use_texture,
              single.texture != nullptr);

          Texture *tex_to_use =
              (single.texture != nullptr)
                  ? single.texture
                  : (m_resources ? m_resources->white() : nullptr);
          if ((tex_to_use != nullptr) && tex_to_use != m_last_bound_texture) {
            tex_to_use->bind(0);
            m_last_bound_texture = tex_to_use;
            active_shader->set_uniform(
                m_banner_pipeline->m_banner_uniforms.texture, 0);
          }
          single.mesh->draw();
        }
        break;
      }

      auto *uniforms =
          m_character_pipeline
              ? m_character_pipeline->resolve_uniforms(active_shader)
              : nullptr;
      if (uniforms == nullptr) {
        break;
      }

      if (m_last_bound_shader != active_shader) {
        active_shader->use();
        if (uniforms->view_proj != Shader::InvalidUniform) {
          active_shader->set_uniform(uniforms->view_proj, view_proj);
        }
        const Shader::UniformHandle time_uniform =
            active_shader->optional_uniform_handle("u_time");
        if (time_uniform != Shader::InvalidUniform) {
          active_shader->set_uniform(time_uniform, m_animation_time);
        }
        m_last_bound_shader = active_shader;
      }

      const bool can_execute_prepared_batch =
          prepared.kind == PreparedBatchKind::MeshInstanced &&
          m_mesh_instancing_pipeline &&
          m_mesh_instancing_pipeline->is_initialized() && !is_transparent &&
          !is_shadow_shader &&
          (uniforms->instanced != Shader::InvalidUniform ||
           uniforms->instanced_variant != nullptr);

      if (can_execute_prepared_batch) {
        const bool use_dedicated_variant =
            (uniforms->instanced_variant != nullptr);
        GL::Shader *batch_shader =
            use_dedicated_variant ? uniforms->instanced_variant : active_shader;

        if (use_dedicated_variant) {
          auto *inst_uniforms =
              m_character_pipeline
                  ? m_character_pipeline->resolve_uniforms(batch_shader)
                  : nullptr;
          if (inst_uniforms != nullptr) {
            batch_shader->use();
            if (inst_uniforms->view_proj != Shader::InvalidUniform) {
              batch_shader->set_uniform(inst_uniforms->view_proj, view_proj);
            }
            const Shader::UniformHandle time_uniform =
                batch_shader->optional_uniform_handle("u_time");
            if (time_uniform != Shader::InvalidUniform) {
              batch_shader->set_uniform(time_uniform, m_animation_time);
            }
            Texture *tex_to_use =
                (it.texture != nullptr)
                    ? it.texture
                    : (m_resources ? m_resources->white() : nullptr);
            if ((tex_to_use != nullptr) && tex_to_use != m_last_bound_texture) {
              tex_to_use->bind(0);
              m_last_bound_texture = tex_to_use;
              batch_shader->set_uniform(inst_uniforms->texture, 0);
            }
            batch_shader->set_uniform(inst_uniforms->use_texture,
                                      it.texture != nullptr);
            batch_shader->set_uniform(inst_uniforms->material_id,
                                      it.material_id);
            m_last_bound_shader = batch_shader;
          }
        } else {
          active_shader->set_uniform(uniforms->instanced, true);

          Texture *tex_to_use =
              (it.texture != nullptr)
                  ? it.texture
                  : (m_resources ? m_resources->white() : nullptr);
          if ((tex_to_use != nullptr) && tex_to_use != m_last_bound_texture) {
            tex_to_use->bind(0);
            m_last_bound_texture = tex_to_use;
            active_shader->set_uniform(uniforms->texture, 0);
          }
          active_shader->set_uniform(uniforms->use_texture,
                                     it.texture != nullptr);
          active_shader->set_uniform(uniforms->material_id, it.material_id);
        }

        m_mesh_instancing_pipeline->begin_batch(it.mesh, batch_shader,
                                              it.texture);
        for (std::size_t j = i; j < batch_end; ++j) {
          const auto &batch_it = std::get<MeshCmdIndex>(queue.get_sorted(j));
          m_mesh_instancing_pipeline->accumulate(batch_it.model, batch_it.color,
                                               batch_it.alpha,
                                               batch_it.material_id);
        }
        m_mesh_instancing_pipeline->flush();

        if (!use_dedicated_variant) {
          active_shader->set_uniform(uniforms->instanced, false);
        }
      } else {
        for (std::size_t j = i; j < batch_end; ++j) {
          const auto &single = std::get<MeshCmdIndex>(queue.get_sorted(j));

          active_shader->set_uniform(uniforms->model, single.model);
          if (uniforms->view_proj == Shader::InvalidUniform &&
              uniforms->mvp != Shader::InvalidUniform) {
            QMatrix4x4 const mvp = view_proj * single.model;
            active_shader->set_uniform(uniforms->mvp, mvp);
          }

          Texture *single_tex_to_use =
              (single.texture != nullptr)
                  ? single.texture
                  : (m_resources ? m_resources->white() : nullptr);
          if ((single_tex_to_use != nullptr) &&
              single_tex_to_use != m_last_bound_texture) {
            single_tex_to_use->bind(0);
            m_last_bound_texture = single_tex_to_use;
            active_shader->set_uniform(uniforms->texture, 0);
          }

          active_shader->set_uniform(uniforms->use_texture,
                                     single.texture != nullptr);
          active_shader->set_uniform(uniforms->color, single.color);
          active_shader->set_uniform(uniforms->alpha, single.alpha);
          active_shader->set_uniform(uniforms->material_id, single.material_id);
          if (uniforms->instanced != Shader::InvalidUniform) {
            active_shader->set_uniform(uniforms->instanced, false);
          }
          single.mesh->draw();
        }
      }
      if (is_transparent) {
        glDepthFunc(GL_LESS);
      }
      break;
    }
    case DrawPartCmdIndex: {

      const auto &part = std::get<DrawPartCmdIndex>(cmd);
      if (part.mesh == nullptr) {
        break;
      }

      const Render::ShaderQuality shader_quality =
          Render::GraphicsSettings::instance().features().shader_quality;
      Shader *active_shader = (part.material != nullptr)
                                  ? part.material->resolve(shader_quality)
                                  : nullptr;
      if (active_shader == nullptr) {
        active_shader = m_basic_shader;
      }
      if (active_shader == nullptr) {
        break;
      }

      if (polygon_offset_enabled) {
        glDisable(GL_POLYGON_OFFSET_FILL);
        polygon_offset_enabled = false;
      }

      const float part_alpha = part.alpha;
      const bool is_transparent = part_alpha < k_opaque_threshold;

      std::optional<DepthMaskScope> transparent_depth_scope;
      std::optional<BlendScope> transparent_blend_scope;
      glDepthMask(GL_TRUE);
      if (is_transparent) {
        transparent_depth_scope.emplace(false);
        transparent_blend_scope.emplace(true);
        glDepthFunc(GL_LEQUAL);
      }

      auto *uniforms =
          m_character_pipeline
              ? m_character_pipeline->resolve_uniforms(active_shader)
              : nullptr;
      if (uniforms == nullptr) {
        if (is_transparent) {
          glDepthFunc(GL_LESS);
        }
        break;
      }

      if (m_last_bound_shader != active_shader) {
        active_shader->use();
        if (uniforms->view_proj != Shader::InvalidUniform) {
          active_shader->set_uniform(uniforms->view_proj, view_proj);
        }
        m_last_bound_shader = active_shader;
      }

      Texture *tex_to_use =
          (part.texture != nullptr)
              ? part.texture
              : (m_resources ? m_resources->white() : nullptr);
      if ((tex_to_use != nullptr) && tex_to_use != m_last_bound_texture) {
        tex_to_use->bind(0);
        m_last_bound_texture = tex_to_use;
        active_shader->set_uniform(uniforms->texture, 0);
      }
      active_shader->set_uniform(uniforms->use_texture,
                                 part.texture != nullptr);
      active_shader->set_uniform(uniforms->material_id, part.material_id);

      const bool can_execute_prepared_batch =
          prepared.kind == PreparedBatchKind::DrawPartInstanced &&
          m_mesh_instancing_pipeline &&
          m_mesh_instancing_pipeline->is_initialized() && !is_transparent &&
          uniforms->instanced != Shader::InvalidUniform && part.palette.empty();

      if (can_execute_prepared_batch) {

        active_shader->set_uniform(uniforms->instanced, true);

        m_mesh_instancing_pipeline->begin_batch(part.mesh, active_shader,
                                              part.texture);
        for (std::size_t j = i; j < batch_end; ++j) {
          const auto &batch_part =
              std::get<DrawPartCmdIndex>(queue.get_sorted(j));
          m_mesh_instancing_pipeline->accumulate(
              batch_part.world, batch_part.color, batch_part.alpha,
              batch_part.material_id);
        }
        m_mesh_instancing_pipeline->flush();

        active_shader->set_uniform(uniforms->instanced, false);
      } else {
        for (std::size_t j = i; j < batch_end; ++j) {
          const auto &single_part =
              std::get<DrawPartCmdIndex>(queue.get_sorted(j));

          Texture *single_tex_to_use =
              (single_part.texture != nullptr)
                  ? single_part.texture
                  : (m_resources ? m_resources->white() : nullptr);
          if ((single_tex_to_use != nullptr) &&
              single_tex_to_use != m_last_bound_texture) {
            single_tex_to_use->bind(0);
            m_last_bound_texture = single_tex_to_use;
            active_shader->set_uniform(uniforms->texture, 0);
          }
          active_shader->set_uniform(uniforms->use_texture,
                                     single_part.texture != nullptr);
          active_shader->set_uniform(uniforms->material_id,
                                     single_part.material_id);
          active_shader->set_uniform(uniforms->model, single_part.world);
          if (uniforms->view_proj == Shader::InvalidUniform &&
              uniforms->mvp != Shader::InvalidUniform) {
            QMatrix4x4 const mvp = view_proj * single_part.world;
            active_shader->set_uniform(uniforms->mvp, mvp);
          }
          active_shader->set_uniform(uniforms->color, single_part.color);
          active_shader->set_uniform(uniforms->alpha, single_part.alpha);
          if (uniforms->instanced != Shader::InvalidUniform) {
            active_shader->set_uniform(uniforms->instanced, false);
          }
          single_part.mesh->draw();
        }
      }

      if (is_transparent) {
        glDepthFunc(GL_LESS);
      }
      break;
    }
    case GridCmdIndex: {
      if (m_effects_pipeline->m_grid_shader == nullptr) {
        break;
      }
      const auto &gc = std::get<GridCmdIndex>(cmd);

      if (m_last_bound_shader != m_effects_pipeline->m_grid_shader) {
        m_effects_pipeline->m_grid_shader->use();
        m_last_bound_shader = m_effects_pipeline->m_grid_shader;
      }

      m_effects_pipeline->m_grid_shader->set_uniform(
          m_effects_pipeline->m_grid_uniforms.mvp, gc.mvp);
      m_effects_pipeline->m_grid_shader->set_uniform(
          m_effects_pipeline->m_grid_uniforms.model, gc.model);
      m_effects_pipeline->m_grid_shader->set_uniform(
          m_effects_pipeline->m_grid_uniforms.grid_color, gc.color);
      m_effects_pipeline->m_grid_shader->set_uniform(
          m_effects_pipeline->m_grid_uniforms.line_color, k_grid_line_color);
      m_effects_pipeline->m_grid_shader->set_uniform(
          m_effects_pipeline->m_grid_uniforms.cell_size, gc.cell_size);
      m_effects_pipeline->m_grid_shader->set_uniform(
          m_effects_pipeline->m_grid_uniforms.thickness, gc.thickness);

      if (m_resources) {
        if (auto *plane = m_resources->ground()) {
          plane->draw();
        }
      }
      break;
    }
    case SelectionRingCmdIndex: {
      const auto &sc = std::get<SelectionRingCmdIndex>(cmd);
      Mesh *ring = Render::Geom::SelectionRing::get();
      if (ring == nullptr) {
        break;
      }

      if (m_last_bound_shader != m_effects_pipeline->m_basic_shader) {
        m_effects_pipeline->m_basic_shader->use();
        m_last_bound_shader = m_effects_pipeline->m_basic_shader;
      }
      m_effects_pipeline->m_basic_shader->set_uniform(
          m_effects_pipeline->m_basic_uniforms.use_texture, false);
      m_effects_pipeline->m_basic_shader->set_uniform(
          m_effects_pipeline->m_basic_uniforms.instanced, false);
      m_effects_pipeline->m_basic_shader->set_uniform(
          m_effects_pipeline->m_basic_uniforms.view_proj, view_proj);
      m_effects_pipeline->m_basic_shader->set_uniform(
          m_effects_pipeline->m_basic_uniforms.color, sc.color);

      DepthMaskScope const depth_mask(false);
      DepthTestScope const depth_test(true);
      PolygonOffsetScope const poly(-1.0F, -1.0F);
      BlendScope const blend(true);
      CullFaceScope const cull(false);

      {
        QMatrix4x4 m = sc.model;
        m.scale(1.08F, 1.0F, 1.08F);
        const QMatrix4x4 mvp = view_proj * m;
        m_effects_pipeline->m_basic_shader->set_uniform(
            m_effects_pipeline->m_basic_uniforms.mvp, mvp);
        m_effects_pipeline->m_basic_shader->set_uniform(
            m_effects_pipeline->m_basic_uniforms.model, m);
        m_effects_pipeline->m_basic_shader->set_uniform(
            m_effects_pipeline->m_basic_uniforms.alpha, sc.alpha_outer);
        ring->draw();
      }

      {
        const QMatrix4x4 mvp = view_proj * sc.model;
        m_effects_pipeline->m_basic_shader->set_uniform(
            m_effects_pipeline->m_basic_uniforms.mvp, mvp);
        m_effects_pipeline->m_basic_shader->set_uniform(
            m_effects_pipeline->m_basic_uniforms.model, sc.model);
        m_effects_pipeline->m_basic_shader->set_uniform(
            m_effects_pipeline->m_basic_uniforms.alpha, sc.alpha_inner);
        ring->draw();
      }
      break;
    }
    case SelectionSmokeCmdIndex: {
      const auto &sm = std::get<SelectionSmokeCmdIndex>(cmd);
      Mesh *disc = Render::Geom::SelectionDisc::get();
      if (disc == nullptr) {
        break;
      }

      if (m_last_bound_shader != m_effects_pipeline->m_basic_shader) {
        m_effects_pipeline->m_basic_shader->use();
        m_last_bound_shader = m_effects_pipeline->m_basic_shader;
      }
      m_effects_pipeline->m_basic_shader->set_uniform(
          m_effects_pipeline->m_basic_uniforms.use_texture, false);
      m_effects_pipeline->m_basic_shader->set_uniform(
          m_effects_pipeline->m_basic_uniforms.instanced, false);
      m_effects_pipeline->m_basic_shader->set_uniform(
          m_effects_pipeline->m_basic_uniforms.view_proj, view_proj);
      m_effects_pipeline->m_basic_shader->set_uniform(
          m_effects_pipeline->m_basic_uniforms.color, sm.color);
      DepthMaskScope const depth_mask(false);
      DepthTestScope const depth_test(true);

      PolygonOffsetScope const poly(-1.0F, -1.0F);
      BlendScope const blend(true);
      for (int i = 0; i < 7; ++i) {
        float const scale = 1.35F + 0.12F * i;
        float const a = sm.base_alpha * (1.0F - 0.09F * i);
        QMatrix4x4 m = sm.model;
        m.translate(0.0F, 0.02F, 0.0F);
        m.scale(scale, 1.0F, scale);
        const QMatrix4x4 mvp = view_proj * m;
        m_effects_pipeline->m_basic_shader->set_uniform(
            m_effects_pipeline->m_basic_uniforms.mvp, mvp);
        m_effects_pipeline->m_basic_shader->set_uniform(
            m_effects_pipeline->m_basic_uniforms.model, m);
        m_effects_pipeline->m_basic_shader->set_uniform(
            m_effects_pipeline->m_basic_uniforms.alpha, a);
        disc->draw();
      }
      break;
    }
    case PrimitiveBatchCmdIndex: {
      const auto &batch = std::get<PrimitiveBatchCmdIndex>(cmd);
      if (batch.instance_count() == 0 || m_primitive_batch_pipeline == nullptr ||
          !m_primitive_batch_pipeline->is_initialized()) {
        break;
      }

      const auto *data = batch.instance_data();

      switch (batch.type) {
      case PrimitiveType::Sphere:
        m_primitive_batch_pipeline->upload_sphere_instances(
            data, batch.instance_count());
        m_primitive_batch_pipeline->draw_spheres(batch.instance_count(),
                                               view_proj);
        break;
      case PrimitiveType::Cylinder:
        m_primitive_batch_pipeline->upload_cylinder_instances(
            data, batch.instance_count());
        m_primitive_batch_pipeline->draw_cylinders(batch.instance_count(),
                                                 view_proj);
        break;
      case PrimitiveType::Cone:
        m_primitive_batch_pipeline->upload_cone_instances(data,
                                                        batch.instance_count());
        m_primitive_batch_pipeline->draw_cones(batch.instance_count(), view_proj);
        break;
      }

      m_last_bound_shader = m_primitive_batch_pipeline->shader();
      break;
    }
    case EffectBatchCmdIndex: {
      const auto &eff_cmd_ = std::get<EffectBatchCmdIndex>(cmd);
      switch (eff_cmd_.kind) {
      case EffectBatchCmd::Kind::HealingBeam: {
        struct HealingBeamView {
          const QVector3D &start_pos;
          const QVector3D &end_pos;
          const QVector3D &color;
          float progress;
          float beam_width;
          float intensity;
          float time;
        };
        const HealingBeamView beam{eff_cmd_.position,   eff_cmd_.end_pos,
                                   eff_cmd_.color,      eff_cmd_.progress,
                                   eff_cmd_.beam_width, eff_cmd_.intensity,
                                   eff_cmd_.time};
        if (m_healing_beam_pipeline == nullptr ||
            !m_healing_beam_pipeline->is_initialized()) {
          break;
        }
        m_healing_beam_pipeline->render_single_beam(
            beam.start_pos, beam.end_pos, beam.color, beam.progress,
            beam.beam_width, beam.intensity, beam.time, view_proj);
        m_last_bound_shader = nullptr;
        break;
      }
      case EffectBatchCmd::Kind::HealerAura: {
        struct HealerAuraView {
          const QVector3D &position;
          const QVector3D &color;
          float radius;
          float intensity;
          float time;
        };
        const HealerAuraView aura{eff_cmd_.position, eff_cmd_.color,
                                  eff_cmd_.radius, eff_cmd_.intensity,
                                  eff_cmd_.time};
        if (m_healer_aura_pipeline == nullptr ||
            !m_healer_aura_pipeline->is_initialized()) {
          break;
        }
        m_healer_aura_pipeline->render_single_aura(aura.position, aura.color,
                                                 aura.radius, aura.intensity,
                                                 aura.time, view_proj);
        m_last_bound_shader = nullptr;
        break;
      }
      case EffectBatchCmd::Kind::CombatDust: {
        struct CombatDustView {
          const QVector3D &position;
          const QVector3D &color;
          float radius;
          float intensity;
          float time;
        };
        const CombatDustView dust{eff_cmd_.position, eff_cmd_.color,
                                  eff_cmd_.radius, eff_cmd_.intensity,
                                  eff_cmd_.time};
        if (m_combat_dust_pipeline == nullptr ||
            !m_combat_dust_pipeline->is_initialized()) {
          break;
        }
        m_combat_dust_pipeline->render_single_dust(dust.position, dust.color,
                                                 dust.radius, dust.intensity,
                                                 dust.time, view_proj);
        m_last_bound_shader = nullptr;
        break;
      }
      case EffectBatchCmd::Kind::BuildingFlame: {
        struct BuildingFlameView {
          const QVector3D &position;
          const QVector3D &color;
          float radius;
          float intensity;
          float time;
        };
        const BuildingFlameView flame{eff_cmd_.position, eff_cmd_.color,
                                      eff_cmd_.radius, eff_cmd_.intensity,
                                      eff_cmd_.time};
        if (m_combat_dust_pipeline == nullptr ||
            !m_combat_dust_pipeline->is_initialized()) {
          break;
        }
        m_combat_dust_pipeline->render_single_flame(flame.position, flame.color,
                                                  flame.radius, flame.intensity,
                                                  flame.time, view_proj);
        m_last_bound_shader = nullptr;
        break;
      }
      case EffectBatchCmd::Kind::StoneImpact: {
        struct StoneImpactView {
          const QVector3D &position;
          const QVector3D &color;
          float radius;
          float intensity;
          float time;
        };
        const StoneImpactView impact{eff_cmd_.position, eff_cmd_.color,
                                     eff_cmd_.radius, eff_cmd_.intensity,
                                     eff_cmd_.time};
        if (m_combat_dust_pipeline == nullptr ||
            !m_combat_dust_pipeline->is_initialized()) {
          break;
        }
        m_combat_dust_pipeline->render_single_stone_impact(
            impact.position, impact.color, impact.radius, impact.intensity,
            impact.time, view_proj);
        m_last_bound_shader = nullptr;
        break;
      }
      }
      break;
    }
    case ModeIndicatorCmdIndex: {
      const auto &mc = std::get<ModeIndicatorCmdIndex>(cmd);

      if (m_mode_indicator_pipeline == nullptr ||
          !m_mode_indicator_pipeline->is_initialized()) {
        break;
      }

      Mesh *indicator_mesh = nullptr;
      if (mc.mode_type == Render::Geom::k_mode_type_attack) {
        indicator_mesh = Render::Geom::ModeIndicator::get_attack_mode_mesh();
      } else if (mc.mode_type == Render::Geom::k_mode_type_guard) {
        indicator_mesh = Render::Geom::ModeIndicator::get_guard_mode_mesh();
      } else if (mc.mode_type == Render::Geom::k_mode_type_hold) {
        indicator_mesh = Render::Geom::ModeIndicator::get_hold_mode_mesh();
      } else if (mc.mode_type == Render::Geom::k_mode_type_patrol) {
        indicator_mesh = Render::Geom::ModeIndicator::get_patrol_mode_mesh();
      }

      if (indicator_mesh == nullptr) {
        break;
      }

      m_mode_indicator_pipeline->render_indicator(indicator_mesh, mc.model,
                                                view_proj, mc.color, mc.alpha,
                                                m_animation_time);

      m_last_bound_shader = nullptr;
      break;
    }
    case RiggedCreatureCmdIndex: {
      ++debug_rigged_batches;
      debug_rigged_cmds += prepared.count;
      if (polygon_offset_enabled) {
        glDisable(GL_POLYGON_OFFSET_FILL);
        polygon_offset_enabled = false;
      }
      glDepthMask(GL_TRUE);
      if (!m_rigged_character_pipeline ||
          !m_rigged_character_pipeline->is_initialized()) {
        break;
      }

      std::size_t rig_fallback_start = i;
      if (rigged_instancing_enabled &&
          prepared.kind == PreparedBatchKind::RiggedCreatureInstanced &&
          m_rigged_character_pipeline->instanced_shader() != nullptr) {
        thread_local std::vector<const RiggedCreatureCmd *> rig_batch_refs;
        const std::size_t cap = std::max<std::size_t>(
            1U, m_rigged_character_pipeline->max_instances_per_batch());
        if (rig_batch_refs.capacity() < cap) {
          rig_batch_refs.reserve(cap);
        }
        std::size_t j = i;
        while (j < batch_end) {
          const std::size_t chunk_end = std::min(batch_end, j + cap);
          const std::size_t chunk_count = chunk_end - j;
          if (chunk_count < 2U) {
            const auto &single =
                std::get<RiggedCreatureCmdIndex>(queue.get_sorted(j));
            m_rigged_character_pipeline->draw(single, view_proj);
            ++debug_rigged_single_draws;
            m_last_bound_shader = m_rigged_character_pipeline->shader();
            m_last_bound_texture = single.texture;
            ++j;
            rig_fallback_start = j;
            continue;
          }

          rig_batch_refs.clear();
          for (std::size_t k = j; k < chunk_end; ++k) {
            rig_batch_refs.push_back(
                &std::get<RiggedCreatureCmdIndex>(queue.get_sorted(k)));
          }
          ++debug_rigged_instanced_attempts;
          if (m_rigged_character_pipeline->draw_instanced(
                  rig_batch_refs.data(), rig_batch_refs.size(),
                  view_proj)) {
            ++debug_rigged_instanced_successes;
            m_last_bound_shader = m_rigged_character_pipeline->instanced_shader();
            m_last_bound_texture = nullptr;
            j = chunk_end;
            rig_fallback_start = j;
            continue;
          }

          ++debug_rigged_instanced_failures;
          break;
        }
      }

      if (rig_fallback_start < batch_end) {
        for (std::size_t j = rig_fallback_start; j < batch_end; ++j) {
          const auto &single =
              std::get<RiggedCreatureCmdIndex>(queue.get_sorted(j));
          m_rigged_character_pipeline->draw(single, view_proj);
          ++debug_rigged_single_draws;
          m_last_bound_shader = m_rigged_character_pipeline->shader();
          m_last_bound_texture = single.texture;
        }
      }
      break;
    }
    default:
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
            const PreparedBatch &remaining = prepared_batches[b];
            for (std::size_t j = remaining.start; j < remaining.end(); ++j) {
              if (extract_cmd_priority(queue.get_sorted(j)) <
                  CommandPriority::Low) {
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
              const PreparedBatch &remaining = prepared_batches[b];
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
  if (debug_rigged && debug_rigged_cmds > 0U) {
    qInfo() << "Rigged playback: batches" << debug_rigged_batches << "cmds"
            << debug_rigged_cmds << "single_draws" << debug_rigged_single_draws
            << "instanced_enabled" << rigged_instancing_enabled
            << "instanced_attempts" << debug_rigged_instanced_attempts
            << "instanced_successes" << debug_rigged_instanced_successes
            << "instanced_failures" << debug_rigged_instanced_failures;
  }
  if (m_last_bound_shader != nullptr) {
    m_last_bound_shader->release();
    m_last_bound_shader = nullptr;
  }

  m_frame_tracker.mark_complete();
  m_frame_tracker.end_frame();
}

} // namespace Render::GL
