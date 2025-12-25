#pragma once

#include "../shader.h"
#include "pipeline_interface.h"

namespace Render::GL {
class ShaderCache;
class Backend;

namespace BackendPipelines {

class WaterPipeline final : public IPipeline {
public:
  explicit WaterPipeline(GL::Backend *backend, GL::ShaderCache *shader_cache)
      : m_backend(backend), m_shaderCache(shader_cache) {}
  ~WaterPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

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
    GL::Shader::UniformHandle visibility_texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle visibility_size{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle visibility_tile_size{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle explored_alpha{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle has_visibility{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle segment_visibility{GL::Shader::InvalidUniform};
  };

  struct BridgeUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
  };

  struct RoadUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha{GL::Shader::InvalidUniform};
  };

  GL::Shader *m_riverShader = nullptr;
  GL::Shader *m_riverbankShader = nullptr;
  GL::Shader *m_bridgeShader = nullptr;
  GL::Shader *m_road_shader = nullptr;

  RiverUniforms m_riverUniforms;
  RiverbankUniforms m_riverbankUniforms;
  BridgeUniforms m_bridgeUniforms;
  RoadUniforms m_road_uniforms;

private:
  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shaderCache = nullptr;

  void cache_river_uniforms();
  void cache_riverbank_uniforms();
  void cache_bridge_uniforms();
  void cache_road_uniforms();
};

} 
} 
