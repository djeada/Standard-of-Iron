#pragma once

#include "../shader.h"
#include "pipeline_interface.h"

namespace Render::GL {
class ShaderCache;
class Backend;

namespace BackendPipelines {

class EffectsPipeline final : public IPipeline {
public:
  explicit EffectsPipeline(GL::Backend *backend, GL::ShaderCache *shaderCache)
      : m_backend(backend), m_shaderCache(shaderCache) {}
  ~EffectsPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  struct GridUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle gridColor{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle lineColor{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle cellSize{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle thickness{GL::Shader::InvalidUniform};
  };

  struct BasicUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle useTexture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha{GL::Shader::InvalidUniform};
  };

  GL::Shader *m_basicShader = nullptr;
  GL::Shader *m_gridShader = nullptr;

  BasicUniforms m_basicUniforms;
  GridUniforms m_gridUniforms;

private:
  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shaderCache = nullptr;

  void cache_basic_uniforms();
  void cache_grid_uniforms();
};

} // namespace BackendPipelines
} // namespace Render::GL
