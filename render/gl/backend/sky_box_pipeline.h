#pragma once

#include <QMatrix4x4>
#include <QtGui/qopengl.h>

#include "pipeline_interface.h"
#include "render/gl/shader.h"

namespace Render::GL {
class ShaderCache;

namespace BackendPipelines {

class SkyBoxPipeline final : public IPipeline {
public:
  explicit SkyBoxPipeline(GL::ShaderCache* shader_cache);
  ~SkyBoxPipeline() override;

  SkyBoxPipeline(const SkyBoxPipeline&) = delete;
  auto operator=(const SkyBoxPipeline&) -> SkyBoxPipeline& = delete;
  SkyBoxPipeline(SkyBoxPipeline&&) = delete;
  auto operator=(SkyBoxPipeline&&) -> SkyBoxPipeline& = delete;

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override { return m_initialized; }

  void
  draw(const QMatrix4x4& view, const QMatrix4x4& projection, float time, float blend);

  [[nodiscard]] static auto camera_rotation_only(const QMatrix4x4& view) -> QMatrix4x4;

private:
  struct Uniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle blend{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
  };

  auto build_cube() -> bool;
  void release_cube();

  GL::ShaderCache* m_shader_cache;
  GL::Shader* m_shader{nullptr};
  Uniforms m_uniforms;
  GLuint m_vao{0};
  GLuint m_vertex_buffer{0};
  GLuint m_index_buffer{0};
  bool m_initialized{false};
};

} // namespace BackendPipelines
} // namespace Render::GL
