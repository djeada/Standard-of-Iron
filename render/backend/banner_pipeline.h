#pragma once

#include <memory>

#include "render/backend/pipeline_draw_context.h"
#include "render/backend/pipeline_interface.h"
#include "render/frame/draw_queue.h"
#include "render/geom/mesh.h"
#include "render/gl/shader.h"

namespace Render::GL {
class ShaderCache;

namespace BackendPipelines {

class BannerPipeline final : public IPipeline {
public:
  explicit BannerPipeline(GL::ShaderCache* shader_cache)
      : m_shader_cache(shader_cache) {}
  ~BannerPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  auto get_banner_mesh(int subdivisions = 16) -> GL::Mesh*;

  [[nodiscard]] auto shader() const -> GL::Shader* { return m_banner_shader; }

  void draw_banners(const PreparedBatch& prepared,
                    const MeshCmd& head,
                    bool is_transparent,
                    GL::Texture* fallback_texture,
                    PipelineDrawContext& context);

private:
  struct BannerUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wind_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_direction{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_pos{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle ambient_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle trim_color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle use_texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha{GL::Shader::InvalidUniform};
  };

  GL::ShaderCache* m_shader_cache = nullptr;

  GL::Shader* m_banner_shader = nullptr;
  BannerUniforms m_banner_uniforms;

  std::unique_ptr<GL::Mesh> m_banner_mesh16;
  std::unique_ptr<GL::Mesh> m_banner_mesh8;

  void cache_banner_uniforms();
};

} // namespace BackendPipelines
} // namespace Render::GL
