#pragma once

#include "../shader.h"
#include "pipeline_interface.h"

namespace Render::GL {
class ShaderCache;
class Backend;

namespace BackendPipelines {

class CharacterPipeline final : public IPipeline {
public:
  explicit CharacterPipeline(GL::Backend *backend, GL::ShaderCache *shaderCache)
      : m_backend(backend), m_shaderCache(shaderCache) {}
  ~CharacterPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cacheUniforms() override;
  [[nodiscard]] auto isInitialized() const -> bool override;

  struct BasicUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle useTexture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha{GL::Shader::InvalidUniform};
  };

  GL::Shader *m_basicShader = nullptr;
  GL::Shader *m_archerShader = nullptr;
  GL::Shader *m_swordsmanShader = nullptr;
  GL::Shader *m_spearmanShader = nullptr;

  BasicUniforms m_basicUniforms;
  BasicUniforms m_archerUniforms;
  BasicUniforms m_swordsmanUniforms;
  BasicUniforms m_spearmanUniforms;

private:
  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shaderCache = nullptr;

  void cacheBasicUniforms();
  void cacheArcherUniforms();
  void cacheKnightUniforms();
  void cacheSpearmanUniforms();
};

} // namespace BackendPipelines
} // namespace Render::GL
