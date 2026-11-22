#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QtGui/qopengl.h>

#include "render/backend/pipeline_draw_context.h"
#include "render/backend/pipeline_interface.h"
#include "render/frame/draw_queue.h"
#include "render/gl/shader.h"

namespace Render::GL {
class ShaderCache;
class Mesh;

namespace BackendPipelines {
class MeshInstancingPipeline;

class ModeIndicatorPipeline final : public IPipeline {
public:
  explicit ModeIndicatorPipeline(GL::ShaderCache* shader_cache)
      : m_shader_cache(shader_cache) {}
  ~ModeIndicatorPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  void draw_indicators(const PreparedBatch& prepared,
                       MeshInstancingPipeline* instancing,
                       PipelineDrawContext& context);

private:
  struct IndicatorInstancedUniforms {
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
  };

  void cache_instanced_uniforms();

  GL::ShaderCache* m_shader_cache = nullptr;
  GL::Shader* m_indicator_shader = nullptr;
  GL::Shader* m_instanced_shader{nullptr};
  IndicatorInstancedUniforms m_instanced_uniforms;

  void render_indicator(Mesh* mesh,
                        const QMatrix4x4& model,
                        const QMatrix4x4& view_proj,
                        const QVector3D& color,
                        float alpha,
                        float time);

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
