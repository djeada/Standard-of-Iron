#pragma once

#include "../shader.h"
#include "pipeline_interface.h"

namespace Render::GL {
class ShaderCache;
class Backend;

namespace BackendPipelines {

class WaterPipeline final : public IPipeline {
public:
  explicit WaterPipeline(GL::Backend *backend, GL::ShaderCache *shaderCache)
      : m_backend(backend), m_shaderCache(shaderCache) {}
  ~WaterPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cacheUniforms() override;
  [[nodiscard]] auto isInitialized() const -> bool override;

  struct RiverUniforms {
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle view{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle projection{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
  };

  struct RiverbankUniforms {
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle view{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle projection{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
  };

  struct BridgeUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  };

  GL::Shader *m_riverShader = nullptr;
  GL::Shader *m_riverbankShader = nullptr;
  GL::Shader *m_bridgeShader = nullptr;

  RiverUniforms m_riverUniforms;
  RiverbankUniforms m_riverbankUniforms;
  BridgeUniforms m_bridgeUniforms;

private:
  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shaderCache = nullptr;

  void cacheRiverUniforms();
  void cacheRiverbankUniforms();
  void cacheBridgeUniforms();
};

} // namespace BackendPipelines
} // namespace Render::GL
