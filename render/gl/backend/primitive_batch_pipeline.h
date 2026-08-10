#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <memory>
#include <vector>

#include "instance_draw_guard.h"
#include "mesh_buffers.h"
#include "pipeline_interface.h"
#include "render/gl/persistent_buffer.h"
#include "render/gl/shader_cache.h"
#include "render/primitive_batch.h"

namespace Render::GL::BackendPipelines {

class PrimitiveBatchPipeline : public IPipeline {
public:
  explicit PrimitiveBatchPipeline(GL::ShaderCache* shader_cache);
  ~PrimitiveBatchPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override { return m_initialized; }

  void begin_frame();

  void upload_sphere_instances(const GL::PrimitiveInstanceGpu* data, std::size_t count);
  void upload_cylinder_instances(const GL::PrimitiveInstanceGpu* data,
                                 std::size_t count);
  void upload_cone_instances(const GL::PrimitiveInstanceGpu* data, std::size_t count);

  void draw_spheres(std::size_t count,
                    const QMatrix4x4& view_proj,
                    const QVector3D& light_dir,
                    float ambient_strength);
  void draw_cylinders(std::size_t count,
                      const QMatrix4x4& view_proj,
                      const QVector3D& light_dir,
                      float ambient_strength);
  void draw_cones(std::size_t count,
                  const QMatrix4x4& view_proj,
                  const QVector3D& light_dir,
                  float ambient_strength);

  [[nodiscard]] auto shader() const -> GL::Shader* { return m_shader; }

  struct Uniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_dir{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle ambient_strength{GL::Shader::InvalidUniform};
  };

  Uniforms m_uniforms;

private:
  void initialize_sphere_vao();
  void initialize_cylinder_vao();
  void initialize_cone_vao();
  void shutdown_vaos();

  void setup_instance_attributes(GLuint vao, GLuint instance_buffer);

  GL::ShaderCache* m_shader_cache;
  bool m_initialized{false};

  GL::Shader* m_shader{nullptr};

  StaticMeshBuffers m_sphere_mesh;
  std::size_t m_sphere_instance_capacity{0};
  std::size_t m_sphere_instances_resident{0};
  InstanceDrawGuard m_sphere_draw_guard{"PrimitiveBatchPipeline::spheres"};

  StaticMeshBuffers m_cylinder_mesh;
  std::size_t m_cylinder_instance_capacity{0};
  std::size_t m_cylinder_instances_resident{0};
  InstanceDrawGuard m_cylinder_draw_guard{"PrimitiveBatchPipeline::cylinders"};

  StaticMeshBuffers m_cone_mesh;
  std::size_t m_cone_instance_capacity{0};
  std::size_t m_cone_instances_resident{0};
  InstanceDrawGuard m_cone_draw_guard{"PrimitiveBatchPipeline::cones"};

  static constexpr std::size_t k_default_instance_capacity = 4096;
  static constexpr float k_growth_factor = 1.5F;
};

} // namespace Render::GL::BackendPipelines
