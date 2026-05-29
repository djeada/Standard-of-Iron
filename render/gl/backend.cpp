#include "backend.h"

#include <QDebug>
#include <QOpenGLContext>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qopenglcontext.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <GL/gl.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

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

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {

const QVector3D k_grid_line_color(0.22F, 0.25F, 0.22F);

auto render_stage_logging_enabled() -> bool {
  return qEnvironmentVariableIsSet("SOI_RENDER_STAGE_LOG");
}

void log_render_first_use_once(const char* stage, const QString& detail) {
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
Backend::Backend(ShaderQuality quality)
    : m_shader_quality(quality) {
}

Backend::~Backend() {
  if (shader_bind_audit_enabled()) {
    qInfo() << "Shader bind audit:";
    for (const QString& line : format_shader_bind_audit()) {
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

    if (m_frame_ubo != 0) {
      glDeleteBuffers(1, &m_frame_ubo);
      m_frame_ubo = 0;
    }
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
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_frame_ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  qInfo() << "Backend: Frame UBO created at binding 0";
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
  m_vegetation_pipeline =
      std::make_unique<BackendPipelines::VegetationPipeline>(m_shader_cache.get());
  m_vegetation_pipeline->initialize();
  qInfo() << "Backend: VegetationPipeline initialized";

  qInfo() << "Backend: Creating TerrainPipeline...";
  m_terrain_pipeline =
      std::make_unique<BackendPipelines::TerrainPipeline>(this, m_shader_cache.get());
  m_terrain_pipeline->initialize();
  qInfo() << "Backend: TerrainPipeline initialized";

  qInfo() << "Backend: Creating CharacterPipeline...";
  m_character_pipeline =
      std::make_unique<BackendPipelines::CharacterPipeline>(this, m_shader_cache.get());
  m_character_pipeline->initialize();
  qInfo() << "Backend: CharacterPipeline initialized";

  qInfo() << "Backend: Creating RiggedCharacterPipeline...";
  m_rigged_character_pipeline =
      std::make_unique<BackendPipelines::RiggedCharacterPipeline>(this,
                                                                  m_shader_cache.get());
  m_rigged_character_pipeline->initialize();
  qInfo() << "Backend: RiggedCharacterPipeline initialized";

  qInfo() << "Backend: Creating WaterPipeline...";
  m_water_pipeline =
      std::make_unique<BackendPipelines::WaterPipeline>(this, m_shader_cache.get());
  m_water_pipeline->initialize();
  qInfo() << "Backend: WaterPipeline initialized";

  qInfo() << "Backend: Creating EffectsPipeline...";
  m_effects_pipeline =
      std::make_unique<BackendPipelines::EffectsPipeline>(this, m_shader_cache.get());
  m_effects_pipeline->initialize();
  qInfo() << "Backend: EffectsPipeline initialized";

  qInfo() << "Backend: Creating PrimitiveBatchPipeline...";
  m_primitive_batch_pipeline =
      std::make_unique<BackendPipelines::PrimitiveBatchPipeline>(m_shader_cache.get());
  m_primitive_batch_pipeline->initialize();
  qInfo() << "Backend: PrimitiveBatchPipeline initialized";

  qInfo() << "Backend: Creating BannerPipeline...";
  m_banner_pipeline =
      std::make_unique<BackendPipelines::BannerPipeline>(this, m_shader_cache.get());
  m_banner_pipeline->initialize();
  qInfo() << "Backend: BannerPipeline initialized";

  qInfo() << "Backend: Creating HealingBeamPipeline...";
  m_healing_beam_pipeline = std::make_unique<BackendPipelines::HealingBeamPipeline>(
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
  m_rain_pipeline =
      std::make_unique<BackendPipelines::RainPipeline>(this, m_shader_cache.get());
  m_rain_pipeline->initialize();
  qInfo() << "Backend: RainPipeline initialized";

  qInfo() << "Backend: Creating ModeIndicatorPipeline...";
  m_mode_indicator_pipeline = std::make_unique<BackendPipelines::ModeIndicatorPipeline>(
      this, m_shader_cache.get());
  m_mode_indicator_pipeline->initialize();
  qInfo() << "Backend: ModeIndicatorPipeline initialized";

  qInfo() << "Backend: Creating MeshInstancingPipeline...";
  m_mesh_instancing_pipeline =
      std::make_unique<BackendPipelines::MeshInstancingPipeline>(this,
                                                                 m_shader_cache.get());
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
  glClearColor(m_clear_color[red],
               m_clear_color[green],
               m_clear_color[blue],
               m_clear_color[alpha]);

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
  m_clear_color[red] = r;
  m_clear_color[green] = g;
  m_clear_color[blue] = b;
  m_clear_color[alpha] = a;
}

void Backend::execute(const DrawQueue& queue, const Camera& cam) {
  m_frame_tracker.begin_frame();

  if (m_basic_shader == nullptr) {
    m_frame_tracker.mark_complete();
    m_frame_tracker.end_frame();
    return;
  }

  const QMatrix4x4 view = cam.get_view_matrix();
  const QMatrix4x4 projection = cam.get_projection_matrix();
  const QMatrix4x4 view_proj = projection * view;
  if (m_frame_ubo != 0) {
    glBindBuffer(GL_UNIFORM_BUFFER, m_frame_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 64, view_proj.constData());
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
  }
  const float banner_wind_strength = 0.8F + 0.2F * std::sin(m_animation_time * 0.5F);

  m_last_bound_shader = nullptr;
  m_last_bound_texture = nullptr;

  bool polygon_offset_enabled = (glIsEnabled(GL_POLYGON_OFFSET_FILL) != 0U);

  const auto& prepared_batches = queue.prepared_batches();
  log_render_first_use_once(
      "backend-execute",
      QStringLiteral("first playback has %1 commands and %2 prepared batches")
          .arg(queue.size())
          .arg(prepared_batches.size()));
  const bool rigged_instancing_enabled =
      !qEnvironmentVariableIsSet("SOI_RENDER_DISABLE_RIGGED_INSTANCING");
  const bool debug_rigged = qEnvironmentVariableIsSet("SOI_RENDER_DEBUG_RIGGED");
  std::size_t debug_rigged_batches = 0;
  std::size_t debug_rigged_cmds = 0;
  std::size_t debug_rigged_instanced_attempts = 0;
  std::size_t debug_rigged_instanced_successes = 0;
  std::size_t debug_rigged_instanced_failures = 0;
  std::size_t debug_rigged_single_draws = 0;
  CommandExecutionContext context{queue,
                                  cam,
                                  view,
                                  projection,
                                  view_proj,
                                  banner_wind_strength,
                                  polygon_offset_enabled,
                                  rigged_instancing_enabled,
                                  debug_rigged_batches,
                                  debug_rigged_cmds,
                                  debug_rigged_instanced_attempts,
                                  debug_rigged_instanced_successes,
                                  debug_rigged_instanced_failures,
                                  debug_rigged_single_draws};
  std::size_t batch_index = 0;
  while (batch_index < prepared_batches.size()) {
    const PreparedBatch& prepared = prepared_batches[batch_index];
    const std::size_t i = prepared.start;
    const std::size_t batch_end = prepared.end();
    const auto& cmd = queue.get_sorted(i);
    switch (cmd.index()) {
    case CylinderCmdIndex:
    case FogBatchCmdIndex:
      execute_cylinder_commands(prepared, context);
      break;
    case TerrainScatterCmdIndex:
      execute_scatter_commands(prepared, context);
      break;
    case TerrainSurfaceCmdIndex:
      execute_terrain_commands(prepared, context);
      break;
    case TerrainFeatureCmdIndex:
      execute_water_linear_commands(prepared, context);
      break;
    case MeshCmdIndex:
    case DrawPartCmdIndex:
      execute_mesh_commands(prepared, context);
      break;
    case RainBatchCmdIndex:
    case GridCmdIndex:
    case SelectionRingCmdIndex:
    case SelectionSmokeCmdIndex:
    case PrimitiveBatchCmdIndex:
    case EffectBatchCmdIndex:
    case ModeIndicatorCmdIndex:
      execute_effects_commands(prepared, context);
      break;
    case RiggedCreatureCmdIndex:
      execute_rigged_commands(prepared, context);
      break;
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
  if (debug_rigged && debug_rigged_cmds > 0U) {
    qInfo() << "Rigged playback: batches" << debug_rigged_batches << "cmds"
            << debug_rigged_cmds << "single_draws" << debug_rigged_single_draws
            << "instanced_enabled" << rigged_instancing_enabled << "instanced_attempts"
            << debug_rigged_instanced_attempts << "instanced_successes"
            << debug_rigged_instanced_successes << "instanced_failures"
            << debug_rigged_instanced_failures;
  }
  if (m_last_bound_shader != nullptr) {
    m_last_bound_shader->release();
    m_last_bound_shader = nullptr;
  }

  m_frame_tracker.mark_complete();
  m_frame_tracker.end_frame();
}

} // namespace Render::GL
