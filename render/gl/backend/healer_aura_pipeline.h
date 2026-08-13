#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QtGui/qopengl.h>

#include <cstddef>

#include "mesh_buffers.h"
#include "pipeline_interface.h"
#include "render/gl/shader.h"

namespace Render::GL {
class ShaderCache;

namespace BackendPipelines {

class HealerAuraPipeline final : public IPipeline {
public:
  explicit HealerAuraPipeline(GL::ShaderCache* shader_cache)
      : m_shader_cache(shader_cache) {}
  ~HealerAuraPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  struct AuraInstanceData {
    QVector3D position;
    QVector3D color;
    float radius{1.0F};
    float intensity{1.0F};
    float time{0.0F};
  };

  void render_aura_batch(const AuraInstanceData* instances,
                         std::size_t count,
                         const QMatrix4x4& view_proj);

private:
  auto create_dome_geometry() -> bool;
  void release_geometry();

  GL::ShaderCache* m_shader_cache = nullptr;
  GL::Shader* m_aura_shader = nullptr;

  StaticMeshBuffers m_mesh;

  struct AuraUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle aura_radius{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle intensity{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle aura_color{GL::Shader::InvalidUniform};
  };

  AuraUniforms m_uniforms;
};

} // namespace BackendPipelines
} // namespace Render::GL
