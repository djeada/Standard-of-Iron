#pragma once

#include "../shader.h"
#include "pipeline_interface.h"
#include <GL/gl.h>
#include <QMatrix4x4>
#include <QVector3D>
#include <vector>

namespace Engine::Core {
class World;
}

namespace Render::GL {
class ShaderCache;
class Backend;
class Camera;

namespace BackendPipelines {

struct HealerAuraData {
  QVector3D position;
  float radius;
  float intensity;
  QVector3D color;
  bool is_active;
};

class HealerAuraPipeline final : public IPipeline {
public:
  explicit HealerAuraPipeline(GL::Backend *backend,
                              GL::ShaderCache *shaderCache)
      : m_backend(backend), m_shaderCache(shaderCache) {}
  ~HealerAuraPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  void collect_healers(Engine::Core::World *world);

  void render(const Camera &cam, float animation_time);

  void render_single_aura(const QVector3D &position, const QVector3D &color,
                          float radius, float intensity, float time,
                          const QMatrix4x4 &view_proj);

  void clear_data() { m_healerData.clear(); }

private:
  void render_aura(const HealerAuraData &data, const Camera &cam,
                   float animation_time);
  auto create_dome_geometry() -> bool;
  void shutdown_geometry();

  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shaderCache = nullptr;
  GL::Shader *m_auraShader = nullptr;

  GLuint m_vao = 0;
  GLuint m_vertexBuffer = 0;
  GLuint m_indexBuffer = 0;
  GLsizei m_indexCount = 0;

  std::vector<HealerAuraData> m_healerData;

  struct AuraUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle auraRadius{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle intensity{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle auraColor{GL::Shader::InvalidUniform};
  };

  AuraUniforms m_uniforms;
};

} // namespace BackendPipelines
} // namespace Render::GL
