#include "backend.h"
#include "backend/banner_pipeline.h"
#include "backend/character_pipeline.h"
#include "backend/combat_dust_pipeline.h"
#include "backend/cylinder_pipeline.h"
#include "backend/effects_pipeline.h"
#include "backend/healer_aura_pipeline.h"
#include "backend/healing_beam_pipeline.h"
#include "backend/mode_indicator_pipeline.h"
#include "backend/primitive_batch_pipeline.h"
#include "backend/rain_pipeline.h"
#include "backend/terrain_pipeline.h"
#include "backend/vegetation_pipeline.h"
#include "backend/water_pipeline.h"
#include "resources.h"
#include "shader_cache.h"
#include <GL/gl.h>
#include <QDebug>

namespace Render::GL {

void Backend::initialize() {
  qInfo() << "Backend::initialize() - Starting...";

  initializeOpenGLFunctions();

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthRange(0.0, 1.0);
  glDepthMask(GL_TRUE);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_resources = std::make_unique<ResourceManager>();
  if (!m_resources->initialize()) {
    qWarning() << "Backend: failed to initialize ResourceManager";
  }

  m_shaderCache = std::make_unique<ShaderCache>();
  m_shaderCache->initializeDefaults();

  m_cylinderPipeline =
      std::make_unique<BackendPipelines::CylinderPipeline>(m_shaderCache.get());
  m_cylinderPipeline->initialize();

  m_vegetationPipeline = std::make_unique<BackendPipelines::VegetationPipeline>(
      m_shaderCache.get());
  m_vegetationPipeline->initialize();

  m_terrainPipeline = std::make_unique<BackendPipelines::TerrainPipeline>(
      this, m_shaderCache.get());
  m_terrainPipeline->initialize();

  m_characterPipeline = std::make_unique<BackendPipelines::CharacterPipeline>(
      this, m_shaderCache.get());
  m_characterPipeline->initialize();

  m_waterPipeline = std::make_unique<BackendPipelines::WaterPipeline>(
      this, m_shaderCache.get());
  m_waterPipeline->initialize();

  m_effectsPipeline = std::make_unique<BackendPipelines::EffectsPipeline>(
      this, m_shaderCache.get());
  m_effectsPipeline->initialize();

  m_primitiveBatchPipeline =
      std::make_unique<BackendPipelines::PrimitiveBatchPipeline>(
          m_shaderCache.get());
  m_primitiveBatchPipeline->initialize();

  m_bannerPipeline = std::make_unique<BackendPipelines::BannerPipeline>(
      this, m_shaderCache.get());
  m_bannerPipeline->initialize();

  m_healingBeamPipeline =
      std::make_unique<BackendPipelines::HealingBeamPipeline>(
          this, m_shaderCache.get());
  m_healingBeamPipeline->initialize();

  m_healerAuraPipeline = std::make_unique<BackendPipelines::HealerAuraPipeline>(
      this, m_shaderCache.get());
  m_healerAuraPipeline->initialize();

  m_combatDustPipeline = std::make_unique<BackendPipelines::CombatDustPipeline>(
      this, m_shaderCache.get());
  m_combatDustPipeline->initialize();

  m_rainPipeline = std::make_unique<BackendPipelines::RainPipeline>(
      this, m_shaderCache.get());
  m_rainPipeline->initialize();

  m_modeIndicatorPipeline =
      std::make_unique<BackendPipelines::ModeIndicatorPipeline>(
          this, m_shaderCache.get());
  m_modeIndicatorPipeline->initialize();

  m_basicShader = m_shaderCache->get(QStringLiteral("basic"));
  m_gridShader = m_shaderCache->get(QStringLiteral("grid"));
  if (m_basicShader == nullptr) {
    qWarning() << "Backend: basic shader missing";
  }
  if (m_gridShader == nullptr) {
    qWarning() << "Backend: grid shader missing";
  }
  qInfo() << "Backend::initialize() - Complete!";
}

} // namespace Render::GL
