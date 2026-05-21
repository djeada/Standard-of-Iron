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

enum class EffectType {
  Dust = 0,
  Flame = 1,
  StoneImpact = 2,
  Fireball = 3,
  BurningFlame = 4
};

struct CombatDustData {
  QVector3D position;
  float radius;
  float intensity;
  QVector3D color;
  float time;
  EffectType effect_type{EffectType::Dust};
};

struct BloodPoolData {
  QVector3D position;
  float radius;
  float alpha_scale{1.0F};
  float rotation{0.0F};
  float aspect_ratio{1.0F};
  float seed{0.0F};
};

class CombatDustPipeline final : public IPipeline {
public:
  explicit CombatDustPipeline(GL::Backend* backend, GL::ShaderCache* shader_cache)
      : m_backend(backend)
      , m_shader_cache(shader_cache) {}
  ~CombatDustPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  void collect_combat_zones(Engine::Core::World* world, float animation_time);

  void collect_building_flames(Engine::Core::World* world, float animation_time);

  void collect_all_effects(Engine::Core::World* world, float animation_time);
  void collect_blood_pools(Engine::Core::World* world);

  void render(const Camera& cam, float animation_time);

  void render_single_dust(const QVector3D& position,
                          const QVector3D& color,
                          float radius,
                          float intensity,
                          float time,
                          const QMatrix4x4& view_proj);

  void render_single_flame(const QVector3D& position,
                           const QVector3D& color,
                           float radius,
                           float intensity,
                           float time,
                           const QMatrix4x4& view_proj);

  void render_single_fireball(const QVector3D& position,
                              const QVector3D& color,
                              float radius,
                              float intensity,
                              float time,
                              const QMatrix4x4& view_proj);

  void render_single_stone_impact(const QVector3D& position,
                                  const QVector3D& color,
                                  float radius,
                                  float intensity,
                                  float time,
                                  const QMatrix4x4& view_proj);

  void render_single_blood_pool(const QVector3D& position,
                                float radius,
                                float alpha_scale,
                                float rotation,
                                float aspect_ratio,
                                float seed,
                                const QMatrix4x4& view_proj);

  struct DustInstanceData {
    QVector3D position;
    QVector3D color;
    float radius{2.0F};
    float intensity{0.6F};
    float time{0.0F};
    EffectType effect_type{EffectType::Dust};
    bool overlay{false};
  };

  void render_dust_batch(const DustInstanceData* instances,
                         std::size_t count,
                         const QMatrix4x4& view_proj);

  struct BloodPoolInstanceData {
    QVector3D position;
    float radius{0.6F};
    float alpha_scale{1.0F};
    float rotation{0.0F};
    float aspect_ratio{1.0F};
    float seed{0.0F};
  };

  void render_blood_pool_batch(const BloodPoolInstanceData* instances,
                               std::size_t count,
                               const QMatrix4x4& view_proj);

  void clear_data() {
    m_dust_data.clear();
    m_blood_data.clear();
  }

  void add_dust_zone(const QVector3D& position,
                     float radius,
                     float intensity,
                     const QVector3D& color,
                     float time);

  void add_flame_zone(const QVector3D& position,
                      float radius,
                      float intensity,
                      const QVector3D& color,
                      float time);

private:
  void render_dust(const CombatDustData& data, const Camera& cam);
  auto create_dust_geometry() -> bool;
  void shutdown_geometry();
  auto create_fireball_geometry() -> bool;
  void shutdown_fireball_geometry();
  auto create_blood_geometry() -> bool;
  void shutdown_blood_geometry();
  void render_blood_pools(const Camera& cam);

  GL::Backend* m_backend = nullptr;
  GL::ShaderCache* m_shader_cache = nullptr;
  GL::Shader* m_dust_shader = nullptr;
  GL::Shader* m_blood_shader = nullptr;

  GLuint m_vao = 0;
  GLuint m_vertex_buffer = 0;
  GLuint m_index_buffer = 0;
  GLsizei m_index_count = 0;
  GLuint m_fireball_vao = 0;
  GLuint m_fireball_vertex_buffer = 0;
  GLuint m_fireball_index_buffer = 0;
  GLsizei m_fireball_index_count = 0;
  GLuint m_blood_vao = 0;
  GLuint m_blood_vertex_buffer = 0;
  GLuint m_blood_index_buffer = 0;
  GLsizei m_blood_index_count = 0;

  std::vector<CombatDustData> m_dust_data;
  std::vector<BloodPoolData> m_blood_data;

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

  struct BloodUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle radius{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha_scale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rotation{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle aspect_ratio{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle seed{GL::Shader::InvalidUniform};
  };

  DustUniforms m_uniforms;
  BloodUniforms m_blood_uniforms;
};

} // namespace BackendPipelines
} // namespace Render::GL
