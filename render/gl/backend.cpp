#include "backend.h"
#include "../draw_queue.h"
#include "../geom/selection_disc.h"
#include "../geom/selection_ring.h"
#include "backend/character_pipeline.h"
#include "backend/cylinder_pipeline.h"
#include "backend/effects_pipeline.h"
#include "backend/terrain_pipeline.h"
#include "backend/vegetation_pipeline.h"
#include "backend/water_pipeline.h"
#include "buffer.h"
#include "gl/camera.h"
#include "gl/resources.h"
#include "ground/firecamp_gpu.h"
#include "ground/grass_gpu.h"
#include "ground/pine_gpu.h"
#include "ground/plant_gpu.h"
#include "ground/stone_gpu.h"
#include "mesh.h"
#include "render_constants.h"
#include "shader.h"
#include "state_scopes.h"
#include "texture.h"
#include <GL/gl.h>
#include <QDebug>
#include <QOpenGLContext>
#include <cmath>
#include <cstddef>
#include <memory>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qopenglcontext.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <vector>

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {

const QVector3D k_grid_line_color(0.22F, 0.25F, 0.22F);
}

Backend::Backend() = default;

Backend::~Backend() {

  if (QOpenGLContext::currentContext() == nullptr) {

    (void)m_cylinderPipeline.release();
    (void)m_vegetationPipeline.release();
    (void)m_terrainPipeline.release();
    (void)m_characterPipeline.release();
    (void)m_waterPipeline.release();
    (void)m_effectsPipeline.release();
  } else {

    m_cylinderPipeline.reset();
    m_vegetationPipeline.reset();
    m_terrainPipeline.reset();
    m_characterPipeline.reset();
    m_waterPipeline.reset();
    m_effectsPipeline.reset();
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
  m_shaderCache = std::make_unique<ShaderCache>();
  m_shaderCache->initializeDefaults();
  qInfo() << "Backend: ShaderCache created";

  qInfo() << "Backend: Creating CylinderPipeline...";
  m_cylinderPipeline =
      std::make_unique<BackendPipelines::CylinderPipeline>(m_shaderCache.get());
  m_cylinderPipeline->initialize();
  qInfo() << "Backend: CylinderPipeline initialized";

  qInfo() << "Backend: Creating VegetationPipeline...";
  m_vegetationPipeline = std::make_unique<BackendPipelines::VegetationPipeline>(
      m_shaderCache.get());
  m_vegetationPipeline->initialize();
  qInfo() << "Backend: VegetationPipeline initialized";

  qInfo() << "Backend: Creating TerrainPipeline...";
  m_terrainPipeline = std::make_unique<BackendPipelines::TerrainPipeline>(
      this, m_shaderCache.get());
  m_terrainPipeline->initialize();
  qInfo() << "Backend: TerrainPipeline initialized";

  qInfo() << "Backend: Creating CharacterPipeline...";
  m_characterPipeline = std::make_unique<BackendPipelines::CharacterPipeline>(
      this, m_shaderCache.get());
  m_characterPipeline->initialize();
  qInfo() << "Backend: CharacterPipeline initialized";

  qInfo() << "Backend: Creating WaterPipeline...";
  m_waterPipeline = std::make_unique<BackendPipelines::WaterPipeline>(
      this, m_shaderCache.get());
  m_waterPipeline->initialize();
  qInfo() << "Backend: WaterPipeline initialized";

  qInfo() << "Backend: Creating EffectsPipeline...";
  m_effectsPipeline = std::make_unique<BackendPipelines::EffectsPipeline>(
      this, m_shaderCache.get());
  m_effectsPipeline->initialize();
  qInfo() << "Backend: EffectsPipeline initialized";

  qInfo() << "Backend: Loading basic shaders...";
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

void Backend::beginFrame() {
  if (m_viewportWidth > 0 && m_viewportHeight > 0) {
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
  }
  glClearColor(m_clearColor[Red], m_clearColor[Green], m_clearColor[Blue],
               m_clearColor[Alpha]);

  glClearDepth(1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);

  if (m_cylinderPipeline) {
    m_cylinderPipeline->beginFrame();
  }
}

void Backend::setViewport(int w, int h) {
  m_viewportWidth = w;
  m_viewportHeight = h;
}

void Backend::setClearColor(float r, float g, float b, float a) {
  m_clearColor[Red] = r;
  m_clearColor[Green] = g;
  m_clearColor[Blue] = b;
  m_clearColor[Alpha] = a;
}

void Backend::execute(const DrawQueue &queue, const Camera &cam) {
  if (m_basicShader == nullptr) {
    return;
  }

  const QMatrix4x4 view_proj = cam.getProjectionMatrix() * cam.getViewMatrix();

  m_lastBoundShader = nullptr;
  m_lastBoundTexture = nullptr;

  const std::size_t count = queue.size();
  std::size_t i = 0;
  while (i < count) {
    const auto &cmd = queue.getSorted(i);
    switch (cmd.index()) {
    case CylinderCmdIndex: {
      if (!m_cylinderPipeline) {
        ++i;
        continue;
      }
      m_cylinderPipeline->m_cylinderScratch.clear();
      do {
        const auto &cy = std::get<CylinderCmdIndex>(queue.getSorted(i));
        BackendPipelines::CylinderPipeline::CylinderInstanceGpu gpu{};
        gpu.start = cy.start;
        gpu.end = cy.end;
        gpu.radius = cy.radius;
        gpu.alpha = cy.alpha;
        gpu.color = cy.color;
        m_cylinderPipeline->m_cylinderScratch.emplace_back(gpu);
        ++i;
      } while (i < count && queue.getSorted(i).index() == CylinderCmdIndex);

      const std::size_t instance_count =
          m_cylinderPipeline->m_cylinderScratch.size();
      if (instance_count > 0 &&
          (m_cylinderPipeline->cylinderShader() != nullptr)) {
        glDepthMask(GL_TRUE);
        if (glIsEnabled(GL_POLYGON_OFFSET_FILL) != 0U) {
          glDisable(GL_POLYGON_OFFSET_FILL);
        }
        Shader *cylinder_shader = m_cylinderPipeline->cylinderShader();
        if (m_lastBoundShader != cylinder_shader) {
          cylinder_shader->use();
          m_lastBoundShader = cylinder_shader;
          m_lastBoundTexture = nullptr;
        }
        if (m_cylinderPipeline->m_cylinderUniforms.view_proj !=
            Shader::InvalidUniform) {
          cylinder_shader->setUniform(
              m_cylinderPipeline->m_cylinderUniforms.view_proj, view_proj);
        }
        m_cylinderPipeline->uploadCylinderInstances(instance_count);
        m_cylinderPipeline->draw_cylinders(instance_count);
      }
      continue;
    }
    case FogBatchCmdIndex: {
      if (!m_cylinderPipeline) {
        ++i;
        continue;
      }
      const auto &batch = std::get<FogBatchCmdIndex>(cmd);
      const FogInstanceData *instances = batch.instances;
      const std::size_t instance_count = batch.count;
      if ((instances != nullptr) && instance_count > 0 &&
          (m_cylinderPipeline->fogShader() != nullptr)) {
        m_cylinderPipeline->m_fogScratch.resize(instance_count);
        for (std::size_t idx = 0; idx < instance_count; ++idx) {
          BackendPipelines::CylinderPipeline::FogInstanceGpu gpu{};
          gpu.center = instances[idx].center;
          gpu.size = instances[idx].size;
          gpu.color = instances[idx].color;
          gpu.alpha = instances[idx].alpha;
          m_cylinderPipeline->m_fogScratch[idx] = gpu;
        }
        glDepthMask(GL_TRUE);
        if (glIsEnabled(GL_POLYGON_OFFSET_FILL) != 0U) {
          glDisable(GL_POLYGON_OFFSET_FILL);
        }
        Shader *fog_shader = m_cylinderPipeline->fogShader();
        if (m_lastBoundShader != fog_shader) {
          fog_shader->use();
          m_lastBoundShader = fog_shader;
          m_lastBoundTexture = nullptr;
        }
        if (m_cylinderPipeline->m_fogUniforms.view_proj !=
            Shader::InvalidUniform) {
          fog_shader->setUniform(m_cylinderPipeline->m_fogUniforms.view_proj,
                                 view_proj);
        }
        m_cylinderPipeline->uploadFogInstances(instance_count);
        m_cylinderPipeline->drawFog(instance_count);
      }
      ++i;
      continue;
    }
    case GrassBatchCmdIndex: {
      const auto &grass = std::get<GrassBatchCmdIndex>(cmd);
      if ((grass.instanceBuffer == nullptr) || grass.instance_count == 0 ||
          (m_terrainPipeline->m_grassShader == nullptr) ||
          (m_terrainPipeline->m_grassVao == 0U) ||
          m_terrainPipeline->m_grassVertexCount == 0) {
        break;
      }

      DepthMaskScope const depth_mask(false);
      BlendScope const blend(true);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
      if (prev_cull != 0U) {
        glDisable(GL_CULL_FACE);
      }

      if (m_lastBoundShader != m_terrainPipeline->m_grassShader) {
        m_terrainPipeline->m_grassShader->use();
        m_lastBoundShader = m_terrainPipeline->m_grassShader;
        m_lastBoundTexture = nullptr;
      }

      if (m_terrainPipeline->m_grassUniforms.view_proj !=
          Shader::InvalidUniform) {
        m_terrainPipeline->m_grassShader->setUniform(
            m_terrainPipeline->m_grassUniforms.view_proj, view_proj);
      }
      if (m_terrainPipeline->m_grassUniforms.time != Shader::InvalidUniform) {
        m_terrainPipeline->m_grassShader->setUniform(
            m_terrainPipeline->m_grassUniforms.time, grass.params.time);
      }
      if (m_terrainPipeline->m_grassUniforms.windStrength !=
          Shader::InvalidUniform) {
        m_terrainPipeline->m_grassShader->setUniform(
            m_terrainPipeline->m_grassUniforms.windStrength,
            grass.params.windStrength);
      }
      if (m_terrainPipeline->m_grassUniforms.windSpeed !=
          Shader::InvalidUniform) {
        m_terrainPipeline->m_grassShader->setUniform(
            m_terrainPipeline->m_grassUniforms.windSpeed,
            grass.params.windSpeed);
      }
      if (m_terrainPipeline->m_grassUniforms.soilColor !=
          Shader::InvalidUniform) {
        m_terrainPipeline->m_grassShader->setUniform(
            m_terrainPipeline->m_grassUniforms.soilColor,
            grass.params.soilColor);
      }
      if (m_terrainPipeline->m_grassUniforms.light_dir !=
          Shader::InvalidUniform) {
        QVector3D light_dir = grass.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        m_terrainPipeline->m_grassShader->setUniform(
            m_terrainPipeline->m_grassUniforms.light_dir, light_dir);
      }

      glBindVertexArray(m_terrainPipeline->m_grassVao);
      grass.instanceBuffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(GrassInstanceGpu));
      glVertexAttribPointer(
          TexCoord, Vec4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(GrassInstanceGpu, posHeight)));
      glVertexAttribPointer(
          InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(GrassInstanceGpu, colorWidth)));
      glVertexAttribPointer(
          InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(GrassInstanceGpu, swayParams)));
      grass.instanceBuffer->unbind();

      glDrawArraysInstanced(GL_TRIANGLES, 0,
                            m_terrainPipeline->m_grassVertexCount,
                            static_cast<GLsizei>(grass.instance_count));
      glBindVertexArray(0);

      if (prev_cull != 0U) {
        glEnable(GL_CULL_FACE);
      }

      break;
    }
    case StoneBatchCmdIndex: {
      if (!m_vegetationPipeline) {
        ++i;
        continue;
      }
      const auto &stone = std::get<StoneBatchCmdIndex>(cmd);
      if ((stone.instanceBuffer == nullptr) || stone.instance_count == 0 ||
          (m_vegetationPipeline->stoneShader() == nullptr) ||
          (m_vegetationPipeline->m_stoneVao == 0U) ||
          m_vegetationPipeline->m_stoneIndexCount == 0) {
        break;
      }

      DepthMaskScope const depth_mask(true);
      BlendScope const blend(false);

      Shader *stone_shader = m_vegetationPipeline->stoneShader();
      if (m_lastBoundShader != stone_shader) {
        stone_shader->use();
        m_lastBoundShader = stone_shader;
        m_lastBoundTexture = nullptr;
      }

      if (m_vegetationPipeline->m_stoneUniforms.view_proj !=
          Shader::InvalidUniform) {
        stone_shader->setUniform(
            m_vegetationPipeline->m_stoneUniforms.view_proj, view_proj);
      }
      if (m_vegetationPipeline->m_stoneUniforms.light_direction !=
          Shader::InvalidUniform) {
        QVector3D light_dir = stone.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        stone_shader->setUniform(
            m_vegetationPipeline->m_stoneUniforms.light_direction, light_dir);
      }

      glBindVertexArray(m_vegetationPipeline->m_stoneVao);
      stone.instanceBuffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(StoneInstanceGpu));
      glVertexAttribPointer(
          TexCoord, Vec4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(StoneInstanceGpu, posScale)));
      glVertexAttribPointer(
          InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(StoneInstanceGpu, colorRot)));
      stone.instanceBuffer->unbind();

      glDrawElementsInstanced(GL_TRIANGLES,
                              m_vegetationPipeline->m_stoneIndexCount,
                              GL_UNSIGNED_SHORT, nullptr,
                              static_cast<GLsizei>(stone.instance_count));
      glBindVertexArray(0);

      break;
    }
    case PlantBatchCmdIndex: {
      if (!m_vegetationPipeline) {
        ++i;
        continue;
      }
      const auto &plant = std::get<PlantBatchCmdIndex>(cmd);

      if ((plant.instanceBuffer == nullptr) || plant.instance_count == 0 ||
          (m_vegetationPipeline->plantShader() == nullptr) ||
          (m_vegetationPipeline->m_plantVao == 0U) ||
          m_vegetationPipeline->m_plantIndexCount == 0) {
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

      Shader *plant_shader = m_vegetationPipeline->plantShader();
      if (m_lastBoundShader != plant_shader) {
        plant_shader->use();
        m_lastBoundShader = plant_shader;
        m_lastBoundTexture = nullptr;
      }

      if (m_vegetationPipeline->m_plantUniforms.view_proj !=
          Shader::InvalidUniform) {
        plant_shader->setUniform(
            m_vegetationPipeline->m_plantUniforms.view_proj, view_proj);
      }
      if (m_vegetationPipeline->m_plantUniforms.time !=
          Shader::InvalidUniform) {
        plant_shader->setUniform(m_vegetationPipeline->m_plantUniforms.time,
                                 plant.params.time);
      }
      if (m_vegetationPipeline->m_plantUniforms.windStrength !=
          Shader::InvalidUniform) {
        plant_shader->setUniform(
            m_vegetationPipeline->m_plantUniforms.windStrength,
            plant.params.windStrength);
      }
      if (m_vegetationPipeline->m_plantUniforms.windSpeed !=
          Shader::InvalidUniform) {
        plant_shader->setUniform(
            m_vegetationPipeline->m_plantUniforms.windSpeed,
            plant.params.windSpeed);
      }
      if (m_vegetationPipeline->m_plantUniforms.light_direction !=
          Shader::InvalidUniform) {
        QVector3D light_dir = plant.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        plant_shader->setUniform(
            m_vegetationPipeline->m_plantUniforms.light_direction, light_dir);
      }

      glBindVertexArray(m_vegetationPipeline->m_plantVao);
      plant.instanceBuffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(PlantInstanceGpu));
      glVertexAttribPointer(
          InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(PlantInstanceGpu, posScale)));
      glVertexAttribPointer(
          InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(PlantInstanceGpu, colorSway)));
      glVertexAttribPointer(
          InstanceColor, Vec4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(PlantInstanceGpu, typeParams)));
      plant.instanceBuffer->unbind();

      glDrawElementsInstanced(GL_TRIANGLES,
                              m_vegetationPipeline->m_plantIndexCount,
                              GL_UNSIGNED_SHORT, nullptr,
                              static_cast<GLsizei>(plant.instance_count));
      glBindVertexArray(0);

      if (prev_cull != 0U) {
        glEnable(GL_CULL_FACE);
      }

      break;
    }
    case PineBatchCmdIndex: {
      if (!m_vegetationPipeline) {
        ++i;
        continue;
      }
      const auto &pine = std::get<PineBatchCmdIndex>(cmd);

      if ((pine.instanceBuffer == nullptr) || pine.instance_count == 0 ||
          (m_vegetationPipeline->pineShader() == nullptr) ||
          (m_vegetationPipeline->m_pineVao == 0U) ||
          m_vegetationPipeline->m_pineIndexCount == 0) {
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

      Shader *pine_shader = m_vegetationPipeline->pineShader();
      if (m_lastBoundShader != pine_shader) {
        pine_shader->use();
        m_lastBoundShader = pine_shader;
        m_lastBoundTexture = nullptr;
      }

      if (m_vegetationPipeline->m_pineUniforms.view_proj !=
          Shader::InvalidUniform) {
        pine_shader->setUniform(m_vegetationPipeline->m_pineUniforms.view_proj,
                                view_proj);
      }
      if (m_vegetationPipeline->m_pineUniforms.time != Shader::InvalidUniform) {
        pine_shader->setUniform(m_vegetationPipeline->m_pineUniforms.time,
                                pine.params.time);
      }
      if (m_vegetationPipeline->m_pineUniforms.windStrength !=
          Shader::InvalidUniform) {
        pine_shader->setUniform(
            m_vegetationPipeline->m_pineUniforms.windStrength,
            pine.params.windStrength);
      }
      if (m_vegetationPipeline->m_pineUniforms.windSpeed !=
          Shader::InvalidUniform) {
        pine_shader->setUniform(m_vegetationPipeline->m_pineUniforms.windSpeed,
                                pine.params.windSpeed);
      }
      if (m_vegetationPipeline->m_pineUniforms.light_direction !=
          Shader::InvalidUniform) {
        QVector3D light_dir = pine.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        pine_shader->setUniform(
            m_vegetationPipeline->m_pineUniforms.light_direction, light_dir);
      }

      glBindVertexArray(m_vegetationPipeline->m_pineVao);
      pine.instanceBuffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(PineInstanceGpu));
      glVertexAttribPointer(
          InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(PineInstanceGpu, posScale)));
      glVertexAttribPointer(
          InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(PineInstanceGpu, colorSway)));
      glVertexAttribPointer(
          InstanceColor, Vec4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(PineInstanceGpu, rotation)));
      pine.instanceBuffer->unbind();

      glDrawElementsInstanced(GL_TRIANGLES,
                              m_vegetationPipeline->m_pineIndexCount,
                              GL_UNSIGNED_SHORT, nullptr,
                              static_cast<GLsizei>(pine.instance_count));
      glBindVertexArray(0);

      if (prev_cull != 0U) {
        glEnable(GL_CULL_FACE);
      }

      break;
    }
    case FireCampBatchCmdIndex: {
      if (!m_vegetationPipeline) {
        ++i;
        continue;
      }
      const auto &firecamp = std::get<FireCampBatchCmdIndex>(cmd);

      if ((firecamp.instanceBuffer == nullptr) ||
          firecamp.instance_count == 0 ||
          (m_vegetationPipeline->firecampShader() == nullptr) ||
          (m_vegetationPipeline->m_firecampVao == 0U) ||
          m_vegetationPipeline->m_firecampIndexCount == 0) {
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

      Shader *firecamp_shader = m_vegetationPipeline->firecampShader();
      if (m_lastBoundShader != firecamp_shader) {
        firecamp_shader->use();
        m_lastBoundShader = firecamp_shader;
        m_lastBoundTexture = nullptr;
      }

      if (m_vegetationPipeline->m_firecampUniforms.view_proj !=
          Shader::InvalidUniform) {
        firecamp_shader->setUniform(
            m_vegetationPipeline->m_firecampUniforms.view_proj, view_proj);
      }
      if (m_vegetationPipeline->m_firecampUniforms.time !=
          Shader::InvalidUniform) {
        firecamp_shader->setUniform(
            m_vegetationPipeline->m_firecampUniforms.time,
            firecamp.params.time);
      }
      if (m_vegetationPipeline->m_firecampUniforms.flickerSpeed !=
          Shader::InvalidUniform) {
        firecamp_shader->setUniform(
            m_vegetationPipeline->m_firecampUniforms.flickerSpeed,
            firecamp.params.flickerSpeed);
      }
      if (m_vegetationPipeline->m_firecampUniforms.flickerAmount !=
          Shader::InvalidUniform) {
        firecamp_shader->setUniform(
            m_vegetationPipeline->m_firecampUniforms.flickerAmount,
            firecamp.params.flickerAmount);
      }
      if (m_vegetationPipeline->m_firecampUniforms.glowStrength !=
          Shader::InvalidUniform) {
        firecamp_shader->setUniform(
            m_vegetationPipeline->m_firecampUniforms.glowStrength,
            firecamp.params.glowStrength);
      }
      if (m_vegetationPipeline->m_firecampUniforms.camera_right !=
          Shader::InvalidUniform) {
        QVector3D camera_right = cam.getRightVector();
        if (camera_right.lengthSquared() < 1e-6F) {
          camera_right = QVector3D(1.0F, 0.0F, 0.0F);
        } else {
          camera_right.normalize();
        }
        firecamp_shader->setUniform(
            m_vegetationPipeline->m_firecampUniforms.camera_right,
            camera_right);
      }
      if (m_vegetationPipeline->m_firecampUniforms.camera_forward !=
          Shader::InvalidUniform) {
        QVector3D camera_forward = cam.getForwardVector();
        if (camera_forward.lengthSquared() < 1e-6F) {
          camera_forward = QVector3D(0.0F, 0.0F, -1.0F);
        } else {
          camera_forward.normalize();
        }
        firecamp_shader->setUniform(
            m_vegetationPipeline->m_firecampUniforms.camera_forward,
            camera_forward);
      }

      if (m_vegetationPipeline->m_firecampUniforms.fireTexture !=
          Shader::InvalidUniform) {
        if (m_resources && (m_resources->white() != nullptr)) {
          m_resources->white()->bind(0);
          firecamp_shader->setUniform(
              m_vegetationPipeline->m_firecampUniforms.fireTexture, 0);
        }
      }

      glBindVertexArray(m_vegetationPipeline->m_firecampVao);
      firecamp.instanceBuffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(FireCampInstanceGpu));
      glVertexAttribPointer(InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
                            reinterpret_cast<void *>(
                                offsetof(FireCampInstanceGpu, pos_intensity)));
      glVertexAttribPointer(InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
                            reinterpret_cast<void *>(
                                offsetof(FireCampInstanceGpu, radius_phase)));
      firecamp.instanceBuffer->unbind();

      glDrawElementsInstanced(GL_TRIANGLES,
                              m_vegetationPipeline->m_firecampIndexCount,
                              GL_UNSIGNED_SHORT, nullptr,
                              static_cast<GLsizei>(firecamp.instance_count));
      glBindVertexArray(0);

      if (prev_cull != 0U) {
        glEnable(GL_CULL_FACE);
      }

      break;
    }
    case TerrainChunkCmdIndex: {
      const auto &terrain = std::get<TerrainChunkCmdIndex>(cmd);

      Shader *active_shader = terrain.params.isGroundPlane
                                  ? m_terrainPipeline->m_groundShader
                                  : m_terrainPipeline->m_terrainShader;

      if ((terrain.mesh == nullptr) || (active_shader == nullptr)) {
        break;
      }

      if (m_lastBoundShader != active_shader) {
        active_shader->use();
        m_lastBoundShader = active_shader;
        m_lastBoundTexture = nullptr;
      }

      const QMatrix4x4 mvp = view_proj * terrain.model;

      if (terrain.params.isGroundPlane) {

        if (m_terrainPipeline->m_groundUniforms.mvp != Shader::InvalidUniform) {
          active_shader->setUniform(m_terrainPipeline->m_groundUniforms.mvp,
                                    mvp);
        }
        if (m_terrainPipeline->m_groundUniforms.model !=
            Shader::InvalidUniform) {
          active_shader->setUniform(m_terrainPipeline->m_groundUniforms.model,
                                    terrain.model);
        }
        if (m_terrainPipeline->m_groundUniforms.grassPrimary !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_groundUniforms.grassPrimary,
              terrain.params.grassPrimary);
        }
        if (m_terrainPipeline->m_groundUniforms.grassSecondary !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_groundUniforms.grassSecondary,
              terrain.params.grassSecondary);
        }
        if (m_terrainPipeline->m_groundUniforms.grassDry !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_groundUniforms.grassDry,
              terrain.params.grassDry);
        }
        if (m_terrainPipeline->m_groundUniforms.soilColor !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_groundUniforms.soilColor,
              terrain.params.soilColor);
        }
        if (m_terrainPipeline->m_groundUniforms.tint !=
            Shader::InvalidUniform) {
          active_shader->setUniform(m_terrainPipeline->m_groundUniforms.tint,
                                    terrain.params.tint);
        }
        if (m_terrainPipeline->m_groundUniforms.noiseOffset !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_groundUniforms.noiseOffset,
              terrain.params.noiseOffset);
        }
        if (m_terrainPipeline->m_groundUniforms.tile_size !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_groundUniforms.tile_size,
              terrain.params.tile_size);
        }
        if (m_terrainPipeline->m_groundUniforms.macroNoiseScale !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_groundUniforms.macroNoiseScale,
              terrain.params.macroNoiseScale);
        }
        if (m_terrainPipeline->m_groundUniforms.detail_noiseScale !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_groundUniforms.detail_noiseScale,
              terrain.params.detail_noiseScale);
        }
        if (m_terrainPipeline->m_groundUniforms.soilBlendHeight !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_groundUniforms.soilBlendHeight,
              terrain.params.soilBlendHeight);
        }
        if (m_terrainPipeline->m_groundUniforms.soilBlendSharpness !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_groundUniforms.soilBlendSharpness,
              terrain.params.soilBlendSharpness);
        }
        if (m_terrainPipeline->m_groundUniforms.ambientBoost !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_groundUniforms.ambientBoost,
              terrain.params.ambientBoost);
        }
        if (m_terrainPipeline->m_groundUniforms.light_dir !=
            Shader::InvalidUniform) {
          QVector3D light_dir = terrain.params.light_direction;
          if (!light_dir.isNull()) {
            light_dir.normalize();
          }
          active_shader->setUniform(
              m_terrainPipeline->m_groundUniforms.light_dir, light_dir);
        }
      } else {

        if (m_terrainPipeline->m_terrainUniforms.mvp !=
            Shader::InvalidUniform) {
          active_shader->setUniform(m_terrainPipeline->m_terrainUniforms.mvp,
                                    mvp);
        }
        if (m_terrainPipeline->m_terrainUniforms.model !=
            Shader::InvalidUniform) {
          active_shader->setUniform(m_terrainPipeline->m_terrainUniforms.model,
                                    terrain.model);
        }
        if (m_terrainPipeline->m_terrainUniforms.grassPrimary !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.grassPrimary,
              terrain.params.grassPrimary);
        }
        if (m_terrainPipeline->m_terrainUniforms.grassSecondary !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.grassSecondary,
              terrain.params.grassSecondary);
        }
        if (m_terrainPipeline->m_terrainUniforms.grassDry !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.grassDry,
              terrain.params.grassDry);
        }
        if (m_terrainPipeline->m_terrainUniforms.soilColor !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.soilColor,
              terrain.params.soilColor);
        }
        if (m_terrainPipeline->m_terrainUniforms.rockLow !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.rockLow,
              terrain.params.rockLow);
        }
        if (m_terrainPipeline->m_terrainUniforms.rockHigh !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.rockHigh,
              terrain.params.rockHigh);
        }
        if (m_terrainPipeline->m_terrainUniforms.tint !=
            Shader::InvalidUniform) {
          active_shader->setUniform(m_terrainPipeline->m_terrainUniforms.tint,
                                    terrain.params.tint);
        }
        if (m_terrainPipeline->m_terrainUniforms.noiseOffset !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.noiseOffset,
              terrain.params.noiseOffset);
        }
        if (m_terrainPipeline->m_terrainUniforms.tile_size !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.tile_size,
              terrain.params.tile_size);
        }
        if (m_terrainPipeline->m_terrainUniforms.macroNoiseScale !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.macroNoiseScale,
              terrain.params.macroNoiseScale);
        }
        if (m_terrainPipeline->m_terrainUniforms.detail_noiseScale !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.detail_noiseScale,
              terrain.params.detail_noiseScale);
        }
        if (m_terrainPipeline->m_terrainUniforms.slopeRockThreshold !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.slopeRockThreshold,
              terrain.params.slopeRockThreshold);
        }
        if (m_terrainPipeline->m_terrainUniforms.slopeRockSharpness !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.slopeRockSharpness,
              terrain.params.slopeRockSharpness);
        }
        if (m_terrainPipeline->m_terrainUniforms.soilBlendHeight !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.soilBlendHeight,
              terrain.params.soilBlendHeight);
        }
        if (m_terrainPipeline->m_terrainUniforms.soilBlendSharpness !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.soilBlendSharpness,
              terrain.params.soilBlendSharpness);
        }
        if (m_terrainPipeline->m_terrainUniforms.heightNoiseStrength !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.heightNoiseStrength,
              terrain.params.heightNoiseStrength);
        }
        if (m_terrainPipeline->m_terrainUniforms.heightNoiseFrequency !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.heightNoiseFrequency,
              terrain.params.heightNoiseFrequency);
        }
        if (m_terrainPipeline->m_terrainUniforms.ambientBoost !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.ambientBoost,
              terrain.params.ambientBoost);
        }
        if (m_terrainPipeline->m_terrainUniforms.rockDetailStrength !=
            Shader::InvalidUniform) {
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.rockDetailStrength,
              terrain.params.rockDetailStrength);
        }
        if (m_terrainPipeline->m_terrainUniforms.light_dir !=
            Shader::InvalidUniform) {
          QVector3D light_dir = terrain.params.light_direction;
          if (!light_dir.isNull()) {
            light_dir.normalize();
          }
          active_shader->setUniform(
              m_terrainPipeline->m_terrainUniforms.light_dir, light_dir);
        }
      }

      DepthMaskScope const depth_mask(terrain.depthWrite);
      std::unique_ptr<PolygonOffsetScope> poly_scope;
      if (terrain.depthBias != 0.0F) {
        poly_scope = std::make_unique<PolygonOffsetScope>(terrain.depthBias,
                                                          terrain.depthBias);
      }

      terrain.mesh->draw();
      break;
    }
    case MeshCmdIndex: {
      const auto &it = std::get<MeshCmdIndex>(cmd);
      if (it.mesh == nullptr) {
        break;
      }

      glDepthMask(GL_TRUE);
      if (glIsEnabled(GL_POLYGON_OFFSET_FILL) != 0U) {
        glDisable(GL_POLYGON_OFFSET_FILL);
      }

      Shader *active_shader =
          (it.shader != nullptr) ? it.shader : m_basicShader;
      if (active_shader == nullptr) {
        break;
      }

      if (active_shader == m_waterPipeline->m_riverShader) {
        if (m_lastBoundShader != active_shader) {
          active_shader->use();
          m_lastBoundShader = active_shader;
        }

        active_shader->setUniform(m_waterPipeline->m_riverUniforms.model,
                                  it.model);
        active_shader->setUniform(m_waterPipeline->m_riverUniforms.view,
                                  cam.getViewMatrix());
        active_shader->setUniform(m_waterPipeline->m_riverUniforms.projection,
                                  cam.getProjectionMatrix());
        active_shader->setUniform(m_waterPipeline->m_riverUniforms.time,
                                  m_animationTime);

        it.mesh->draw();
        break;
      }

      if (active_shader == m_waterPipeline->m_riverbankShader) {
        if (m_lastBoundShader != active_shader) {
          active_shader->use();
          m_lastBoundShader = active_shader;
        }

        active_shader->setUniform(m_waterPipeline->m_riverbankUniforms.model,
                                  it.model);
        active_shader->setUniform(m_waterPipeline->m_riverbankUniforms.view,
                                  cam.getViewMatrix());
        active_shader->setUniform(
            m_waterPipeline->m_riverbankUniforms.projection,
            cam.getProjectionMatrix());
        active_shader->setUniform(m_waterPipeline->m_riverbankUniforms.time,
                                  m_animationTime);

        it.mesh->draw();
        break;
      }

      if (active_shader == m_waterPipeline->m_bridgeShader) {
        if (m_lastBoundShader != active_shader) {
          active_shader->use();
          m_lastBoundShader = active_shader;
        }

        active_shader->setUniform(m_waterPipeline->m_bridgeUniforms.mvp,
                                  it.mvp);
        active_shader->setUniform(m_waterPipeline->m_bridgeUniforms.model,
                                  it.model);
        active_shader->setUniform(m_waterPipeline->m_bridgeUniforms.color,
                                  it.color);

        QVector3D const light_dir(0.35F, 0.8F, 0.45F);
        active_shader->setUniform(
            m_waterPipeline->m_bridgeUniforms.light_direction, light_dir);

        it.mesh->draw();
        break;
      }

      BackendPipelines::CharacterPipeline::BasicUniforms *uniforms =
          &m_characterPipeline->m_basicUniforms;
      if (active_shader == m_characterPipeline->m_archerShader) {
        uniforms = &m_characterPipeline->m_archerUniforms;
      } else if (active_shader == m_characterPipeline->m_swordsmanShader) {
        uniforms = &m_characterPipeline->m_swordsmanUniforms;
      } else if (active_shader == m_characterPipeline->m_spearmanShader) {
        uniforms = &m_characterPipeline->m_spearmanUniforms;
      }

      if (m_lastBoundShader != active_shader) {
        active_shader->use();
        m_lastBoundShader = active_shader;
      }

      active_shader->setUniform(uniforms->mvp, it.mvp);
      active_shader->setUniform(uniforms->model, it.model);

      Texture *tex_to_use =
          (it.texture != nullptr)
              ? it.texture
              : (m_resources ? m_resources->white() : nullptr);
      if ((tex_to_use != nullptr) && tex_to_use != m_lastBoundTexture) {
        tex_to_use->bind(0);
        m_lastBoundTexture = tex_to_use;
        active_shader->setUniform(uniforms->texture, 0);
      }

      active_shader->setUniform(uniforms->useTexture, it.texture != nullptr);
      active_shader->setUniform(uniforms->color, it.color);
      active_shader->setUniform(uniforms->alpha, it.alpha);
      it.mesh->draw();
      break;
    }
    case GridCmdIndex: {
      if (m_effectsPipeline->m_gridShader == nullptr) {
        break;
      }
      const auto &gc = std::get<GridCmdIndex>(cmd);

      if (m_lastBoundShader != m_effectsPipeline->m_gridShader) {
        m_effectsPipeline->m_gridShader->use();
        m_lastBoundShader = m_effectsPipeline->m_gridShader;
      }

      m_effectsPipeline->m_gridShader->setUniform(
          m_effectsPipeline->m_gridUniforms.mvp, gc.mvp);
      m_effectsPipeline->m_gridShader->setUniform(
          m_effectsPipeline->m_gridUniforms.model, gc.model);
      m_effectsPipeline->m_gridShader->setUniform(
          m_effectsPipeline->m_gridUniforms.gridColor, gc.color);
      m_effectsPipeline->m_gridShader->setUniform(
          m_effectsPipeline->m_gridUniforms.lineColor, k_grid_line_color);
      m_effectsPipeline->m_gridShader->setUniform(
          m_effectsPipeline->m_gridUniforms.cellSize, gc.cellSize);
      m_effectsPipeline->m_gridShader->setUniform(
          m_effectsPipeline->m_gridUniforms.thickness, gc.thickness);

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

      if (m_lastBoundShader != m_effectsPipeline->m_basicShader) {
        m_effectsPipeline->m_basicShader->use();
        m_lastBoundShader = m_effectsPipeline->m_basicShader;
      }

      m_effectsPipeline->m_basicShader->use();
      m_effectsPipeline->m_basicShader->setUniform(
          m_effectsPipeline->m_basicUniforms.useTexture, false);
      m_effectsPipeline->m_basicShader->setUniform(
          m_effectsPipeline->m_basicUniforms.color, sc.color);

      DepthMaskScope const depth_mask(false);
      PolygonOffsetScope const poly(-1.0F, -1.0F);
      BlendScope const blend(true);

      {
        QMatrix4x4 m = sc.model;
        m.scale(1.08F, 1.0F, 1.08F);
        const QMatrix4x4 mvp = view_proj * m;
        m_effectsPipeline->m_basicShader->setUniform(
            m_effectsPipeline->m_basicUniforms.mvp, mvp);
        m_effectsPipeline->m_basicShader->setUniform(
            m_effectsPipeline->m_basicUniforms.model, m);
        m_effectsPipeline->m_basicShader->setUniform(
            m_effectsPipeline->m_basicUniforms.alpha, sc.alphaOuter);
        ring->draw();
      }

      {
        const QMatrix4x4 mvp = view_proj * sc.model;
        m_effectsPipeline->m_basicShader->setUniform(
            m_effectsPipeline->m_basicUniforms.mvp, mvp);
        m_effectsPipeline->m_basicShader->setUniform(
            m_effectsPipeline->m_basicUniforms.model, sc.model);
        m_effectsPipeline->m_basicShader->setUniform(
            m_effectsPipeline->m_basicUniforms.alpha, sc.alphaInner);
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

      if (m_lastBoundShader != m_effectsPipeline->m_basicShader) {
        m_effectsPipeline->m_basicShader->use();
        m_lastBoundShader = m_effectsPipeline->m_basicShader;
      }
      m_effectsPipeline->m_basicShader->setUniform(
          m_effectsPipeline->m_basicUniforms.useTexture, false);
      m_effectsPipeline->m_basicShader->setUniform(
          m_effectsPipeline->m_basicUniforms.color, sm.color);
      DepthMaskScope const depth_mask(false);
      DepthTestScope const depth_test(true);

      PolygonOffsetScope const poly(-1.0F, -1.0F);
      BlendScope const blend(true);
      for (int i = 0; i < 7; ++i) {
        float const scale = 1.35F + 0.12F * i;
        float const a = sm.baseAlpha * (1.0F - 0.09F * i);
        QMatrix4x4 m = sm.model;
        m.translate(0.0F, 0.02F, 0.0F);
        m.scale(scale, 1.0F, scale);
        const QMatrix4x4 mvp = view_proj * m;
        m_effectsPipeline->m_basicShader->setUniform(
            m_effectsPipeline->m_basicUniforms.mvp, mvp);
        m_effectsPipeline->m_basicShader->setUniform(
            m_effectsPipeline->m_basicUniforms.model, m);
        m_effectsPipeline->m_basicShader->setUniform(
            m_effectsPipeline->m_basicUniforms.alpha, a);
        disc->draw();
      }
      break;
    }
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
