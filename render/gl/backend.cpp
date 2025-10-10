#include "backend.h"
#include "../draw_queue.h"
#include "../geom/selection_disc.h"
#include "../geom/selection_ring.h"
#include "buffer.h"
#include "mesh.h"
#include "primitives.h"
#include "shader.h"
#include "state_scopes.h"
#include "texture.h"
#include <QDebug>
#include <algorithm>
#include <cstddef>
#include <memory>

namespace Render::GL {

namespace {

const QVector3D kGridLineColor(0.22f, 0.25f, 0.22f);
}

Backend::~Backend() {
  shutdownCylinderPipeline();
  shutdownFogPipeline();
  shutdownGrassPipeline();
  shutdownStonePipeline();
}

void Backend::initialize() {
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
  m_basicShader = m_shaderCache->get(QStringLiteral("basic"));
  m_gridShader = m_shaderCache->get(QStringLiteral("grid"));
  m_cylinderShader = m_shaderCache->get(QStringLiteral("cylinder_instanced"));
  m_fogShader = m_shaderCache->get(QStringLiteral("fog_instanced"));
  m_grassShader = m_shaderCache->get(QStringLiteral("grass_instanced"));
  m_stoneShader = m_shaderCache->get(QStringLiteral("stone_instanced"));
  m_groundShader = m_shaderCache->get(QStringLiteral("ground_plane"));
  m_terrainShader = m_shaderCache->get(QStringLiteral("terrain_chunk"));
  if (!m_basicShader)
    qWarning() << "Backend: basic shader missing";
  if (!m_gridShader)
    qWarning() << "Backend: grid shader missing";
  if (!m_cylinderShader)
    qWarning() << "Backend: cylinder shader missing";
  if (!m_fogShader)
    qWarning() << "Backend: fog shader missing";
  if (!m_grassShader)
    qWarning() << "Backend: grass shader missing";
  if (!m_stoneShader)
    qWarning() << "Backend: stone shader missing";
  if (!m_groundShader)
    qWarning() << "Backend: ground_plane shader missing";
  if (!m_terrainShader)
    qWarning() << "Backend: terrain shader missing";

  cacheBasicUniforms();
  cacheGridUniforms();
  cacheCylinderUniforms();
  cacheFogUniforms();
  cacheGrassUniforms();
  cacheStoneUniforms();
  cacheGroundUniforms();
  cacheTerrainUniforms();
  initializeCylinderPipeline();
  initializeFogPipeline();
  initializeGrassPipeline();
  initializeStonePipeline();
}

void Backend::beginFrame() {
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
  if (!m_basicShader)
    return;

  const QMatrix4x4 viewProj = cam.getProjectionMatrix() * cam.getViewMatrix();

  m_lastBoundShader = nullptr;
  m_lastBoundTexture = nullptr;

  const std::size_t count = queue.size();
  std::size_t i = 0;
  while (i < count) {
    const auto &cmd = queue.getSorted(i);
    switch (cmd.index()) {
    case CylinderCmdIndex: {
      m_cylinderScratch.clear();
      do {
        const auto &cy = std::get<CylinderCmdIndex>(queue.getSorted(i));
        CylinderInstanceGpu gpu{};
        gpu.start = cy.start;
        gpu.end = cy.end;
        gpu.radius = cy.radius;
        gpu.alpha = cy.alpha;
        gpu.color = cy.color;
        m_cylinderScratch.emplace_back(gpu);
        ++i;
      } while (i < count && queue.getSorted(i).index() == CylinderCmdIndex);

      const std::size_t instanceCount = m_cylinderScratch.size();
      if (instanceCount > 0 && m_cylinderShader && m_cylinderVao) {
        glDepthMask(GL_TRUE);
        if (glIsEnabled(GL_POLYGON_OFFSET_FILL))
          glDisable(GL_POLYGON_OFFSET_FILL);
        if (m_lastBoundShader != m_cylinderShader) {
          m_cylinderShader->use();
          m_lastBoundShader = m_cylinderShader;
          m_lastBoundTexture = nullptr;
        }
        if (m_cylinderUniforms.viewProj != Shader::InvalidUniform) {
          m_cylinderShader->setUniform(m_cylinderUniforms.viewProj, viewProj);
        }
        uploadCylinderInstances(instanceCount);
        drawCylinders(instanceCount);
      }
      continue;
    }
    case FogBatchCmdIndex: {
      const auto &batch = std::get<FogBatchCmdIndex>(cmd);
      const FogInstanceData *instances = batch.instances;
      const std::size_t instanceCount = batch.count;
      if (instances && instanceCount > 0 && m_fogShader && m_fogVao) {
        m_fogScratch.resize(instanceCount);
        for (std::size_t idx = 0; idx < instanceCount; ++idx) {
          FogInstanceGpu gpu{};
          gpu.center = instances[idx].center;
          gpu.size = instances[idx].size;
          gpu.color = instances[idx].color;
          gpu.alpha = instances[idx].alpha;
          m_fogScratch[idx] = gpu;
        }
        glDepthMask(GL_TRUE);
        if (glIsEnabled(GL_POLYGON_OFFSET_FILL))
          glDisable(GL_POLYGON_OFFSET_FILL);
        if (m_lastBoundShader != m_fogShader) {
          m_fogShader->use();
          m_lastBoundShader = m_fogShader;
          m_lastBoundTexture = nullptr;
        }
        if (m_fogUniforms.viewProj != Shader::InvalidUniform) {
          m_fogShader->setUniform(m_fogUniforms.viewProj, viewProj);
        }
        uploadFogInstances(instanceCount);
        drawFog(instanceCount);
      }
      ++i;
      continue;
    }
    case GrassBatchCmdIndex: {
      const auto &grass = std::get<GrassBatchCmdIndex>(cmd);
      if (!grass.instanceBuffer || grass.instanceCount == 0 || !m_grassShader ||
          !m_grassVao || m_grassVertexCount == 0)
        break;

      DepthMaskScope depthMask(false);
      BlendScope blend(true);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      GLboolean prevCull = glIsEnabled(GL_CULL_FACE);
      if (prevCull)
        glDisable(GL_CULL_FACE);

      if (m_lastBoundShader != m_grassShader) {
        m_grassShader->use();
        m_lastBoundShader = m_grassShader;
        m_lastBoundTexture = nullptr;
      }

      if (m_grassUniforms.viewProj != Shader::InvalidUniform) {
        m_grassShader->setUniform(m_grassUniforms.viewProj, viewProj);
      }
      if (m_grassUniforms.time != Shader::InvalidUniform) {
        m_grassShader->setUniform(m_grassUniforms.time, grass.params.time);
      }
      if (m_grassUniforms.windStrength != Shader::InvalidUniform) {
        m_grassShader->setUniform(m_grassUniforms.windStrength,
                                  grass.params.windStrength);
      }
      if (m_grassUniforms.windSpeed != Shader::InvalidUniform) {
        m_grassShader->setUniform(m_grassUniforms.windSpeed,
                                  grass.params.windSpeed);
      }
      if (m_grassUniforms.soilColor != Shader::InvalidUniform) {
        m_grassShader->setUniform(m_grassUniforms.soilColor,
                                  grass.params.soilColor);
      }
      if (m_grassUniforms.lightDir != Shader::InvalidUniform) {
        QVector3D lightDir = grass.params.lightDirection;
        if (!lightDir.isNull())
          lightDir.normalize();
        m_grassShader->setUniform(m_grassUniforms.lightDir, lightDir);
      }

      glBindVertexArray(m_grassVao);
      grass.instanceBuffer->bind();
      const GLsizei stride = static_cast<GLsizei>(sizeof(GrassInstanceGpu));
      glVertexAttribPointer(
          2, 4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(GrassInstanceGpu, posHeight)));
      glVertexAttribPointer(
          3, 4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(GrassInstanceGpu, colorWidth)));
      glVertexAttribPointer(
          4, 4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(GrassInstanceGpu, swayParams)));
      grass.instanceBuffer->unbind();

      glDrawArraysInstanced(GL_TRIANGLES, 0, m_grassVertexCount,
                            static_cast<GLsizei>(grass.instanceCount));
      glBindVertexArray(0);

      if (prevCull)
        glEnable(GL_CULL_FACE);

      break;
    }
    case StoneBatchCmdIndex: {
      const auto &stone = std::get<StoneBatchCmdIndex>(cmd);
      if (!stone.instanceBuffer || stone.instanceCount == 0 || !m_stoneShader ||
          !m_stoneVao || m_stoneIndexCount == 0)
        break;

      DepthMaskScope depthMask(true);
      BlendScope blend(false);

      if (m_lastBoundShader != m_stoneShader) {
        m_stoneShader->use();
        m_lastBoundShader = m_stoneShader;
        m_lastBoundTexture = nullptr;
      }

      if (m_stoneUniforms.viewProj != Shader::InvalidUniform) {
        m_stoneShader->setUniform(m_stoneUniforms.viewProj, viewProj);
      }
      if (m_stoneUniforms.lightDirection != Shader::InvalidUniform) {
        QVector3D lightDir = stone.params.lightDirection;
        if (!lightDir.isNull())
          lightDir.normalize();
        m_stoneShader->setUniform(m_stoneUniforms.lightDirection, lightDir);
      }

      glBindVertexArray(m_stoneVao);
      stone.instanceBuffer->bind();
      const GLsizei stride = static_cast<GLsizei>(sizeof(StoneInstanceGpu));
      glVertexAttribPointer(
          2, 4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(StoneInstanceGpu, posScale)));
      glVertexAttribPointer(
          3, 4, GL_FLOAT, GL_FALSE, stride,
          reinterpret_cast<void *>(offsetof(StoneInstanceGpu, colorRot)));
      stone.instanceBuffer->unbind();

      glDrawElementsInstanced(GL_TRIANGLES, m_stoneIndexCount,
                              GL_UNSIGNED_SHORT, nullptr,
                              static_cast<GLsizei>(stone.instanceCount));
      glBindVertexArray(0);

      break;
    }
    case TerrainChunkCmdIndex: {
      const auto &terrain = std::get<TerrainChunkCmdIndex>(cmd);
      
      // Choose shader based on whether this is ground plane or elevated terrain
      Shader *activeShader = terrain.params.isGroundPlane ? m_groundShader : m_terrainShader;
      
      if (!terrain.mesh || !activeShader)
        break;

      if (m_lastBoundShader != activeShader) {
        activeShader->use();
        m_lastBoundShader = activeShader;
        m_lastBoundTexture = nullptr;
      }

      const QMatrix4x4 mvp = viewProj * terrain.model;
      
      if (terrain.params.isGroundPlane) {
        // Use ground uniforms for flat base plane
        if (m_groundUniforms.mvp != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.mvp, mvp);
        if (m_groundUniforms.model != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.model, terrain.model);
        if (m_groundUniforms.grassPrimary != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.grassPrimary,
                                      terrain.params.grassPrimary);
        if (m_groundUniforms.grassSecondary != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.grassSecondary,
                                      terrain.params.grassSecondary);
        if (m_groundUniforms.grassDry != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.grassDry,
                                      terrain.params.grassDry);
        if (m_groundUniforms.soilColor != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.soilColor,
                                      terrain.params.soilColor);
        if (m_groundUniforms.tint != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.tint,
                                      terrain.params.tint);
        if (m_groundUniforms.noiseOffset != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.noiseOffset,
                                      terrain.params.noiseOffset);
        if (m_groundUniforms.tileSize != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.tileSize,
                                      terrain.params.tileSize);
        if (m_groundUniforms.macroNoiseScale != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.macroNoiseScale,
                                      terrain.params.macroNoiseScale);
        if (m_groundUniforms.detailNoiseScale != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.detailNoiseScale,
                                      terrain.params.detailNoiseScale);
        if (m_groundUniforms.soilBlendHeight != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.soilBlendHeight,
                                      terrain.params.soilBlendHeight);
        if (m_groundUniforms.soilBlendSharpness != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.soilBlendSharpness,
                                      terrain.params.soilBlendSharpness);
        if (m_groundUniforms.ambientBoost != Shader::InvalidUniform)
          activeShader->setUniform(m_groundUniforms.ambientBoost,
                                      terrain.params.ambientBoost);
        if (m_groundUniforms.lightDir != Shader::InvalidUniform) {
          QVector3D lightDir = terrain.params.lightDirection;
          if (!lightDir.isNull())
            lightDir.normalize();
          activeShader->setUniform(m_groundUniforms.lightDir, lightDir);
        }
      } else {
        // Use terrain uniforms for elevated hills/mountains
        if (m_terrainUniforms.mvp != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.mvp, mvp);
        if (m_terrainUniforms.model != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.model, terrain.model);
        if (m_terrainUniforms.grassPrimary != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.grassPrimary,
                                      terrain.params.grassPrimary);
        if (m_terrainUniforms.grassSecondary != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.grassSecondary,
                                      terrain.params.grassSecondary);
        if (m_terrainUniforms.grassDry != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.grassDry,
                                      terrain.params.grassDry);
        if (m_terrainUniforms.soilColor != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.soilColor,
                                      terrain.params.soilColor);
        if (m_terrainUniforms.rockLow != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.rockLow,
                                      terrain.params.rockLow);
        if (m_terrainUniforms.rockHigh != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.rockHigh,
                                      terrain.params.rockHigh);
        if (m_terrainUniforms.tint != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.tint,
                                      terrain.params.tint);
        if (m_terrainUniforms.noiseOffset != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.noiseOffset,
                                      terrain.params.noiseOffset);
        if (m_terrainUniforms.tileSize != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.tileSize,
                                      terrain.params.tileSize);
        if (m_terrainUniforms.macroNoiseScale != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.macroNoiseScale,
                                      terrain.params.macroNoiseScale);
        if (m_terrainUniforms.detailNoiseScale != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.detailNoiseScale,
                                      terrain.params.detailNoiseScale);
        if (m_terrainUniforms.slopeRockThreshold != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.slopeRockThreshold,
                                      terrain.params.slopeRockThreshold);
        if (m_terrainUniforms.slopeRockSharpness != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.slopeRockSharpness,
                                      terrain.params.slopeRockSharpness);
        if (m_terrainUniforms.soilBlendHeight != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.soilBlendHeight,
                                      terrain.params.soilBlendHeight);
        if (m_terrainUniforms.soilBlendSharpness != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.soilBlendSharpness,
                                      terrain.params.soilBlendSharpness);
        if (m_terrainUniforms.heightNoiseStrength != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.heightNoiseStrength,
                                      terrain.params.heightNoiseStrength);
        if (m_terrainUniforms.heightNoiseFrequency != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.heightNoiseFrequency,
                                      terrain.params.heightNoiseFrequency);
        if (m_terrainUniforms.ambientBoost != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.ambientBoost,
                                      terrain.params.ambientBoost);
        if (m_terrainUniforms.rockDetailStrength != Shader::InvalidUniform)
          activeShader->setUniform(m_terrainUniforms.rockDetailStrength,
                                      terrain.params.rockDetailStrength);
        if (m_terrainUniforms.lightDir != Shader::InvalidUniform) {
          QVector3D lightDir = terrain.params.lightDirection;
          if (!lightDir.isNull())
            lightDir.normalize();
          activeShader->setUniform(m_terrainUniforms.lightDir, lightDir);
        }
      }

      DepthMaskScope depthMask(terrain.depthWrite);
      std::unique_ptr<PolygonOffsetScope> polyScope;
      if (terrain.depthBias != 0.0f)
        polyScope = std::make_unique<PolygonOffsetScope>(terrain.depthBias,
                                                         terrain.depthBias);

      terrain.mesh->draw();
      break;
    }
    case MeshCmdIndex: {
      const auto &it = std::get<MeshCmdIndex>(cmd);
      if (!it.mesh)
        break;

      glDepthMask(GL_TRUE);
      if (glIsEnabled(GL_POLYGON_OFFSET_FILL))
        glDisable(GL_POLYGON_OFFSET_FILL);

      if (m_lastBoundShader != m_basicShader) {
        m_basicShader->use();
        m_lastBoundShader = m_basicShader;
      }

      m_basicShader->setUniform(m_basicUniforms.mvp, it.mvp);
      m_basicShader->setUniform(m_basicUniforms.model, it.model);

      Texture *texToUse = it.texture
                              ? it.texture
                              : (m_resources ? m_resources->white() : nullptr);
      if (texToUse && texToUse != m_lastBoundTexture) {
        texToUse->bind(0);
        m_lastBoundTexture = texToUse;
        m_basicShader->setUniform(m_basicUniforms.texture, 0);
      }

      m_basicShader->setUniform(m_basicUniforms.useTexture,
                                it.texture != nullptr);
      m_basicShader->setUniform(m_basicUniforms.color, it.color);
      m_basicShader->setUniform(m_basicUniforms.alpha, it.alpha);
      it.mesh->draw();
      break;
    }
    case GridCmdIndex: {
      if (!m_gridShader)
        break;
      const auto &gc = std::get<GridCmdIndex>(cmd);

      if (m_lastBoundShader != m_gridShader) {
        m_gridShader->use();
        m_lastBoundShader = m_gridShader;
      }

      m_gridShader->setUniform(m_gridUniforms.mvp, gc.mvp);
      m_gridShader->setUniform(m_gridUniforms.model, gc.model);
      m_gridShader->setUniform(m_gridUniforms.gridColor, gc.color);
      m_gridShader->setUniform(m_gridUniforms.lineColor, kGridLineColor);
      m_gridShader->setUniform(m_gridUniforms.cellSize, gc.cellSize);
      m_gridShader->setUniform(m_gridUniforms.thickness, gc.thickness);

      if (m_resources) {
        if (auto *plane = m_resources->ground())
          plane->draw();
      }
      break;
    }
    case SelectionRingCmdIndex: {
      const auto &sc = std::get<SelectionRingCmdIndex>(cmd);
      Mesh *ring = Render::Geom::SelectionRing::get();
      if (!ring)
        break;

      if (m_lastBoundShader != m_basicShader) {
        m_basicShader->use();
        m_lastBoundShader = m_basicShader;
      }

      m_basicShader->use();
      m_basicShader->setUniform(m_basicUniforms.useTexture, false);
      m_basicShader->setUniform(m_basicUniforms.color, sc.color);

      DepthMaskScope depthMask(false);
      DepthTestScope depthTest(false);
      PolygonOffsetScope poly(-1.0f, -1.0f);
      BlendScope blend(true);

      {
        QMatrix4x4 m = sc.model;
        m.scale(1.08f, 1.0f, 1.08f);
        const QMatrix4x4 mvp = viewProj * m;
        m_basicShader->setUniform(m_basicUniforms.mvp, mvp);
        m_basicShader->setUniform(m_basicUniforms.model, m);
        m_basicShader->setUniform(m_basicUniforms.alpha, sc.alphaOuter);
        ring->draw();
      }

      {
        const QMatrix4x4 mvp = viewProj * sc.model;
        m_basicShader->setUniform(m_basicUniforms.mvp, mvp);
        m_basicShader->setUniform(m_basicUniforms.model, sc.model);
        m_basicShader->setUniform(m_basicUniforms.alpha, sc.alphaInner);
        ring->draw();
      }
      break;
    }
    case SelectionSmokeCmdIndex: {
      const auto &sm = std::get<SelectionSmokeCmdIndex>(cmd);
      Mesh *disc = Render::Geom::SelectionDisc::get();
      if (!disc)
        break;

      if (m_lastBoundShader != m_basicShader) {
        m_basicShader->use();
        m_lastBoundShader = m_basicShader;
      }
      m_basicShader->setUniform(m_basicUniforms.useTexture, false);
      m_basicShader->setUniform(m_basicUniforms.color, sm.color);
      DepthMaskScope depthMask(false);
      DepthTestScope depthTest(true);

      PolygonOffsetScope poly(-1.0f, -1.0f);
      BlendScope blend(true);
      for (int i = 0; i < 7; ++i) {
        float scale = 1.35f + 0.12f * i;
        float a = sm.baseAlpha * (1.0f - 0.09f * i);
        QMatrix4x4 m = sm.model;
        m.translate(0.0f, 0.02f, 0.0f);
        m.scale(scale, 1.0f, scale);
        const QMatrix4x4 mvp = viewProj * m;
        m_basicShader->setUniform(m_basicUniforms.mvp, mvp);
        m_basicShader->setUniform(m_basicUniforms.model, m);
        m_basicShader->setUniform(m_basicUniforms.alpha, a);
        disc->draw();
      }
      break;
    }
    default:
      break;
    }
    ++i;
  }
  if (m_lastBoundShader) {
    m_lastBoundShader->release();
    m_lastBoundShader = nullptr;
  }
}

void Backend::cacheBasicUniforms() {
  if (!m_basicShader)
    return;

  m_basicUniforms.mvp = m_basicShader->uniformHandle("u_mvp");
  m_basicUniforms.model = m_basicShader->uniformHandle("u_model");
  m_basicUniforms.texture = m_basicShader->uniformHandle("u_texture");
  m_basicUniforms.useTexture = m_basicShader->uniformHandle("u_useTexture");
  m_basicUniforms.color = m_basicShader->uniformHandle("u_color");
  m_basicUniforms.alpha = m_basicShader->uniformHandle("u_alpha");
}

void Backend::cacheGridUniforms() {
  if (!m_gridShader)
    return;

  m_gridUniforms.mvp = m_gridShader->uniformHandle("u_mvp");
  m_gridUniforms.model = m_gridShader->uniformHandle("u_model");
  m_gridUniforms.gridColor = m_gridShader->uniformHandle("u_gridColor");
  m_gridUniforms.lineColor = m_gridShader->uniformHandle("u_lineColor");
  m_gridUniforms.cellSize = m_gridShader->uniformHandle("u_cellSize");
  m_gridUniforms.thickness = m_gridShader->uniformHandle("u_thickness");
}

void Backend::cacheCylinderUniforms() {
  if (!m_cylinderShader)
    return;

  m_cylinderUniforms.viewProj = m_cylinderShader->uniformHandle("u_viewProj");
}

void Backend::cacheFogUniforms() {
  if (!m_fogShader)
    return;

  m_fogUniforms.viewProj = m_fogShader->uniformHandle("u_viewProj");
}

void Backend::initializeCylinderPipeline() {
  initializeOpenGLFunctions();
  shutdownCylinderPipeline();

  Mesh *unit = getUnitCylinder();
  if (!unit)
    return;

  const auto &vertices = unit->getVertices();
  const auto &indices = unit->getIndices();
  if (vertices.empty() || indices.empty())
    return;

  glGenVertexArrays(1, &m_cylinderVao);
  glBindVertexArray(m_cylinderVao);

  glGenBuffers(1, &m_cylinderVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_cylinderVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
               vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m_cylinderIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cylinderIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);
  m_cylinderIndexCount = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, texCoord)));

  glGenBuffers(1, &m_cylinderInstanceBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_cylinderInstanceBuffer);
  m_cylinderInstanceCapacity = 256;
  glBufferData(GL_ARRAY_BUFFER,
               m_cylinderInstanceCapacity * sizeof(CylinderInstanceGpu),
               nullptr, GL_DYNAMIC_DRAW);

  const GLsizei stride = static_cast<GLsizei>(sizeof(CylinderInstanceGpu));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(
      3, 3, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(CylinderInstanceGpu, start)));
  glVertexAttribDivisor(3, 1);

  glEnableVertexAttribArray(4);
  glVertexAttribPointer(
      4, 3, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(CylinderInstanceGpu, end)));
  glVertexAttribDivisor(4, 1);

  glEnableVertexAttribArray(5);
  glVertexAttribPointer(
      5, 1, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(CylinderInstanceGpu, radius)));
  glVertexAttribDivisor(5, 1);

  glEnableVertexAttribArray(6);
  glVertexAttribPointer(
      6, 1, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(CylinderInstanceGpu, alpha)));
  glVertexAttribDivisor(6, 1);

  glEnableVertexAttribArray(7);
  glVertexAttribPointer(
      7, 3, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(CylinderInstanceGpu, color)));
  glVertexAttribDivisor(7, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  m_cylinderScratch.reserve(m_cylinderInstanceCapacity);
}

void Backend::shutdownCylinderPipeline() {
  initializeOpenGLFunctions();
  if (m_cylinderInstanceBuffer) {
    glDeleteBuffers(1, &m_cylinderInstanceBuffer);
    m_cylinderInstanceBuffer = 0;
  }
  if (m_cylinderVertexBuffer) {
    glDeleteBuffers(1, &m_cylinderVertexBuffer);
    m_cylinderVertexBuffer = 0;
  }
  if (m_cylinderIndexBuffer) {
    glDeleteBuffers(1, &m_cylinderIndexBuffer);
    m_cylinderIndexBuffer = 0;
  }
  if (m_cylinderVao) {
    glDeleteVertexArrays(1, &m_cylinderVao);
    m_cylinderVao = 0;
  }
  m_cylinderIndexCount = 0;
  m_cylinderInstanceCapacity = 0;
  m_cylinderScratch.clear();
}

void Backend::uploadCylinderInstances(std::size_t count) {
  if (!m_cylinderInstanceBuffer || count == 0)
    return;

  initializeOpenGLFunctions();
  glBindBuffer(GL_ARRAY_BUFFER, m_cylinderInstanceBuffer);
  if (count > m_cylinderInstanceCapacity) {
    m_cylinderInstanceCapacity = std::max<std::size_t>(
        count,
        m_cylinderInstanceCapacity ? m_cylinderInstanceCapacity * 2 : count);
    glBufferData(GL_ARRAY_BUFFER,
                 m_cylinderInstanceCapacity * sizeof(CylinderInstanceGpu),
                 nullptr, GL_DYNAMIC_DRAW);
    m_cylinderScratch.reserve(m_cylinderInstanceCapacity);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(CylinderInstanceGpu),
                  m_cylinderScratch.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Backend::drawCylinders(std::size_t count) {
  if (!m_cylinderVao || m_cylinderIndexCount == 0 || count == 0)
    return;

  initializeOpenGLFunctions();
  glBindVertexArray(m_cylinderVao);
  glDrawElementsInstanced(GL_TRIANGLES, m_cylinderIndexCount, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

void Backend::initializeFogPipeline() {
  initializeOpenGLFunctions();
  shutdownFogPipeline();

  const Vertex vertices[4] = {
      {{-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
      {{0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
      {{-0.5f, 0.0f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
      {{0.5f, 0.0f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
  };

  const unsigned int indices[6] = {0, 1, 2, 2, 1, 3};

  glGenVertexArrays(1, &m_fogVao);
  glBindVertexArray(m_fogVao);

  glGenBuffers(1, &m_fogVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_fogVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glGenBuffers(1, &m_fogIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fogIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);
  m_fogIndexCount = 6;

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, texCoord)));

  glGenBuffers(1, &m_fogInstanceBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_fogInstanceBuffer);
  m_fogInstanceCapacity = 512;
  glBufferData(GL_ARRAY_BUFFER, m_fogInstanceCapacity * sizeof(FogInstanceGpu),
               nullptr, GL_DYNAMIC_DRAW);

  const GLsizei stride = static_cast<GLsizei>(sizeof(FogInstanceGpu));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(
      3, 3, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(FogInstanceGpu, center)));
  glVertexAttribDivisor(3, 1);

  glEnableVertexAttribArray(4);
  glVertexAttribPointer(
      4, 1, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(FogInstanceGpu, size)));
  glVertexAttribDivisor(4, 1);

  glEnableVertexAttribArray(5);
  glVertexAttribPointer(
      5, 3, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(FogInstanceGpu, color)));
  glVertexAttribDivisor(5, 1);

  glEnableVertexAttribArray(6);
  glVertexAttribPointer(
      6, 1, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(FogInstanceGpu, alpha)));
  glVertexAttribDivisor(6, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  m_fogScratch.reserve(m_fogInstanceCapacity);
}

void Backend::shutdownFogPipeline() {
  initializeOpenGLFunctions();
  if (m_fogInstanceBuffer) {
    glDeleteBuffers(1, &m_fogInstanceBuffer);
    m_fogInstanceBuffer = 0;
  }
  if (m_fogVertexBuffer) {
    glDeleteBuffers(1, &m_fogVertexBuffer);
    m_fogVertexBuffer = 0;
  }
  if (m_fogIndexBuffer) {
    glDeleteBuffers(1, &m_fogIndexBuffer);
    m_fogIndexBuffer = 0;
  }
  if (m_fogVao) {
    glDeleteVertexArrays(1, &m_fogVao);
    m_fogVao = 0;
  }
  m_fogIndexCount = 0;
  m_fogInstanceCapacity = 0;
  m_fogScratch.clear();
}

void Backend::uploadFogInstances(std::size_t count) {
  if (!m_fogInstanceBuffer || count == 0)
    return;

  initializeOpenGLFunctions();
  glBindBuffer(GL_ARRAY_BUFFER, m_fogInstanceBuffer);
  if (count > m_fogInstanceCapacity) {
    m_fogInstanceCapacity = std::max<std::size_t>(
        count, m_fogInstanceCapacity ? m_fogInstanceCapacity * 2 : count);
    glBufferData(GL_ARRAY_BUFFER,
                 m_fogInstanceCapacity * sizeof(FogInstanceGpu), nullptr,
                 GL_DYNAMIC_DRAW);
    m_fogScratch.reserve(m_fogInstanceCapacity);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(FogInstanceGpu),
                  m_fogScratch.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Backend::drawFog(std::size_t count) {
  if (!m_fogVao || m_fogIndexCount == 0 || count == 0)
    return;

  initializeOpenGLFunctions();
  glBindVertexArray(m_fogVao);
  glDrawElementsInstanced(GL_TRIANGLES, m_fogIndexCount, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

void Backend::cacheGrassUniforms() {
  if (!m_grassShader)
    return;

  m_grassUniforms.viewProj = m_grassShader->uniformHandle("u_viewProj");
  m_grassUniforms.time = m_grassShader->uniformHandle("u_time");
  m_grassUniforms.windStrength = m_grassShader->uniformHandle("u_windStrength");
  m_grassUniforms.windSpeed = m_grassShader->uniformHandle("u_windSpeed");
  m_grassUniforms.soilColor = m_grassShader->uniformHandle("u_soilColor");
  m_grassUniforms.lightDir = m_grassShader->uniformHandle("u_lightDir");
}

void Backend::cacheGroundUniforms() {
  if (!m_groundShader)
    return;

  m_groundUniforms.mvp = m_groundShader->uniformHandle("u_mvp");
  m_groundUniforms.model = m_groundShader->uniformHandle("u_model");
  m_groundUniforms.grassPrimary =
      m_groundShader->uniformHandle("u_grassPrimary");
  m_groundUniforms.grassSecondary =
      m_groundShader->uniformHandle("u_grassSecondary");
  m_groundUniforms.grassDry = m_groundShader->uniformHandle("u_grassDry");
  m_groundUniforms.soilColor = m_groundShader->uniformHandle("u_soilColor");
  m_groundUniforms.tint = m_groundShader->uniformHandle("u_tint");
  m_groundUniforms.noiseOffset =
      m_groundShader->uniformHandle("u_noiseOffset");
  m_groundUniforms.tileSize = m_groundShader->uniformHandle("u_tileSize");
  m_groundUniforms.macroNoiseScale =
      m_groundShader->uniformHandle("u_macroNoiseScale");
  m_groundUniforms.detailNoiseScale =
      m_groundShader->uniformHandle("u_detailNoiseScale");
  m_groundUniforms.soilBlendHeight =
      m_groundShader->uniformHandle("u_soilBlendHeight");
  m_groundUniforms.soilBlendSharpness =
      m_groundShader->uniformHandle("u_soilBlendSharpness");
  m_groundUniforms.ambientBoost =
      m_groundShader->uniformHandle("u_ambientBoost");
  m_groundUniforms.lightDir = m_groundShader->uniformHandle("u_lightDir");
}

void Backend::cacheTerrainUniforms() {
  if (!m_terrainShader)
    return;

  m_terrainUniforms.mvp = m_terrainShader->uniformHandle("u_mvp");
  m_terrainUniforms.model = m_terrainShader->uniformHandle("u_model");
  m_terrainUniforms.grassPrimary =
      m_terrainShader->uniformHandle("u_grassPrimary");
  m_terrainUniforms.grassSecondary =
      m_terrainShader->uniformHandle("u_grassSecondary");
  m_terrainUniforms.grassDry = m_terrainShader->uniformHandle("u_grassDry");
  m_terrainUniforms.soilColor = m_terrainShader->uniformHandle("u_soilColor");
  m_terrainUniforms.rockLow = m_terrainShader->uniformHandle("u_rockLow");
  m_terrainUniforms.rockHigh = m_terrainShader->uniformHandle("u_rockHigh");
  m_terrainUniforms.tint = m_terrainShader->uniformHandle("u_tint");
  m_terrainUniforms.noiseOffset =
      m_terrainShader->uniformHandle("u_noiseOffset");
  m_terrainUniforms.tileSize = m_terrainShader->uniformHandle("u_tileSize");
  m_terrainUniforms.macroNoiseScale =
      m_terrainShader->uniformHandle("u_macroNoiseScale");
  m_terrainUniforms.detailNoiseScale =
      m_terrainShader->uniformHandle("u_detailNoiseScale");
  m_terrainUniforms.slopeRockThreshold =
      m_terrainShader->uniformHandle("u_slopeRockThreshold");
  m_terrainUniforms.slopeRockSharpness =
      m_terrainShader->uniformHandle("u_slopeRockSharpness");
  m_terrainUniforms.soilBlendHeight =
      m_terrainShader->uniformHandle("u_soilBlendHeight");
  m_terrainUniforms.soilBlendSharpness =
      m_terrainShader->uniformHandle("u_soilBlendSharpness");
  m_terrainUniforms.heightNoiseStrength =
      m_terrainShader->uniformHandle("u_heightNoiseStrength");
  m_terrainUniforms.heightNoiseFrequency =
      m_terrainShader->uniformHandle("u_heightNoiseFrequency");
  m_terrainUniforms.ambientBoost =
      m_terrainShader->uniformHandle("u_ambientBoost");
  m_terrainUniforms.rockDetailStrength =
      m_terrainShader->uniformHandle("u_rockDetailStrength");
  m_terrainUniforms.lightDir = m_terrainShader->uniformHandle("u_lightDir");
}

void Backend::initializeGrassPipeline() {
  initializeOpenGLFunctions();
  shutdownGrassPipeline();

  struct GrassVertex {
    QVector3D position;
    QVector2D uv;
  };

  const GrassVertex bladeVertices[6] = {
      {{-0.5f, 0.0f, 0.0f}, {0.0f, 0.0f}},
      {{0.5f, 0.0f, 0.0f}, {1.0f, 0.0f}},
      {{-0.35f, 1.0f, 0.0f}, {0.1f, 1.0f}},
      {{-0.35f, 1.0f, 0.0f}, {0.1f, 1.0f}},
      {{0.5f, 0.0f, 0.0f}, {1.0f, 0.0f}},
      {{0.35f, 1.0f, 0.0f}, {0.9f, 1.0f}},
  };

  glGenVertexArrays(1, &m_grassVao);
  glBindVertexArray(m_grassVao);

  glGenBuffers(1, &m_grassVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_grassVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(bladeVertices), bladeVertices,
               GL_STATIC_DRAW);
  m_grassVertexCount = 6;

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      0, 3, GL_FLOAT, GL_FALSE, sizeof(GrassVertex),
      reinterpret_cast<void *>(offsetof(GrassVertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GrassVertex),
                        reinterpret_cast<void *>(offsetof(GrassVertex, uv)));

  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1);
  glEnableVertexAttribArray(3);
  glVertexAttribDivisor(3, 1);
  glEnableVertexAttribArray(4);
  glVertexAttribDivisor(4, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Backend::shutdownGrassPipeline() {
  initializeOpenGLFunctions();
  if (m_grassVertexBuffer) {
    glDeleteBuffers(1, &m_grassVertexBuffer);
    m_grassVertexBuffer = 0;
  }
  if (m_grassVao) {
    glDeleteVertexArrays(1, &m_grassVao);
    m_grassVao = 0;
  }
  m_grassVertexCount = 0;
}

void Backend::cacheStoneUniforms() {
  if (m_stoneShader) {
    m_stoneUniforms.viewProj = m_stoneShader->uniformHandle("uViewProj");
    m_stoneUniforms.lightDirection =
        m_stoneShader->uniformHandle("uLightDirection");
  }
}

void Backend::initializeStonePipeline() {
  initializeOpenGLFunctions();
  shutdownStonePipeline();

  struct StoneVertex {
    QVector3D position;
    QVector3D normal;
  };

  const StoneVertex stoneVertices[] = {

      {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
      {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
      {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
      {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},

      {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
      {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
      {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
      {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},

      {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},

      {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
      {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
      {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
      {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},

      {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},

      {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
      {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
      {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
      {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
  };

  const uint16_t stoneIndices[] = {
      0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
      12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};

  glGenVertexArrays(1, &m_stoneVao);
  glBindVertexArray(m_stoneVao);

  glGenBuffers(1, &m_stoneVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_stoneVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(stoneVertices), stoneVertices,
               GL_STATIC_DRAW);
  m_stoneVertexCount = 24;

  glGenBuffers(1, &m_stoneIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_stoneIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(stoneIndices), stoneIndices,
               GL_STATIC_DRAW);
  m_stoneIndexCount = 36;

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      0, 3, GL_FLOAT, GL_FALSE, sizeof(StoneVertex),
      reinterpret_cast<void *>(offsetof(StoneVertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(
      1, 3, GL_FLOAT, GL_FALSE, sizeof(StoneVertex),
      reinterpret_cast<void *>(offsetof(StoneVertex, normal)));

  glEnableVertexAttribArray(2);
  glVertexAttribDivisor(2, 1);
  glEnableVertexAttribArray(3);
  glVertexAttribDivisor(3, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Backend::shutdownStonePipeline() {
  initializeOpenGLFunctions();
  if (m_stoneIndexBuffer) {
    glDeleteBuffers(1, &m_stoneIndexBuffer);
    m_stoneIndexBuffer = 0;
  }
  if (m_stoneVertexBuffer) {
    glDeleteBuffers(1, &m_stoneVertexBuffer);
    m_stoneVertexBuffer = 0;
  }
  if (m_stoneVao) {
    glDeleteVertexArrays(1, &m_stoneVao);
    m_stoneVao = 0;
  }
  m_stoneVertexCount = 0;
  m_stoneIndexCount = 0;
}

} // namespace Render::GL
