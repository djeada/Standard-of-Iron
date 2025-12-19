#include "backend.h"
#include "../draw_queue.h"
#include "backend_executor.h"
#include <GL/gl.h>
#include <QOpenGLContext>

namespace Render::GL {

Backend::Backend() = default;

Backend::~Backend() {
  // Smart pointers automatically clean up if context is available
  // If no context, release without destruction
  if (QOpenGLContext::currentContext() == nullptr) {
    (void)m_cylinderPipeline.release();
    (void)m_vegetationPipeline.release();
    (void)m_terrainPipeline.release();
    (void)m_characterPipeline.release();
    (void)m_waterPipeline.release();
    (void)m_effectsPipeline.release();
    (void)m_primitiveBatchPipeline.release();
    (void)m_bannerPipeline.release();
    (void)m_healingBeamPipeline.release();
    (void)m_healerAuraPipeline.release();
    (void)m_combatDustPipeline.release();
    (void)m_rainPipeline.release();
    (void)m_modeIndicatorPipeline.release();
  }
}

auto Backend::banner_mesh() const -> Mesh * {
  return m_bannerPipeline ? m_bannerPipeline->getBannerMesh() : nullptr;
}

auto Backend::banner_shader() const -> Shader * {
  return m_bannerPipeline ? m_bannerPipeline->m_bannerShader : nullptr;
}

void Backend::begin_frame() {
  if (m_viewportWidth > 0 && m_viewportHeight > 0) {
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
  }
  glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2],
               m_clearColor[3]);
  glClearDepth(1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);

  if (m_cylinderPipeline) {
    m_cylinderPipeline->begin_frame();
  }
}

void Backend::setViewport(int w, int h) {
  m_viewportWidth = w;
  m_viewportHeight = h;
}

void Backend::setClearColor(float r, float g, float b, float a) {
  m_clearColor[0] = r;
  m_clearColor[1] = g;
  m_clearColor[2] = b;
  m_clearColor[3] = a;
}

void Backend::execute(const DrawQueue &queue, const Camera &cam) {
  if (m_basicShader == nullptr) {
    return;
  }

  const QMatrix4x4 view_proj =
      cam.get_projection_matrix() * cam.get_view_matrix();

  m_lastBoundShader = nullptr;
  m_lastBoundTexture = nullptr;

  const std::size_t count = queue.size();
  std::size_t i = 0;
  while (i < count) {
    const auto &cmd = queue.get_sorted(i);
    switch (cmd.index()) {
    case CylinderCmdIndex:
      BackendExecutor::execute_cylinder_batch(this, queue, i, view_proj);
      continue;
    case FogBatchCmdIndex:
      BackendExecutor::execute_fog_batch(this, queue, i, view_proj);
      continue;
    case GrassBatchCmdIndex:
      BackendExecutor::execute_grass_batch(this, queue, i, view_proj);
      break;
    case StoneBatchCmdIndex:
      BackendExecutor::execute_stone_batch(this, queue, i, view_proj);
      break;
    case PlantBatchCmdIndex:
      BackendExecutor::execute_plant_batch(this, queue, i, view_proj);
      break;
    case PineBatchCmdIndex:
      BackendExecutor::execute_pine_batch(this, queue, i, view_proj);
      break;
    case OliveBatchCmdIndex:
      BackendExecutor::execute_olive_batch(this, queue, i, view_proj);
      break;
    case FireCampBatchCmdIndex:
      BackendExecutor::execute_firecamp_batch(this, queue, i, cam, view_proj);
      break;
    case RainBatchCmdIndex:
      BackendExecutor::execute_rain_batch(this, queue, i, cam);
      break;
    case TerrainChunkCmdIndex:
      BackendExecutor::execute_terrain_chunk(this, queue, i, view_proj);
      break;
    case MeshCmdIndex:
      BackendExecutor::execute_mesh_cmd(this, queue, i, cam, view_proj);
      break;
    case GridCmdIndex:
      BackendExecutor::execute_grid_cmd(this, queue, i);
      break;
    case SelectionRingCmdIndex:
      BackendExecutor::execute_selection_ring(this, queue, i, view_proj);
      break;
    case SelectionSmokeCmdIndex:
      BackendExecutor::execute_selection_smoke(this, queue, i, view_proj);
      break;
    case PrimitiveBatchCmdIndex:
      BackendExecutor::execute_primitive_batch(this, queue, i, view_proj);
      break;
    case HealingBeamCmdIndex:
      BackendExecutor::execute_healing_beam(this, queue, i, view_proj);
      break;
    case HealerAuraCmdIndex:
      BackendExecutor::execute_healer_aura(this, queue, i, view_proj);
      break;
    case CombatDustCmdIndex:
      BackendExecutor::execute_combat_dust(this, queue, i, view_proj);
      break;
    case BuildingFlameCmdIndex:
      BackendExecutor::execute_building_flame(this, queue, i, view_proj);
      break;
    case StoneImpactCmdIndex:
      BackendExecutor::execute_stone_impact(this, queue, i, view_proj);
      break;
    case ModeIndicatorCmdIndex:
      BackendExecutor::execute_mode_indicator(this, queue, i, view_proj);
      break;
    default:
      break;
    }
    ++i;
  }
  if (m_lastBoundShader != nullptr) {
    m_lastBoundShader->release();
    m_lastBoundShader = nullptr;
  }
}

} // namespace Render::GL
