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

/**
 * @brief Pipeline for rendering magical healing beam visual effects.
 *
 * Renders glowing, animated energy beams from healers to their targets.
 * Uses GPU shaders for impressive magical effects including:
 * - Curved arc path with bezier interpolation
 * - Spiral twisting animation
 * - Flowing energy particles
 * - Golden-green magical glow
 */
class HealingBeamPipeline final : public IPipeline {
public:
  explicit HealingBeamPipeline(GL::Backend *backend,
                               GL::ShaderCache *shaderCache)
      : m_backend(backend), m_shaderCache(shaderCache) {}
  ~HealingBeamPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  /**
   * @brief Render all active healing beams.
   * @param beam_system The system containing active beams.
   * @param cam The camera for view/projection matrices.
   * @param animation_time Current animation time for shader effects.
   */
  void render(const Game::Systems::HealingBeamSystem *beam_system,
              const Camera &cam, float animation_time);

  /**
   * @brief Render a single healing beam with given parameters.
   * Called from backend execute() when processing HealingBeamCmd.
   */
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

  // Raw OpenGL handles like other pipelines
  GLuint m_vao = 0;
  GLuint m_vertexBuffer = 0;
  GLuint m_indexBuffer = 0;
  GLsizei m_indexCount = 0;

  // Cached uniform handles
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
