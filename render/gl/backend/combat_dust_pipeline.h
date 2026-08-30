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

enum class EffectType {
  Dust = 0,
  Flame = 1,
  StoneImpact = 2,
  Fireball = 3,
  BurningFlame = 4,
  MetalSpark = 5,
  WeaponArc = 6
};

class CombatDustPipeline final : public IPipeline {
public:
  explicit CombatDustPipeline(GL::ShaderCache* shader_cache)
      : m_shader_cache(shader_cache) {}
  ~CombatDustPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  struct DustInstanceData {
    QVector3D position;
    QVector3D color;
    float radius{2.0F};
    float intensity{0.6F};
    float time{0.0F};
    EffectType effect_type{EffectType::Dust};
    bool overlay{false};

    QVector3D direction{0.0F, 0.0F, 0.0F};
    float span{0.0F};
    float tilt{0.0F};
  };

  void render_dust_batch(const DustInstanceData* instances,
                         std::size_t count,
                         const QMatrix4x4& view_proj);

  void set_view_position(const QVector3D& view_position) {
    m_view_position = view_position;
  }

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

private:
  auto create_dust_geometry() -> bool;
  auto create_fireball_geometry() -> bool;
  auto create_metal_spark_geometry() -> bool;
  auto create_weapon_arc_geometry() -> bool;
  auto create_blood_geometry() -> bool;
  void release_geometry();

  GL::ShaderCache* m_shader_cache = nullptr;
  GL::Shader* m_dust_shader = nullptr;
  GL::Shader* m_blood_shader = nullptr;

  StaticMeshBuffers m_dust_mesh;
  StaticMeshBuffers m_fireball_mesh;
  StaticMeshBuffers m_metal_spark_mesh;
  StaticMeshBuffers m_weapon_arc_mesh;
  StaticMeshBuffers m_blood_mesh;

  struct DustUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle center{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle radius{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle intensity{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle dust_color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle effect_type{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_pos{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle span{GL::Shader::InvalidUniform};
  };

  struct BloodUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle radius{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha_scale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle rotation{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle aspect_ratio{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle seed{GL::Shader::InvalidUniform};
  };

  QVector3D m_view_position;
  DustUniforms m_uniforms;
  BloodUniforms m_blood_uniforms;
};

} // namespace BackendPipelines
} // namespace Render::GL
