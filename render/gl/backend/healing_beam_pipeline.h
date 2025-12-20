#pragma once

#include "../shader.h"
#include "pipeline_interface.h"
#include <GL/gl.h>
#include <QMatrix4x4>
#include <QVector3D>
#include <vector>

namespace Game::Systems {
class HealingBeamSystem;
class HealingBeam;
} // namespace Game::Systems

namespace Render::GL {
class ShaderCache;
class Backend;
class Camera;

namespace BackendPipelines {

class HealingBeamPipeline final : public IPipeline {
public:
  explicit HealingBeamPipeline(GL::Backend *backend,
                               GL::ShaderCache *shader_cache)
      : m_backend(backend), m_shaderCache(shader_cache) {}
  ~HealingBeamPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  void render(const Game::Systems::HealingBeamSystem *beam_system,
              const Camera &cam, float animation_time);

  void render_single_beam(const QVector3D &start, const QVector3D &end,
                          const QVector3D &color, float progress,
                          float beam_width, float intensity, float time,
                          const QMatrix4x4 &view_proj);

private:
  void render_beam(const Game::Systems::HealingBeam &beam, const Camera &cam,
                   float animation_time);
  auto create_beam_geometry() -> bool;
  void shutdown_geometry();

  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shaderCache = nullptr;
  GL::Shader *m_beamShader = nullptr;

  GLuint m_vao = 0;
  GLuint m_vertexBuffer = 0;
  GLuint m_indexBuffer = 0;
  GLsizei m_indexCount = 0;

  struct BeamUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle progress{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle startPos{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle endPos{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle beamWidth{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle healColor{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha{GL::Shader::InvalidUniform};
  };

  BeamUniforms m_uniforms;
};

} // namespace BackendPipelines
} // namespace Render::GL
