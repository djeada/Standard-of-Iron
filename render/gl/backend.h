#pragma once

#include "../draw_queue.h"
#include "../ground/grass_gpu.h"
#include "../ground/plant_gpu.h"
#include "../ground/stone_gpu.h"
#include "../ground/terrain_gpu.h"
#include "camera.h"
#include "persistent_buffer.h"
#include "resources.h"
#include "shader.h"
#include "shader_cache.h"
#include <QOpenGLFunctions_3_3_Core>
#include <QVector2D>
#include <QVector3D>
#include <array>
#include <memory>
#include <vector>

namespace Render::GL::BackendPipelines {
class CylinderPipeline;
class VegetationPipeline;
class TerrainPipeline;
class CharacterPipeline;
class WaterPipeline;
class EffectsPipeline;
class PrimitiveBatchPipeline;
} // namespace Render::GL::BackendPipelines

namespace Render::GL {

class Backend : protected QOpenGLFunctions_3_3_Core {
public:
  friend class BackendPipelines::CylinderPipeline;
  friend class BackendPipelines::VegetationPipeline;

  Backend();
  ~Backend() override;

  Backend(const Backend &) = delete;
  auto operator=(const Backend &) -> Backend & = delete;
  Backend(Backend &&) = delete;
  auto operator=(Backend &&) -> Backend & = delete;

  void initialize();
  void beginFrame();
  void setViewport(int w, int h);
  void setClearColor(float r, float g, float b, float a);
  void setAnimationTime(float time) { m_animationTime = time; }
  void execute(const DrawQueue &queue, const Camera &cam);

  [[nodiscard]] auto resources() const -> ResourceManager * {
    return m_resources.get();
  }

  [[nodiscard]] auto shader(const QString &name) const -> Shader * {
    return m_shaderCache ? m_shaderCache->get(name) : nullptr;
  }
  auto get_or_load_shader(const QString &name, const QString &vertPath,
                       const QString &fragPath) -> Shader * {
    if (!m_shaderCache) {
      return nullptr;
    }
    return m_shaderCache->load(name, vertPath, fragPath);
  }

  void enableDepthTest(bool enable) {
    if (enable) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }
  void setDepthFunc(GLenum func) { glDepthFunc(func); }
  void setDepthMask(bool write) { glDepthMask(write ? GL_TRUE : GL_FALSE); }

  void enableBlend(bool enable) {
    if (enable) {
      glEnable(GL_BLEND);
    } else {
      glDisable(GL_BLEND);
    }
  }
  void setBlendFunc(GLenum src, GLenum dst) { glBlendFunc(src, dst); }

  void enablePolygonOffset(bool enable) {
    if (enable) {
      glEnable(GL_POLYGON_OFFSET_FILL);
    } else {
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
  }
  void setPolygonOffset(float factor, float units) {
    glPolygonOffset(factor, units);
  }

private:
  int m_viewportWidth{0};
  int m_viewportHeight{0};
  std::array<float, 4> m_clearColor{0.2F, 0.3F, 0.3F, 0.0F};
  std::unique_ptr<ShaderCache> m_shaderCache;
  std::unique_ptr<ResourceManager> m_resources;
  std::unique_ptr<BackendPipelines::CylinderPipeline> m_cylinderPipeline;
  std::unique_ptr<BackendPipelines::VegetationPipeline> m_vegetationPipeline;
  std::unique_ptr<BackendPipelines::TerrainPipeline> m_terrainPipeline;
  std::unique_ptr<BackendPipelines::CharacterPipeline> m_characterPipeline;
  std::unique_ptr<BackendPipelines::WaterPipeline> m_waterPipeline;
  std::unique_ptr<BackendPipelines::EffectsPipeline> m_effectsPipeline;
  std::unique_ptr<BackendPipelines::PrimitiveBatchPipeline>
      m_primitiveBatchPipeline;

  Shader *m_basicShader = nullptr;
  Shader *m_gridShader = nullptr;

  Shader *m_lastBoundShader = nullptr;
  Texture *m_lastBoundTexture = nullptr;
  bool m_depth_testEnabled = true;
  bool m_blendEnabled = false;
  float m_animationTime = 0.0F;
};

} // namespace Render::GL
