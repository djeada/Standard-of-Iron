#pragma once

#include "../../primitive_batch.h"
#include "../persistent_buffer.h"
#include "../shader_cache.h"
#include "pipeline_interface.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <memory>
#include <vector>

namespace Render::GL::BackendPipelines {

class PrimitiveBatchPipeline : public IPipeline {
public:
  explicit PrimitiveBatchPipeline(GL::ShaderCache *shaderCache);
  ~PrimitiveBatchPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cacheUniforms() override;
  [[nodiscard]] auto isInitialized() const -> bool override {
    return m_initialized;
  }

  void begin_frame();

  void upload_sphere_instances(const GL::PrimitiveInstanceGpu *data,
                               std::size_t count);
  void upload_cylinder_instances(const GL::PrimitiveInstanceGpu *data,
                                 std::size_t count);
  void upload_cone_instances(const GL::PrimitiveInstanceGpu *data,
                             std::size_t count);

  void draw_spheres(std::size_t count, const QMatrix4x4 &view_proj);
  void draw_cylinders(std::size_t count, const QMatrix4x4 &view_proj);
  void draw_cones(std::size_t count, const QMatrix4x4 &view_proj);

  [[nodiscard]] auto shader() const -> GL::Shader * { return m_shader; }

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

  GL::ShaderCache *m_shader_cache;
  bool m_initialized{false};

  GL::Shader *m_shader{nullptr};

  GLuint m_sphere_vao{0};
  GLuint m_sphere_vertex_buffer{0};
  GLuint m_sphere_index_buffer{0};
  GLuint m_sphere_instance_buffer{0};
  GLsizei m_sphere_index_count{0};
  std::size_t m_sphere_instance_capacity{0};

  GLuint m_cylinder_vao{0};
  GLuint m_cylinder_vertex_buffer{0};
  GLuint m_cylinder_index_buffer{0};
  GLuint m_cylinder_instance_buffer{0};
  GLsizei m_cylinder_index_count{0};
  std::size_t m_cylinder_instance_capacity{0};

  GLuint m_cone_vao{0};
  GLuint m_cone_vertex_buffer{0};
  GLuint m_cone_index_buffer{0};
  GLuint m_cone_instance_buffer{0};
  GLsizei m_cone_index_count{0};
  std::size_t m_cone_instance_capacity{0};

  static constexpr std::size_t k_default_instance_capacity = 4096;
  static constexpr float k_growth_factor = 1.5F;
};

} // namespace Render::GL::BackendPipelines
