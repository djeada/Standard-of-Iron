#include "backend.h"
#include "../draw_queue.h"
#include "../geom/mode_indicator.h"
#include "../geom/selection_disc.h"
#include "../geom/selection_ring.h"
#include "../primitive_batch.h"
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
#include "backend/terrain_pipeline.h"
#include "backend/vegetation_pipeline.h"
#include "backend/water_pipeline.h"
#include "buffer.h"
#include "gl/camera.h"
#include "gl/resources.h"
#include "ground/firecamp_gpu.h"
#include "ground/grass_gpu.h"
#include "ground/olive_gpu.h"
#include "ground/pine_gpu.h"
#include "ground/plant_gpu.h"
#include "ground/rain_gpu.h"
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

[[nodiscard]] inline auto can_batch_mesh_cmds(const MeshCmd &a,
                                              const MeshCmd &b) -> bool {

  if (a.alpha < k_opaque_threshold || b.alpha < k_opaque_threshold) {
    return false;
  }
  return a.mesh == b.mesh && a.shader == b.shader && a.texture == b.texture;
}

} // namespace

Backend::Backend() = default;

Backend::~Backend() {

  if (QOpenGLContext::currentContext() == nullptr) {

    (void)m_cylinderPipeline.release();
    (void)m_vegetationPipeline.release();
    (void)m_terrainPipeline.release();
    (void)m_characterPipeline.release();
    (void)m_waterPipeline.release();
    (void)m_effectsPipeline.release();
    (void)m_meshInstancingPipeline.release();
  } else {

    m_cylinderPipeline.reset();
    m_vegetationPipeline.reset();
    m_terrainPipeline.reset();
    m_characterPipeline.reset();
    m_waterPipeline.reset();
    m_effectsPipeline.reset();
    m_meshInstancingPipeline.reset();
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
  m_shaderCache->initialize_defaults();
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

  qInfo() << "Backend: Creating PrimitiveBatchPipeline...";
  m_primitiveBatchPipeline =
      std::make_unique<BackendPipelines::PrimitiveBatchPipeline>(
          m_shaderCache.get());
  m_primitiveBatchPipeline->initialize();
  qInfo() << "Backend: PrimitiveBatchPipeline initialized";

  qInfo() << "Backend: Creating BannerPipeline...";
  m_bannerPipeline = std::make_unique<BackendPipelines::BannerPipeline>(
      this, m_shaderCache.get());
  m_bannerPipeline->initialize();
  qInfo() << "Backend: BannerPipeline initialized";

  qInfo() << "Backend: Creating HealingBeamPipeline...";
  m_healingBeamPipeline =
      std::make_unique<BackendPipelines::HealingBeamPipeline>(
          this, m_shaderCache.get());
  m_healingBeamPipeline->initialize();
  qInfo() << "Backend: HealingBeamPipeline initialized";

  qInfo() << "Backend: Creating HealerAuraPipeline...";
  m_healerAuraPipeline = std::make_unique<BackendPipelines::HealerAuraPipeline>(
      this, m_shaderCache.get());
  m_healerAuraPipeline->initialize();
  qInfo() << "Backend: HealerAuraPipeline initialized";

  qInfo() << "Backend: Creating CombatDustPipeline...";
  m_combatDustPipeline = std::make_unique<BackendPipelines::CombatDustPipeline>(
      this, m_shaderCache.get());
  m_combatDustPipeline->initialize();
  qInfo() << "Backend: CombatDustPipeline initialized";

  qInfo() << "Backend: Creating RainPipeline...";
  m_rainPipeline = std::make_unique<BackendPipelines::RainPipeline>(
      this, m_shaderCache.get());
  m_rainPipeline->initialize();
  qInfo() << "Backend: RainPipeline initialized";

  qInfo() << "Backend: Creating ModeIndicatorPipeline...";
  m_modeIndicatorPipeline =
      std::make_unique<BackendPipelines::ModeIndicatorPipeline>(
          this, m_shaderCache.get());
  m_modeIndicatorPipeline->initialize();
  qInfo() << "Backend: ModeIndicatorPipeline initialized";

  qInfo() << "Backend: Creating MeshInstancingPipeline...";
  m_meshInstancingPipeline =
      std::make_unique<BackendPipelines::MeshInstancingPipeline>(
          this, m_shaderCache.get());
  m_meshInstancingPipeline->initialize();
  qInfo() << "Backend: MeshInstancingPipeline initialized";

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

auto Backend::banner_mesh() const -> Mesh * {
  if (m_bannerPipeline != nullptr) {
    return m_bannerPipeline->get_banner_mesh();
  }
  return nullptr;
}

auto Backend::banner_shader() const -> Shader * {
  if (m_bannerPipeline != nullptr) {
    return m_bannerPipeline->m_bannerShader;
  }
  return nullptr;
}

void Backend::begin_frame() {
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
    m_cylinderPipeline->begin_frame();
  }
  if (m_meshInstancingPipeline) {
    m_meshInstancingPipeline->begin_frame();
  }
}

void Backend::set_viewport(int w, int h) {
  m_viewportWidth = w;
  m_viewportHeight = h;
}

void Backend::set_clear_color(float r, float g, float b, float a) {
  m_clearColor[Red] = r;
  m_clearColor[Green] = g;
  m_clearColor[Blue] = b;
  m_clearColor[Alpha] = a;
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
    case CylinderCmdIndex: {
      if (!m_cylinderPipeline) {
        ++i;
        continue;
      }
      m_cylinderPipeline->m_cylinderScratch.clear();
      do {
        const auto &cy = std::get<CylinderCmdIndex>(queue.get_sorted(i));
        BackendPipelines::CylinderPipeline::CylinderInstanceGpu gpu{};
        gpu.start = cy.start;
        gpu.end = cy.end;
        gpu.radius = cy.radius;
        gpu.alpha = cy.alpha;
        gpu.color = cy.color;
        m_cylinderPipeline->m_cylinderScratch.emplace_back(gpu);
        ++i;
      } while (i < count && queue.get_sorted(i).index() == CylinderCmdIndex);

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
          cylinder_shader->set_uniform(
              m_cylinderPipeline->m_cylinderUniforms.view_proj, view_proj);
        }
        m_cylinderPipeline->upload_cylinder_instances(instance_count);
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
          fog_shader->set_uniform(m_cylinderPipeline->m_fogUniforms.view_proj,
                                  view_proj);
        }
        m_cylinderPipeline->upload_fog_instances(instance_count);
        m_cylinderPipeline->draw_fog(instance_count);
      }
      ++i;
      continue;
    }
    case GrassBatchCmdIndex: {
      const auto &grass = std::get<GrassBatchCmdIndex>(cmd);
      if ((grass.instance_buffer == nullptr) || grass.instance_count == 0 ||
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
        m_terrainPipeline->m_grassShader->set_uniform(
            m_terrainPipeline->m_grassUniforms.view_proj, view_proj);
      }
      if (m_terrainPipeline->m_grassUniforms.time != Shader::InvalidUniform) {
        m_terrainPipeline->m_grassShader->set_uniform(
            m_terrainPipeline->m_grassUniforms.time, grass.params.time);
      }
      if (m_terrainPipeline->m_grassUniforms.wind_strength !=
          Shader::InvalidUniform) {
        m_terrainPipeline->m_grassShader->set_uniform(
            m_terrainPipeline->m_grassUniforms.wind_strength,
            grass.params.wind_strength);
      }
      if (m_terrainPipeline->m_grassUniforms.wind_speed !=
          Shader::InvalidUniform) {
        m_terrainPipeline->m_grassShader->set_uniform(
            m_terrainPipeline->m_grassUniforms.wind_speed,
            grass.params.wind_speed);
      }
      if (m_terrainPipeline->m_grassUniforms.soil_color !=
          Shader::InvalidUniform) {
        m_terrainPipeline->m_grassShader->set_uniform(
            m_terrainPipeline->m_grassUniforms.soil_color,
            grass.params.soil_color);
      }
      if (m_terrainPipeline->m_grassUniforms.light_dir !=
          Shader::InvalidUniform) {
        QVector3D light_dir = grass.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        m_terrainPipeline->m_grassShader->set_uniform(
            m_terrainPipeline->m_grassUniforms.light_dir, light_dir);
      }

      glBindVertexArray(m_terrainPipeline->m_grassVao);
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
      if ((stone.instance_buffer == nullptr) || stone.instance_count == 0 ||
          (m_vegetationPipeline->stone_shader() == nullptr) ||
          (m_vegetationPipeline->m_stoneVao == 0U) ||
          m_vegetationPipeline->m_stoneIndexCount == 0) {
        break;
      }

      DepthMaskScope const depth_mask(true);
      BlendScope const blend(false);

      Shader *stone_shader = m_vegetationPipeline->stone_shader();
      if (m_lastBoundShader != stone_shader) {
        stone_shader->use();
        m_lastBoundShader = stone_shader;
        m_lastBoundTexture = nullptr;
      }

      if (m_vegetationPipeline->m_stoneUniforms.view_proj !=
          Shader::InvalidUniform) {
        stone_shader->set_uniform(
            m_vegetationPipeline->m_stoneUniforms.view_proj, view_proj);
      }
      if (m_vegetationPipeline->m_stoneUniforms.light_direction !=
          Shader::InvalidUniform) {
        QVector3D light_dir = stone.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        stone_shader->set_uniform(
            m_vegetationPipeline->m_stoneUniforms.light_direction, light_dir);
      }

      glBindVertexArray(m_vegetationPipeline->m_stoneVao);
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

      if ((plant.instance_buffer == nullptr) || plant.instance_count == 0 ||
          (m_vegetationPipeline->plant_shader() == nullptr) ||
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

      Shader *plant_shader = m_vegetationPipeline->plant_shader();
      if (m_lastBoundShader != plant_shader) {
        plant_shader->use();
        m_lastBoundShader = plant_shader;
        m_lastBoundTexture = nullptr;
      }

      if (m_vegetationPipeline->m_plantUniforms.view_proj !=
          Shader::InvalidUniform) {
        plant_shader->set_uniform(
            m_vegetationPipeline->m_plantUniforms.view_proj, view_proj);
      }
      if (m_vegetationPipeline->m_plantUniforms.time !=
          Shader::InvalidUniform) {
        plant_shader->set_uniform(m_vegetationPipeline->m_plantUniforms.time,
                                  plant.params.time);
      }
      if (m_vegetationPipeline->m_plantUniforms.wind_strength !=
          Shader::InvalidUniform) {
        plant_shader->set_uniform(
            m_vegetationPipeline->m_plantUniforms.wind_strength,
            plant.params.wind_strength);
      }
      if (m_vegetationPipeline->m_plantUniforms.wind_speed !=
          Shader::InvalidUniform) {
        plant_shader->set_uniform(
            m_vegetationPipeline->m_plantUniforms.wind_speed,
            plant.params.wind_speed);
      }
      if (m_vegetationPipeline->m_plantUniforms.light_direction !=
          Shader::InvalidUniform) {
        QVector3D light_dir = plant.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        plant_shader->set_uniform(
            m_vegetationPipeline->m_plantUniforms.light_direction, light_dir);
      }

      glBindVertexArray(m_vegetationPipeline->m_plantVao);
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

      if ((pine.instance_buffer == nullptr) || pine.instance_count == 0 ||
          (m_vegetationPipeline->pine_shader() == nullptr) ||
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

      Shader *pine_shader = m_vegetationPipeline->pine_shader();
      if (m_lastBoundShader != pine_shader) {
        pine_shader->use();
        m_lastBoundShader = pine_shader;
        m_lastBoundTexture = nullptr;
      }

      if (m_vegetationPipeline->m_pineUniforms.view_proj !=
          Shader::InvalidUniform) {
        pine_shader->set_uniform(m_vegetationPipeline->m_pineUniforms.view_proj,
                                 view_proj);
      }
      if (m_vegetationPipeline->m_pineUniforms.time != Shader::InvalidUniform) {
        pine_shader->set_uniform(m_vegetationPipeline->m_pineUniforms.time,
                                 pine.params.time);
      }
      if (m_vegetationPipeline->m_pineUniforms.wind_strength !=
          Shader::InvalidUniform) {
        pine_shader->set_uniform(
            m_vegetationPipeline->m_pineUniforms.wind_strength,
            pine.params.wind_strength);
      }
      if (m_vegetationPipeline->m_pineUniforms.wind_speed !=
          Shader::InvalidUniform) {
        pine_shader->set_uniform(
            m_vegetationPipeline->m_pineUniforms.wind_speed,
            pine.params.wind_speed);
      }
      if (m_vegetationPipeline->m_pineUniforms.light_direction !=
          Shader::InvalidUniform) {
        QVector3D light_dir = pine.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        pine_shader->set_uniform(
            m_vegetationPipeline->m_pineUniforms.light_direction, light_dir);
      }

      glBindVertexArray(m_vegetationPipeline->m_pineVao);
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
                              m_vegetationPipeline->m_pineIndexCount,
                              GL_UNSIGNED_SHORT, nullptr,
                              static_cast<GLsizei>(pine.instance_count));
      glBindVertexArray(0);

      if (prev_cull != 0U) {
        glEnable(GL_CULL_FACE);
      }

      break;
    }
    case OliveBatchCmdIndex: {
      if (!m_vegetationPipeline) {
        ++i;
        continue;
      }
      const auto &olive = std::get<OliveBatchCmdIndex>(cmd);

      if ((olive.instance_buffer == nullptr) || olive.instance_count == 0 ||
          (m_vegetationPipeline->olive_shader() == nullptr) ||
          (m_vegetationPipeline->m_oliveVao == 0U) ||
          m_vegetationPipeline->m_oliveIndexCount == 0) {
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

      Shader *olive_shader = m_vegetationPipeline->olive_shader();
      if (m_lastBoundShader != olive_shader) {
        olive_shader->use();
        m_lastBoundShader = olive_shader;
        m_lastBoundTexture = nullptr;
      }

      if (m_vegetationPipeline->m_oliveUniforms.view_proj !=
          Shader::InvalidUniform) {
        olive_shader->set_uniform(
            m_vegetationPipeline->m_oliveUniforms.view_proj, view_proj);
      }
      if (m_vegetationPipeline->m_oliveUniforms.time !=
          Shader::InvalidUniform) {
        olive_shader->set_uniform(m_vegetationPipeline->m_oliveUniforms.time,
                                  olive.params.time);
      }
      if (m_vegetationPipeline->m_oliveUniforms.wind_strength !=
          Shader::InvalidUniform) {
        olive_shader->set_uniform(
            m_vegetationPipeline->m_oliveUniforms.wind_strength,
            olive.params.wind_strength);
      }
      if (m_vegetationPipeline->m_oliveUniforms.wind_speed !=
          Shader::InvalidUniform) {
        olive_shader->set_uniform(
            m_vegetationPipeline->m_oliveUniforms.wind_speed,
            olive.params.wind_speed);
      }
      if (m_vegetationPipeline->m_oliveUniforms.light_direction !=
          Shader::InvalidUniform) {
        QVector3D light_dir = olive.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        olive_shader->set_uniform(
            m_vegetationPipeline->m_oliveUniforms.light_direction, light_dir);
      }

      glBindVertexArray(m_vegetationPipeline->m_oliveVao);
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
                              m_vegetationPipeline->m_oliveIndexCount,
                              GL_UNSIGNED_SHORT, nullptr,
                              static_cast<GLsizei>(olive.instance_count));
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

      if ((firecamp.instance_buffer == nullptr) ||
          firecamp.instance_count == 0 ||
          (m_vegetationPipeline->firecamp_shader() == nullptr) ||
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

      Shader *firecamp_shader = m_vegetationPipeline->firecamp_shader();
      if (m_lastBoundShader != firecamp_shader) {
        firecamp_shader->use();
        m_lastBoundShader = firecamp_shader;
        m_lastBoundTexture = nullptr;
      }

      if (m_vegetationPipeline->m_firecampUniforms.view_proj !=
          Shader::InvalidUniform) {
        firecamp_shader->set_uniform(
            m_vegetationPipeline->m_firecampUniforms.view_proj, view_proj);
      }
      if (m_vegetationPipeline->m_firecampUniforms.time !=
          Shader::InvalidUniform) {
        firecamp_shader->set_uniform(
            m_vegetationPipeline->m_firecampUniforms.time,
            firecamp.params.time);
      }
      if (m_vegetationPipeline->m_firecampUniforms.flickerSpeed !=
          Shader::InvalidUniform) {
        firecamp_shader->set_uniform(
            m_vegetationPipeline->m_firecampUniforms.flickerSpeed,
            firecamp.params.flicker_speed);
      }
      if (m_vegetationPipeline->m_firecampUniforms.flickerAmount !=
          Shader::InvalidUniform) {
        firecamp_shader->set_uniform(
            m_vegetationPipeline->m_firecampUniforms.flickerAmount,
            firecamp.params.flicker_amount);
      }
      if (m_vegetationPipeline->m_firecampUniforms.glowStrength !=
          Shader::InvalidUniform) {
        firecamp_shader->set_uniform(
            m_vegetationPipeline->m_firecampUniforms.glowStrength,
            firecamp.params.glow_strength);
      }
      if (m_vegetationPipeline->m_firecampUniforms.camera_right !=
          Shader::InvalidUniform) {
        QVector3D camera_right = cam.get_right_vector();
        if (camera_right.lengthSquared() < 1e-6F) {
          camera_right = QVector3D(1.0F, 0.0F, 0.0F);
        } else {
          camera_right.normalize();
        }
        firecamp_shader->set_uniform(
            m_vegetationPipeline->m_firecampUniforms.camera_right,
            camera_right);
      }
      if (m_vegetationPipeline->m_firecampUniforms.camera_forward !=
          Shader::InvalidUniform) {
        QVector3D camera_forward = cam.get_forward_vector();
        if (camera_forward.lengthSquared() < 1e-6F) {
          camera_forward = QVector3D(0.0F, 0.0F, -1.0F);
        } else {
          camera_forward.normalize();
        }
        firecamp_shader->set_uniform(
            m_vegetationPipeline->m_firecampUniforms.camera_forward,
            camera_forward);
      }

      if (m_vegetationPipeline->m_firecampUniforms.fireTexture !=
          Shader::InvalidUniform) {
        if (m_resources && (m_resources->white() != nullptr)) {
          m_resources->white()->bind(0);
          firecamp_shader->set_uniform(
              m_vegetationPipeline->m_firecampUniforms.fireTexture, 0);
        }
      }

      glBindVertexArray(m_vegetationPipeline->m_firecampVao);
      firecamp.instance_buffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(FireCampInstanceGpu));
      glVertexAttribPointer(InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
                            reinterpret_cast<void *>(
                                offsetof(FireCampInstanceGpu, pos_intensity)));
      glVertexAttribPointer(InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
                            reinterpret_cast<void *>(
                                offsetof(FireCampInstanceGpu, radius_phase)));
      firecamp.instance_buffer->unbind();

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
    case RainBatchCmdIndex: {
      const auto &rain = std::get<RainBatchCmdIndex>(cmd);
      if (m_rainPipeline == nullptr || !m_rainPipeline->is_initialized()) {
        break;
      }
      m_rainPipeline->render(cam, rain.params);
      break;
    }
    case TerrainChunkCmdIndex: {
      const auto &terrain = std::get<TerrainChunkCmdIndex>(cmd);

      Shader *active_shader = terrain.params.is_ground_plane
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

      if (terrain.params.is_ground_plane) {

        if (m_terrainPipeline->m_groundUniforms.mvp != Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrainPipeline->m_groundUniforms.mvp,
                                     mvp);
        }
        if (m_terrainPipeline->m_groundUniforms.model !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrainPipeline->m_groundUniforms.model,
                                     terrain.model);
        }
        if (m_terrainPipeline->m_groundUniforms.grass_primary !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.grass_primary,
              terrain.params.grass_primary);
        }
        if (m_terrainPipeline->m_groundUniforms.grass_secondary !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.grass_secondary,
              terrain.params.grass_secondary);
        }
        if (m_terrainPipeline->m_groundUniforms.grass_dry !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.grass_dry,
              terrain.params.grass_dry);
        }
        if (m_terrainPipeline->m_groundUniforms.soil_color !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.soil_color,
              terrain.params.soil_color);
        }
        if (m_terrainPipeline->m_groundUniforms.tint !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrainPipeline->m_groundUniforms.tint,
                                     terrain.params.tint);
        }
        if (m_terrainPipeline->m_groundUniforms.noise_offset !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.noise_offset,
              terrain.params.noise_offset);
        }
        if (m_terrainPipeline->m_groundUniforms.tile_size !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.tile_size,
              terrain.params.tile_size);
        }
        if (m_terrainPipeline->m_groundUniforms.macro_noise_scale !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.macro_noise_scale,
              terrain.params.macro_noise_scale);
        }
        if (m_terrainPipeline->m_groundUniforms.detail_noise_scale !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.detail_noise_scale,
              terrain.params.detail_noise_scale);
        }
        if (m_terrainPipeline->m_groundUniforms.soil_blend_height !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.soil_blend_height,
              terrain.params.soil_blend_height);
        }
        if (m_terrainPipeline->m_groundUniforms.soil_blend_sharpness !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.soil_blend_sharpness,
              terrain.params.soil_blend_sharpness);
        }
        if (m_terrainPipeline->m_groundUniforms.height_noise_strength !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.height_noise_strength,
              terrain.params.height_noise_strength);
        }
        if (m_terrainPipeline->m_groundUniforms.height_noise_frequency !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.height_noise_frequency,
              terrain.params.height_noise_frequency);
        }
        if (m_terrainPipeline->m_groundUniforms.ambient_boost !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.ambient_boost,
              terrain.params.ambient_boost);
        }
        if (m_terrainPipeline->m_groundUniforms.light_dir !=
            Shader::InvalidUniform) {
          QVector3D light_dir = terrain.params.light_direction;
          if (!light_dir.isNull()) {
            light_dir.normalize();
          }
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.light_dir, light_dir);
        }

        if (m_terrainPipeline->m_groundUniforms.snow_coverage !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.snow_coverage,
              terrain.params.snow_coverage);
        }
        if (m_terrainPipeline->m_groundUniforms.moisture_level !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.moisture_level,
              terrain.params.moisture_level);
        }
        if (m_terrainPipeline->m_groundUniforms.crack_intensity !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.crack_intensity,
              terrain.params.crack_intensity);
        }
        if (m_terrainPipeline->m_groundUniforms.grass_saturation !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.grass_saturation,
              terrain.params.grass_saturation);
        }
        if (m_terrainPipeline->m_groundUniforms.soil_roughness !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.soil_roughness,
              terrain.params.soil_roughness);
        }
        if (m_terrainPipeline->m_groundUniforms.snow_color !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_groundUniforms.snow_color,
              terrain.params.snow_color);
        }
      } else {

        if (m_terrainPipeline->m_terrainUniforms.mvp !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrainPipeline->m_terrainUniforms.mvp,
                                     mvp);
        }
        if (m_terrainPipeline->m_terrainUniforms.model !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrainPipeline->m_terrainUniforms.model,
                                     terrain.model);
        }
        if (m_terrainPipeline->m_terrainUniforms.grass_primary !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.grass_primary,
              terrain.params.grass_primary);
        }
        if (m_terrainPipeline->m_terrainUniforms.grass_secondary !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.grass_secondary,
              terrain.params.grass_secondary);
        }
        if (m_terrainPipeline->m_terrainUniforms.grass_dry !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.grass_dry,
              terrain.params.grass_dry);
        }
        if (m_terrainPipeline->m_terrainUniforms.soil_color !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.soil_color,
              terrain.params.soil_color);
        }
        if (m_terrainPipeline->m_terrainUniforms.rock_low !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.rock_low,
              terrain.params.rock_low);
        }
        if (m_terrainPipeline->m_terrainUniforms.rock_high !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.rock_high,
              terrain.params.rock_high);
        }
        if (m_terrainPipeline->m_terrainUniforms.tint !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(m_terrainPipeline->m_terrainUniforms.tint,
                                     terrain.params.tint);
        }
        if (m_terrainPipeline->m_terrainUniforms.noise_offset !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.noise_offset,
              terrain.params.noise_offset);
        }
        if (m_terrainPipeline->m_terrainUniforms.tile_size !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.tile_size,
              terrain.params.tile_size);
        }
        if (m_terrainPipeline->m_terrainUniforms.macro_noise_scale !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.macro_noise_scale,
              terrain.params.macro_noise_scale);
        }
        if (m_terrainPipeline->m_terrainUniforms.detail_noise_scale !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.detail_noise_scale,
              terrain.params.detail_noise_scale);
        }
        if (m_terrainPipeline->m_terrainUniforms.slope_rock_threshold !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.slope_rock_threshold,
              terrain.params.slope_rock_threshold);
        }
        if (m_terrainPipeline->m_terrainUniforms.slope_rock_sharpness !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.slope_rock_sharpness,
              terrain.params.slope_rock_sharpness);
        }
        if (m_terrainPipeline->m_terrainUniforms.soil_blend_height !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.soil_blend_height,
              terrain.params.soil_blend_height);
        }
        if (m_terrainPipeline->m_terrainUniforms.soil_blend_sharpness !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.soil_blend_sharpness,
              terrain.params.soil_blend_sharpness);
        }
        if (m_terrainPipeline->m_terrainUniforms.height_noise_strength !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.height_noise_strength,
              terrain.params.height_noise_strength);
        }
        if (m_terrainPipeline->m_terrainUniforms.height_noise_frequency !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.height_noise_frequency,
              terrain.params.height_noise_frequency);
        }
        if (m_terrainPipeline->m_terrainUniforms.ambient_boost !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.ambient_boost,
              terrain.params.ambient_boost);
        }
        if (m_terrainPipeline->m_terrainUniforms.rock_detail_strength !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.rock_detail_strength,
              terrain.params.rock_detail_strength);
        }
        if (m_terrainPipeline->m_terrainUniforms.light_dir !=
            Shader::InvalidUniform) {
          QVector3D light_dir = terrain.params.light_direction;
          if (!light_dir.isNull()) {
            light_dir.normalize();
          }
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.light_dir, light_dir);
        }

        if (m_terrainPipeline->m_terrainUniforms.snow_coverage !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.snow_coverage,
              terrain.params.snow_coverage);
        }
        if (m_terrainPipeline->m_terrainUniforms.moisture_level !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.moisture_level,
              terrain.params.moisture_level);
        }
        if (m_terrainPipeline->m_terrainUniforms.crack_intensity !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.crack_intensity,
              terrain.params.crack_intensity);
        }
        if (m_terrainPipeline->m_terrainUniforms.rock_exposure !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.rock_exposure,
              terrain.params.rock_exposure);
        }
        if (m_terrainPipeline->m_terrainUniforms.grass_saturation !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.grass_saturation,
              terrain.params.grass_saturation);
        }
        if (m_terrainPipeline->m_terrainUniforms.soil_roughness !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.soil_roughness,
              terrain.params.soil_roughness);
        }
        if (m_terrainPipeline->m_terrainUniforms.snow_color !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_terrainPipeline->m_terrainUniforms.snow_color,
              terrain.params.snow_color);
        }
      }

      DepthMaskScope const depth_mask(terrain.depth_write);
      std::unique_ptr<PolygonOffsetScope> poly_scope;
      if (terrain.depth_bias != 0.0F) {
        poly_scope = std::make_unique<PolygonOffsetScope>(terrain.depth_bias,
                                                          terrain.depth_bias);
      }

      terrain.mesh->draw();
      break;
    }
    case MeshCmdIndex: {
      const auto &it = std::get<MeshCmdIndex>(cmd);
      if (it.mesh == nullptr) {
        break;
      }

      Shader *active_shader =
          (it.shader != nullptr) ? it.shader : m_basicShader;
      if (active_shader == nullptr) {
        break;
      }

      if (glIsEnabled(GL_POLYGON_OFFSET_FILL) != 0U) {
        glDisable(GL_POLYGON_OFFSET_FILL);
      }

      Shader *shadowShader =
          m_shaderCache ? m_shaderCache->get(QStringLiteral("troop_shadow"))
                        : nullptr;
      bool const isShadowShader = (active_shader == shadowShader);
      std::unique_ptr<DepthMaskScope> shadow_depth_scope;
      std::unique_ptr<BlendScope> shadow_blend_scope;
      if (isShadowShader) {
        shadow_depth_scope = std::make_unique<DepthMaskScope>(false);
        shadow_blend_scope = std::make_unique<BlendScope>(true);
      } else {
        glDepthMask(GL_TRUE);
      }

      bool const isTransparent = (!isShadowShader) && (it.alpha < 0.999F);
      std::unique_ptr<DepthMaskScope> transparent_depth_scope;
      std::unique_ptr<BlendScope> transparent_blend_scope;
      GLint prev_depth_func = GL_LESS;
      if (isTransparent) {
        glGetIntegerv(GL_DEPTH_FUNC, &prev_depth_func);
        transparent_depth_scope = std::make_unique<DepthMaskScope>(false);
        transparent_blend_scope = std::make_unique<BlendScope>(true);
        glDepthFunc(GL_LEQUAL);
      }

      if (active_shader == m_waterPipeline->m_riverShader) {
        if (m_lastBoundShader != active_shader) {
          active_shader->use();
          m_lastBoundShader = active_shader;
        }

        active_shader->set_uniform(m_waterPipeline->m_riverUniforms.model,
                                   it.model);
        active_shader->set_uniform(m_waterPipeline->m_riverUniforms.view,
                                   cam.get_view_matrix());
        active_shader->set_uniform(m_waterPipeline->m_riverUniforms.projection,
                                   cam.get_projection_matrix());
        active_shader->set_uniform(m_waterPipeline->m_riverUniforms.time,
                                   m_animationTime);

        it.mesh->draw();
        break;
      }

      if (active_shader == m_waterPipeline->m_riverbankShader) {
        if (m_lastBoundShader != active_shader) {
          active_shader->use();
          m_lastBoundShader = active_shader;
        }

        active_shader->set_uniform(m_waterPipeline->m_riverbankUniforms.model,
                                   it.model);
        active_shader->set_uniform(m_waterPipeline->m_riverbankUniforms.view,
                                   cam.get_view_matrix());
        active_shader->set_uniform(
            m_waterPipeline->m_riverbankUniforms.projection,
            cam.get_projection_matrix());
        active_shader->set_uniform(m_waterPipeline->m_riverbankUniforms.time,
                                   m_animationTime);

        if (m_waterPipeline->m_riverbankUniforms.segment_visibility !=
            Shader::InvalidUniform) {
          active_shader->set_uniform(
              m_waterPipeline->m_riverbankUniforms.segment_visibility,
              it.alpha);
        }

        if (m_waterPipeline->m_riverbankUniforms.has_visibility !=
            Shader::InvalidUniform) {
          int const has_vis = m_riverbankVisibility.enabled ? 1 : 0;
          active_shader->set_uniform(
              m_waterPipeline->m_riverbankUniforms.has_visibility, has_vis);
        }

        if (m_riverbankVisibility.enabled &&
            m_riverbankVisibility.texture != nullptr) {
          if (m_waterPipeline->m_riverbankUniforms.visibility_size !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_waterPipeline->m_riverbankUniforms.visibility_size,
                m_riverbankVisibility.size);
          }
          if (m_waterPipeline->m_riverbankUniforms.visibility_tile_size !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_waterPipeline->m_riverbankUniforms.visibility_tile_size,
                m_riverbankVisibility.tile_size);
          }
          if (m_waterPipeline->m_riverbankUniforms.explored_alpha !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_waterPipeline->m_riverbankUniforms.explored_alpha,
                m_riverbankVisibility.explored_alpha);
          }
          constexpr int k_riverbank_vis_texture_unit = 7;
          m_riverbankVisibility.texture->bind(k_riverbank_vis_texture_unit);
          if (m_waterPipeline->m_riverbankUniforms.visibility_texture !=
              Shader::InvalidUniform) {
            active_shader->set_uniform(
                m_waterPipeline->m_riverbankUniforms.visibility_texture,
                k_riverbank_vis_texture_unit);
          }
          m_lastBoundTexture = m_riverbankVisibility.texture;
        }

        it.mesh->draw();
        break;
      }

      if (active_shader == m_waterPipeline->m_bridgeShader) {
        if (m_lastBoundShader != active_shader) {
          active_shader->use();
          m_lastBoundShader = active_shader;
        }

        active_shader->set_uniform(m_waterPipeline->m_bridgeUniforms.mvp,
                                   it.mvp);
        active_shader->set_uniform(m_waterPipeline->m_bridgeUniforms.model,
                                   it.model);
        active_shader->set_uniform(m_waterPipeline->m_bridgeUniforms.color,
                                   it.color);

        QVector3D const light_dir(0.35F, 0.8F, 0.45F);
        active_shader->set_uniform(
            m_waterPipeline->m_bridgeUniforms.light_direction, light_dir);

        it.mesh->draw();
        break;
      }

      if (active_shader == m_waterPipeline->m_road_shader) {
        if (m_lastBoundShader != active_shader) {
          active_shader->use();
          m_lastBoundShader = active_shader;
        }

        active_shader->set_uniform(m_waterPipeline->m_road_uniforms.mvp,
                                   it.mvp);
        active_shader->set_uniform(m_waterPipeline->m_road_uniforms.model,
                                   it.model);
        active_shader->set_uniform(m_waterPipeline->m_road_uniforms.color,
                                   it.color);
        active_shader->set_uniform(m_waterPipeline->m_road_uniforms.alpha,
                                   it.alpha);

        QVector3D const road_light_dir(0.35F, 0.8F, 0.45F);
        active_shader->set_uniform(
            m_waterPipeline->m_road_uniforms.light_direction, road_light_dir);

        it.mesh->draw();
        break;
      }

      if (m_bannerPipeline != nullptr &&
          active_shader == m_bannerPipeline->m_bannerShader) {
        if (m_lastBoundShader != active_shader) {
          active_shader->use();
          m_lastBoundShader = active_shader;
        }

        QMatrix4x4 mvp =
            cam.get_projection_matrix() * cam.get_view_matrix() * it.model;
        active_shader->set_uniform(m_bannerPipeline->m_bannerUniforms.mvp, mvp);
        active_shader->set_uniform(m_bannerPipeline->m_bannerUniforms.model,
                                   it.model);
        active_shader->set_uniform(m_bannerPipeline->m_bannerUniforms.time,
                                   m_animationTime);

        float windStrength = 0.8F + 0.2F * std::sin(m_animationTime * 0.5F);
        active_shader->set_uniform(
            m_bannerPipeline->m_bannerUniforms.windStrength, windStrength);
        active_shader->set_uniform(m_bannerPipeline->m_bannerUniforms.color,
                                   it.color);

        QVector3D trimColor = it.color * 0.7F;
        active_shader->set_uniform(m_bannerPipeline->m_bannerUniforms.trimColor,
                                   trimColor);
        active_shader->set_uniform(m_bannerPipeline->m_bannerUniforms.alpha,
                                   it.alpha);
        active_shader->set_uniform(
            m_bannerPipeline->m_bannerUniforms.useTexture,
            it.texture != nullptr);

        Texture *tex_to_use =
            (it.texture != nullptr)
                ? it.texture
                : (m_resources ? m_resources->white() : nullptr);
        if ((tex_to_use != nullptr) && tex_to_use != m_lastBoundTexture) {
          tex_to_use->bind(0);
          m_lastBoundTexture = tex_to_use;
          active_shader->set_uniform(m_bannerPipeline->m_bannerUniforms.texture,
                                     0);
        }

        it.mesh->draw();
        break;
      }

      // Check if this mesh command can be batched using instanced rendering
      // Batch opaque mesh commands with the same mesh/shader/texture
      if (!isTransparent && !isShadowShader && m_meshInstancingPipeline &&
          m_meshInstancingPipeline->is_initialized() &&
          m_meshInstancingPipeline->instanced_shader() != nullptr) {

        // Check if we can batch with upcoming commands
        bool start_new_batch = !m_meshInstancingPipeline->has_pending() ||
                               !m_meshInstancingPipeline->can_batch(
                                   it.mesh, it.shader, it.texture);

        if (start_new_batch && m_meshInstancingPipeline->has_pending()) {
          // Flush the previous batch before starting a new one
          m_meshInstancingPipeline->flush(view_proj);
          m_lastBoundShader = nullptr;
          m_lastBoundTexture = nullptr;
        }

        if (start_new_batch) {
          m_meshInstancingPipeline->begin_batch(it.mesh, it.shader, it.texture);
        }

        // Add this instance to the batch
        m_meshInstancingPipeline->accumulate(it.model, it.color, it.alpha,
                                             it.material_id);

        // Look ahead: if next command is not batchable, flush now
        bool should_flush = true;
        if (i + 1 < count) {
          const auto &next_cmd = queue.get_sorted(i + 1);
          if (next_cmd.index() == MeshCmdIndex) {
            const auto &next_mesh = std::get<MeshCmdIndex>(next_cmd);
            if (can_batch_mesh_cmds(it, next_mesh)) {
              should_flush = false;
            }
          }
        }

        if (should_flush && m_meshInstancingPipeline->has_pending()) {
          m_meshInstancingPipeline->flush(view_proj);
          m_lastBoundShader = nullptr;
          m_lastBoundTexture = nullptr;
        }

        break;
      }

      auto *uniforms = m_characterPipeline
                           ? m_characterPipeline->resolveUniforms(active_shader)
                           : nullptr;
      if (uniforms == nullptr) {
        break;
      }

      if (m_lastBoundShader != active_shader) {
        active_shader->use();
        m_lastBoundShader = active_shader;
      }

      active_shader->set_uniform(uniforms->mvp, it.mvp);
      active_shader->set_uniform(uniforms->model, it.model);

      Texture *tex_to_use =
          (it.texture != nullptr)
              ? it.texture
              : (m_resources ? m_resources->white() : nullptr);
      if ((tex_to_use != nullptr) && tex_to_use != m_lastBoundTexture) {
        tex_to_use->bind(0);
        m_lastBoundTexture = tex_to_use;
        active_shader->set_uniform(uniforms->texture, 0);
      }

      active_shader->set_uniform(uniforms->useTexture, it.texture != nullptr);
      active_shader->set_uniform(uniforms->color, it.color);
      active_shader->set_uniform(uniforms->alpha, it.alpha);
      active_shader->set_uniform(uniforms->materialId, it.material_id);
      it.mesh->draw();

      if (isTransparent) {
        glDepthFunc(static_cast<GLenum>(prev_depth_func));
      }
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

      m_effectsPipeline->m_gridShader->set_uniform(
          m_effectsPipeline->m_gridUniforms.mvp, gc.mvp);
      m_effectsPipeline->m_gridShader->set_uniform(
          m_effectsPipeline->m_gridUniforms.model, gc.model);
      m_effectsPipeline->m_gridShader->set_uniform(
          m_effectsPipeline->m_gridUniforms.gridColor, gc.color);
      m_effectsPipeline->m_gridShader->set_uniform(
          m_effectsPipeline->m_gridUniforms.lineColor, k_grid_line_color);
      m_effectsPipeline->m_gridShader->set_uniform(
          m_effectsPipeline->m_gridUniforms.cellSize, gc.cell_size);
      m_effectsPipeline->m_gridShader->set_uniform(
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
      m_effectsPipeline->m_basicShader->set_uniform(
          m_effectsPipeline->m_basicUniforms.useTexture, false);
      m_effectsPipeline->m_basicShader->set_uniform(
          m_effectsPipeline->m_basicUniforms.color, sc.color);

      DepthMaskScope const depth_mask(false);
      PolygonOffsetScope const poly(-1.0F, -1.0F);
      BlendScope const blend(true);

      {
        QMatrix4x4 m = sc.model;
        m.scale(1.08F, 1.0F, 1.08F);
        const QMatrix4x4 mvp = view_proj * m;
        m_effectsPipeline->m_basicShader->set_uniform(
            m_effectsPipeline->m_basicUniforms.mvp, mvp);
        m_effectsPipeline->m_basicShader->set_uniform(
            m_effectsPipeline->m_basicUniforms.model, m);
        m_effectsPipeline->m_basicShader->set_uniform(
            m_effectsPipeline->m_basicUniforms.alpha, sc.alpha_outer);
        ring->draw();
      }

      {
        const QMatrix4x4 mvp = view_proj * sc.model;
        m_effectsPipeline->m_basicShader->set_uniform(
            m_effectsPipeline->m_basicUniforms.mvp, mvp);
        m_effectsPipeline->m_basicShader->set_uniform(
            m_effectsPipeline->m_basicUniforms.model, sc.model);
        m_effectsPipeline->m_basicShader->set_uniform(
            m_effectsPipeline->m_basicUniforms.alpha, sc.alpha_inner);
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
      m_effectsPipeline->m_basicShader->set_uniform(
          m_effectsPipeline->m_basicUniforms.useTexture, false);
      m_effectsPipeline->m_basicShader->set_uniform(
          m_effectsPipeline->m_basicUniforms.color, sm.color);
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
        m_effectsPipeline->m_basicShader->set_uniform(
            m_effectsPipeline->m_basicUniforms.mvp, mvp);
        m_effectsPipeline->m_basicShader->set_uniform(
            m_effectsPipeline->m_basicUniforms.model, m);
        m_effectsPipeline->m_basicShader->set_uniform(
            m_effectsPipeline->m_basicUniforms.alpha, a);
        disc->draw();
      }
      break;
    }
    case PrimitiveBatchCmdIndex: {
      const auto &batch = std::get<PrimitiveBatchCmdIndex>(cmd);
      if (batch.instance_count() == 0 || m_primitiveBatchPipeline == nullptr ||
          !m_primitiveBatchPipeline->is_initialized()) {
        break;
      }

      const auto *data = batch.instance_data();

      switch (batch.type) {
      case PrimitiveType::Sphere:
        m_primitiveBatchPipeline->upload_sphere_instances(
            data, batch.instance_count());
        m_primitiveBatchPipeline->draw_spheres(batch.instance_count(),
                                               view_proj);
        break;
      case PrimitiveType::Cylinder:
        m_primitiveBatchPipeline->upload_cylinder_instances(
            data, batch.instance_count());
        m_primitiveBatchPipeline->draw_cylinders(batch.instance_count(),
                                                 view_proj);
        break;
      case PrimitiveType::Cone:
        m_primitiveBatchPipeline->upload_cone_instances(data,
                                                        batch.instance_count());
        m_primitiveBatchPipeline->draw_cones(batch.instance_count(), view_proj);
        break;
      }

      m_lastBoundShader = m_primitiveBatchPipeline->shader();
      break;
    }
    case HealingBeamCmdIndex: {
      const auto &beam = std::get<HealingBeamCmdIndex>(cmd);
      if (m_healingBeamPipeline == nullptr ||
          !m_healingBeamPipeline->is_initialized()) {
        break;
      }
      m_healingBeamPipeline->render_single_beam(
          beam.start_pos, beam.end_pos, beam.color, beam.progress,
          beam.beam_width, beam.intensity, beam.time, view_proj);
      m_lastBoundShader = nullptr;
      break;
    }
    case HealerAuraCmdIndex: {
      const auto &aura = std::get<HealerAuraCmdIndex>(cmd);
      if (m_healerAuraPipeline == nullptr ||
          !m_healerAuraPipeline->is_initialized()) {
        break;
      }
      m_healerAuraPipeline->render_single_aura(aura.position, aura.color,
                                               aura.radius, aura.intensity,
                                               aura.time, view_proj);
      m_lastBoundShader = nullptr;
      break;
    }
    case CombatDustCmdIndex: {
      const auto &dust = std::get<CombatDustCmdIndex>(cmd);
      if (m_combatDustPipeline == nullptr ||
          !m_combatDustPipeline->is_initialized()) {
        break;
      }
      m_combatDustPipeline->render_single_dust(dust.position, dust.color,
                                               dust.radius, dust.intensity,
                                               dust.time, view_proj);
      m_lastBoundShader = nullptr;
      break;
    }
    case BuildingFlameCmdIndex: {
      const auto &flame = std::get<BuildingFlameCmdIndex>(cmd);
      if (m_combatDustPipeline == nullptr ||
          !m_combatDustPipeline->is_initialized()) {
        break;
      }
      m_combatDustPipeline->render_single_flame(flame.position, flame.color,
                                                flame.radius, flame.intensity,
                                                flame.time, view_proj);
      m_lastBoundShader = nullptr;
      break;
    }
    case StoneImpactCmdIndex: {
      const auto &impact = std::get<StoneImpactCmdIndex>(cmd);
      if (m_combatDustPipeline == nullptr ||
          !m_combatDustPipeline->is_initialized()) {
        break;
      }
      m_combatDustPipeline->render_single_stone_impact(
          impact.position, impact.color, impact.radius, impact.intensity,
          impact.time, view_proj);
      m_lastBoundShader = nullptr;
      break;
    }
    case ModeIndicatorCmdIndex: {
      const auto &mc = std::get<ModeIndicatorCmdIndex>(cmd);

      if (m_modeIndicatorPipeline == nullptr ||
          !m_modeIndicatorPipeline->is_initialized()) {
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

      m_modeIndicatorPipeline->render_indicator(indicator_mesh, mc.model,
                                                view_proj, mc.color, mc.alpha,
                                                m_animationTime);

      m_lastBoundShader = nullptr;
      break;
    }
    default:
      break;
    }
    ++i;
  }

  // Flush any pending mesh instances at the end of the frame
  if (m_meshInstancingPipeline && m_meshInstancingPipeline->has_pending()) {
    m_meshInstancingPipeline->flush(view_proj);
  }

  if (m_lastBoundShader != nullptr) {
    m_lastBoundShader->release();
    m_lastBoundShader = nullptr;
  }
}

} // namespace Render::GL
