#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QtGui/qopengl.h>

#include "mesh_buffers.h"
#include "pipeline_interface.h"
#include "render/gl/shader.h"

namespace Render::GL {
class ShaderCache;

namespace BackendPipelines {

class HealingBeamPipeline final : public IPipeline {
public:
  explicit HealingBeamPipeline(GL::ShaderCache* shader_cache)
      : m_shader_cache(shader_cache) {}
  ~HealingBeamPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  void render_single_beam(const QVector3D& start,
                          const QVector3D& end,
                          const QVector3D& color,
                          float progress,
                          float beam_width,
                          float intensity,
                          float time,
                          const QMatrix4x4& view_proj);

private:
  auto create_beam_geometry() -> bool;
  void release_geometry();

  GL::ShaderCache* m_shader_cache = nullptr;
  GL::Shader* m_beam_shader = nullptr;

  StaticMeshBuffers m_mesh;

  struct BeamUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle progress{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle start_pos{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle end_pos{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle beam_width{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle heal_color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha{GL::Shader::InvalidUniform};
  };

  BeamUniforms m_uniforms;
};

} // namespace BackendPipelines
} // namespace Render::GL
