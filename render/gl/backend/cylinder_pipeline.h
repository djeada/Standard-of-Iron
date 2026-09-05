#pragma once

#include <QVector3D>

#include <memory>
#include <vector>

#include "instance_draw_guard.h"
#include "mesh_buffers.h"
#include "pipeline_interface.h"
#include "render/gl/persistent_buffer.h"
#include "render/gl/shader_cache.h"

namespace Render::GL {
class Buffer;
}

namespace Render::GL::BackendPipelines {

class CylinderPipeline : public IPipeline {
public:
  explicit CylinderPipeline(GL::ShaderCache* shader_cache);
  ~CylinderPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override { return m_initialized; }

  void begin_frame();
  void end_frame();
  void point_cylinder_instance_attributes(std::size_t base_bytes);

  void upload_cylinder_instances(std::size_t count);
  void draw_cylinders(std::size_t count);

  void upload_fog_instances(std::size_t count);
  void bind_fog_instance_buffer(GL::Buffer* instance_buffer);
  void draw_fog(std::size_t count);

  [[nodiscard]] auto cylinder_shader() const -> GL::Shader* {
    return m_cylinder_shader;
  }
  [[nodiscard]] auto fog_shader() const -> GL::Shader* { return m_fog_shader; }

  struct CylinderUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
  };

  struct FogUniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle mask_tex{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle mask_size{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle mask_tile_size{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle has_mask{GL::Shader::InvalidUniform};
  };

  struct CylinderInstanceGpu {
    QVector3D start;
    float radius{0.0F};
    QVector3D end;
    float alpha{1.0F};
    QVector3D color;
    float padding{0.0F};
  };

  struct FogInstanceGpu {
    QVector3D center;
    float size{1.0F};
    QVector3D color;
    float alpha{1.0F};
  };

  CylinderUniforms m_cylinder_uniforms;
  FogUniforms m_fog_uniforms;
  std::vector<CylinderInstanceGpu> m_cylinder_scratch;
  std::vector<FogInstanceGpu> m_fog_scratch;

private:
  void initialize_cylinder_pipeline();
  void shutdown_cylinder_pipeline();
  void initialize_fog_pipeline();
  void shutdown_fog_pipeline();

  GL::ShaderCache* m_shader_cache;
  bool m_initialized{false};
  bool m_use_persistent_buffers{false};
  bool m_cylinder_slot_writable{false};
  std::size_t m_cylinder_instance_base_bytes{0};
  bool m_fog_slot_writable{false};

  GL::Shader* m_cylinder_shader{nullptr};
  StaticMeshBuffers m_cylinder_mesh;
  std::size_t m_cylinder_instance_capacity{0};
  std::size_t m_cylinder_instances_resident{0};
  InstanceDrawGuard m_cylinder_draw_guard{"CylinderPipeline::cylinders"};
  GL::PersistentRingBuffer<CylinderInstanceGpu> m_cylinder_persistent_buffer;

  GL::Shader* m_fog_shader{nullptr};
  StaticMeshBuffers m_fog_mesh;
  std::size_t m_fog_instance_capacity{0};
  std::size_t m_fog_instances_resident{0};
  InstanceDrawGuard m_fog_draw_guard{"CylinderPipeline::fog"};
  GL::PersistentRingBuffer<FogInstanceGpu> m_fog_persistent_buffer;
};

} // namespace Render::GL::BackendPipelines
