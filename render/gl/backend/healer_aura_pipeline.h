#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QtGui/qopengl.h>

#include <vector>

#include "../shader.h"
#include "pipeline_interface.h"

namespace Engine::Core {
class World;
}

namespace Render::GL {
class ShaderCache;
class Backend;
class Camera;

namespace BackendPipelines {

struct HealerAuraData {
  QVector3D position;
  float radius;
  float intensity;
  QVector3D color;
  bool is_active;
};

class HealerAuraPipeline final : public IPipeline {
public:
  explicit HealerAuraPipeline(GL::Backend* backend, GL::ShaderCache* shader_cache)
      : m_backend(backend)
      , m_shader_cache(shader_cache) {}
  ~HealerAuraPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  void collect_healers(Engine::Core::World* world);

  void render(const Camera& cam, float animation_time);

  void render_single_aura(const QVector3D& position,
                          const QVector3D& color,
                          float radius,
                          float intensity,
                          float time,
                          const QMatrix4x4& view_proj);

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

  void clear_data() { m_healer_data.clear(); }

private:
  void render_aura(const HealerAuraData& data, const Camera& cam, float animation_time);
  auto create_dome_geometry() -> bool;
  void shutdown_geometry();

  GL::Backend* m_backend = nullptr;
  GL::ShaderCache* m_shader_cache = nullptr;
  GL::Shader* m_aura_shader = nullptr;

  GLuint m_vao = 0;
  GLuint m_vertex_buffer = 0;
  GLuint m_index_buffer = 0;
  GLsizei m_index_count = 0;

  std::vector<HealerAuraData> m_healer_data;

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
