#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QtGui/qopengl.h>

#include "../shader.h"
#include "pipeline_interface.h"

namespace Render::GL {
class ShaderCache;
class Backend;
class Mesh;

namespace BackendPipelines {

class ModeIndicatorPipeline final : public IPipeline {
public:
  explicit ModeIndicatorPipeline(GL::Backend* backend, GL::ShaderCache* shader_cache)
      : m_backend(backend)
      , m_shader_cache(shader_cache) {}
  ~ModeIndicatorPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  void render_indicator(Mesh* mesh,
                        const QMatrix4x4& model,
                        const QMatrix4x4& view_proj,
                        const QVector3D& color,
                        float alpha,
                        float time);

  struct IndicatorInstancedUniforms {
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
  };

  void cache_instanced_uniforms();

  GL::Shader* m_instanced_shader{nullptr};
  IndicatorInstancedUniforms m_instanced_uniforms;

private:
  GL::Backend* m_backend = nullptr;
  GL::ShaderCache* m_shader_cache = nullptr;
  GL::Shader* m_indicator_shader = nullptr;

  struct IndicatorUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle mode_color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
  };

  IndicatorUniforms m_uniforms;
};

} // namespace BackendPipelines
} // namespace Render::GL
