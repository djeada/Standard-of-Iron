#pragma once

#include "../shader.h"
#include "pipeline_interface.h"

namespace Render::GL {
class ShaderCache;
class Backend;

namespace BackendPipelines {

class EffectsPipeline final : public IPipeline {
public:
  explicit EffectsPipeline(GL::Backend *backend, GL::ShaderCache *shader_cache)
      : m_backend(backend), m_shader_cache(shader_cache) {}
  ~EffectsPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  struct GridUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle grid_color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle line_color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle cell_size{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle thickness{GL::Shader::InvalidUniform};
  };

  struct BasicUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle use_texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle instanced{GL::Shader::InvalidUniform};
  };

  GL::Shader *m_basic_shader = nullptr;
  GL::Shader *m_grid_shader = nullptr;

  BasicUniforms m_basic_uniforms;
  GridUniforms m_grid_uniforms;

private:
  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shader_cache = nullptr;

  void cache_basic_uniforms();
  void cache_grid_uniforms();
};

} // namespace BackendPipelines
} // namespace Render::GL
