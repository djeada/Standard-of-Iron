#pragma once

#include "../draw_queue.h"
#include "../ground/grass_gpu.h"
#include "../ground/terrain_gpu.h"
#include "camera.h"
#include "resources.h"
#include "shader.h"
#include "shader_cache.h"
#include <QOpenGLFunctions_3_3_Core>
#include <QVector2D>
#include <QVector3D>
#include <array>
#include <memory>
#include <vector>

namespace Render::GL {

class Backend : protected QOpenGLFunctions_3_3_Core {
public:
  Backend() = default;
  ~Backend();
  void initialize();
  void beginFrame();
  void setViewport(int w, int h);
  void setClearColor(float r, float g, float b, float a);
  void execute(const DrawQueue &queue, const Camera &cam);

  ResourceManager *resources() const { return m_resources.get(); }

  Shader *shader(const QString &name) const {
    return m_shaderCache ? m_shaderCache->get(name) : nullptr;
  }
  Shader *getOrLoadShader(const QString &name, const QString &vertPath,
                          const QString &fragPath) {
    if (!m_shaderCache)
      return nullptr;
    return m_shaderCache->load(name, vertPath, fragPath);
  }

  void enableDepthTest(bool enable) {
    if (enable)
      glEnable(GL_DEPTH_TEST);
    else
      glDisable(GL_DEPTH_TEST);
  }
  void setDepthFunc(GLenum func) { glDepthFunc(func); }
  void setDepthMask(bool write) { glDepthMask(write ? GL_TRUE : GL_FALSE); }

  void enableBlend(bool enable) {
    if (enable)
      glEnable(GL_BLEND);
    else
      glDisable(GL_BLEND);
  }
  void setBlendFunc(GLenum src, GLenum dst) { glBlendFunc(src, dst); }

  void enablePolygonOffset(bool enable) {
    if (enable)
      glEnable(GL_POLYGON_OFFSET_FILL);
    else
      glDisable(GL_POLYGON_OFFSET_FILL);
  }
  void setPolygonOffset(float factor, float units) {
    glPolygonOffset(factor, units);
  }

private:
  int m_viewportWidth{0};
  int m_viewportHeight{0};
  std::array<float, 4> m_clearColor{0.2f, 0.3f, 0.3f, 0.0f};
  std::unique_ptr<ShaderCache> m_shaderCache;
  std::unique_ptr<ResourceManager> m_resources;

  Shader *m_basicShader = nullptr;
  Shader *m_gridShader = nullptr;
  Shader *m_cylinderShader = nullptr;
  Shader *m_fogShader = nullptr;
  Shader *m_grassShader = nullptr;
  Shader *m_terrainShader = nullptr;

  struct BasicUniforms {
    Shader::UniformHandle mvp{Shader::InvalidUniform};
    Shader::UniformHandle model{Shader::InvalidUniform};
    Shader::UniformHandle texture{Shader::InvalidUniform};
    Shader::UniformHandle useTexture{Shader::InvalidUniform};
    Shader::UniformHandle color{Shader::InvalidUniform};
    Shader::UniformHandle alpha{Shader::InvalidUniform};
  } m_basicUniforms;

  struct GridUniforms {
    Shader::UniformHandle mvp{Shader::InvalidUniform};
    Shader::UniformHandle model{Shader::InvalidUniform};
    Shader::UniformHandle gridColor{Shader::InvalidUniform};
    Shader::UniformHandle lineColor{Shader::InvalidUniform};
    Shader::UniformHandle cellSize{Shader::InvalidUniform};
    Shader::UniformHandle thickness{Shader::InvalidUniform};
  } m_gridUniforms;

  struct CylinderUniforms {
    Shader::UniformHandle viewProj{Shader::InvalidUniform};
  } m_cylinderUniforms;

  struct FogUniforms {
    Shader::UniformHandle viewProj{Shader::InvalidUniform};
  } m_fogUniforms;

  struct GrassUniforms {
    Shader::UniformHandle viewProj{Shader::InvalidUniform};
    Shader::UniformHandle time{Shader::InvalidUniform};
    Shader::UniformHandle windStrength{Shader::InvalidUniform};
    Shader::UniformHandle windSpeed{Shader::InvalidUniform};
    Shader::UniformHandle soilColor{Shader::InvalidUniform};
    Shader::UniformHandle lightDir{Shader::InvalidUniform};
  } m_grassUniforms;

  struct TerrainUniforms {
    Shader::UniformHandle mvp{Shader::InvalidUniform};
    Shader::UniformHandle model{Shader::InvalidUniform};
    Shader::UniformHandle grassPrimary{Shader::InvalidUniform};
    Shader::UniformHandle grassSecondary{Shader::InvalidUniform};
    Shader::UniformHandle grassDry{Shader::InvalidUniform};
    Shader::UniformHandle soilColor{Shader::InvalidUniform};
    Shader::UniformHandle rockLow{Shader::InvalidUniform};
    Shader::UniformHandle rockHigh{Shader::InvalidUniform};
    Shader::UniformHandle tint{Shader::InvalidUniform};
    Shader::UniformHandle noiseOffset{Shader::InvalidUniform};
    Shader::UniformHandle tileSize{Shader::InvalidUniform};
    Shader::UniformHandle macroNoiseScale{Shader::InvalidUniform};
    Shader::UniformHandle detailNoiseScale{Shader::InvalidUniform};
    Shader::UniformHandle slopeRockThreshold{Shader::InvalidUniform};
    Shader::UniformHandle slopeRockSharpness{Shader::InvalidUniform};
    Shader::UniformHandle soilBlendHeight{Shader::InvalidUniform};
    Shader::UniformHandle soilBlendSharpness{Shader::InvalidUniform};
    Shader::UniformHandle heightNoiseStrength{Shader::InvalidUniform};
    Shader::UniformHandle heightNoiseFrequency{Shader::InvalidUniform};
    Shader::UniformHandle ambientBoost{Shader::InvalidUniform};
    Shader::UniformHandle rockDetailStrength{Shader::InvalidUniform};
    Shader::UniformHandle lightDir{Shader::InvalidUniform};
  } m_terrainUniforms;

  struct CylinderInstanceGpu {
    QVector3D start;
    float radius{0.0f};
    QVector3D end;
    float alpha{1.0f};
    QVector3D color;
    float padding{0.0f};
  };

  GLuint m_cylinderVao = 0;
  GLuint m_cylinderVertexBuffer = 0;
  GLuint m_cylinderIndexBuffer = 0;
  GLuint m_cylinderInstanceBuffer = 0;
  GLsizei m_cylinderIndexCount = 0;
  std::size_t m_cylinderInstanceCapacity = 0;
  std::vector<CylinderInstanceGpu> m_cylinderScratch;

  struct FogInstanceGpu {
    QVector3D center;
    float size{1.0f};
    QVector3D color;
    float alpha{1.0f};
  };

  GLuint m_fogVao = 0;
  GLuint m_fogVertexBuffer = 0;
  GLuint m_fogIndexBuffer = 0;
  GLuint m_fogInstanceBuffer = 0;
  GLsizei m_fogIndexCount = 0;
  std::size_t m_fogInstanceCapacity = 0;
  std::vector<FogInstanceGpu> m_fogScratch;

  GLuint m_grassVao = 0;
  GLuint m_grassVertexBuffer = 0;
  GLsizei m_grassVertexCount = 0;

  void cacheBasicUniforms();
  void cacheGridUniforms();
  void cacheCylinderUniforms();
  void initializeCylinderPipeline();
  void shutdownCylinderPipeline();
  void uploadCylinderInstances(std::size_t count);
  void drawCylinders(std::size_t count);
  void cacheFogUniforms();
  void initializeFogPipeline();
  void shutdownFogPipeline();
  void uploadFogInstances(std::size_t count);
  void drawFog(std::size_t count);
  void cacheGrassUniforms();
  void initializeGrassPipeline();
  void shutdownGrassPipeline();
  void cacheTerrainUniforms();

  Shader *m_lastBoundShader = nullptr;
  Texture *m_lastBoundTexture = nullptr;
  bool m_depthTestEnabled = true;
  bool m_blendEnabled = false;
};

} // namespace Render::GL
