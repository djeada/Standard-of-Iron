#pragma once

#include "../shader.h"
#include "pipeline_interface.h"
#include <GL/gl.h>
#include <QMatrix4x4>
#include <QVector3D>
#include <vector>

namespace Engine::Core {
class World;
}

namespace Render::GL {
class ShaderCache;
class Backend;
class Camera;

namespace BackendPipelines {

enum class EffectType { Dust, Flame, StoneImpact };

struct CombatDustData {
  QVector3D position;
  float radius;
  float intensity;
  QVector3D color;
  float time;
  EffectType effect_type{EffectType::Dust};
};

class CombatDustPipeline final : public IPipeline {
public:
  explicit CombatDustPipeline(GL::Backend *backend,
                              GL::ShaderCache *shader_cache)
      : m_backend(backend), m_shader_cache(shader_cache) {}
  ~CombatDustPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  void collect_combat_zones(Engine::Core::World *world, float animation_time);

  void collect_building_flames(Engine::Core::World *world,
                               float animation_time);

  void collect_all_effects(Engine::Core::World *world, float animation_time);

  void render(const Camera &cam, float animation_time);

  void render_single_dust(const QVector3D &position, const QVector3D &color,
                          float radius, float intensity, float time,
                          const QMatrix4x4 &view_proj);

  void render_single_flame(const QVector3D &position, const QVector3D &color,
                           float radius, float intensity, float time,
                           const QMatrix4x4 &view_proj);

  void render_single_stone_impact(const QVector3D &position,
                                  const QVector3D &color, float radius,
                                  float intensity, float time,
                                  const QMatrix4x4 &view_proj);

  void clear_data() { m_dust_data.clear(); }

  void add_dust_zone(const QVector3D &position, float radius, float intensity,
                     const QVector3D &color, float time);

  void add_flame_zone(const QVector3D &position, float radius, float intensity,
                      const QVector3D &color, float time);

private:
  void render_dust(const CombatDustData &data, const Camera &cam);
  auto create_dust_geometry() -> bool;
  void shutdown_geometry();

  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shader_cache = nullptr;
  GL::Shader *m_dust_shader = nullptr;

  GLuint m_vao = 0;
  GLuint m_vertex_buffer = 0;
  GLuint m_index_buffer = 0;
  GLsizei m_index_count = 0;

  std::vector<CombatDustData> m_dust_data;

  struct DustUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle center{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle radius{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle intensity{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle dust_color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle effect_type{GL::Shader::InvalidUniform};
  };

  DustUniforms m_uniforms;
};

} // namespace BackendPipelines
} // namespace Render::GL
