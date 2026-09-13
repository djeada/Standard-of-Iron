#pragma once

#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <array>
#include <cstddef>
#include <vector>

#include "instance_draw_guard.h"
#include "mesh_buffers.h"
#include "pipeline_interface.h"
#include "render/geom/ground_marker_tessellation.h"
#include "render/gl/shader.h"
#include "render/gl/shader_cache.h"

namespace Render::GL::BackendPipelines {

class GroundMarkerPipeline final : public IPipeline {
public:
  explicit GroundMarkerPipeline(GL::ShaderCache* shader_cache);
  ~GroundMarkerPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override { return m_initialized; }

  struct InstanceGpu {
    QVector4D center_radius;
    QVector4D color_alpha;
    QVector4D shape;
  };

  static constexpr std::size_t k_pattern_slots = 12;
  static constexpr std::size_t k_lod_count = Render::Geom::k_marker_segment_lods.size();

  struct Uniforms {
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle ground_offset{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle has_height_tex{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle height_tex{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle height_uv_scale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle height_uv_offset{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle height_to_world{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle angular_step{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle height_world_per_texel{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle slope_clearance{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle max_slope_lift{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle half_viewport{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle min_marker_pixels{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle max_thickness_gain{GL::Shader::InvalidUniform};
  };

  [[nodiscard]] auto shader() const -> GL::Shader* { return m_shader; }
  [[nodiscard]] auto uniforms() const -> const Uniforms& { return m_uniforms; }

  void upload_pattern_table();

  void begin_batch();
  void
  add_instance(const InstanceGpu& instance, float outer_radius, float world_per_cell);
  [[nodiscard]] auto batched_instance_count() const -> std::size_t;
  void flush_batch();

private:
  void build_meshes();
  void build_mesh(StaticMeshBuffers& mesh, int angular_segments);
  void upload_instances(std::size_t lod, std::size_t count);
  void draw(std::size_t lod, std::size_t count);

  GL::ShaderCache* m_shader_cache;
  GL::Shader* m_shader{nullptr};
  Uniforms m_uniforms;
  std::array<GL::Shader::UniformHandle, k_pattern_slots> m_pattern_handles{};
  std::array<StaticMeshBuffers, k_lod_count> m_meshes{};
  std::array<std::vector<InstanceGpu>, k_lod_count> m_scratch{};
  GLuint m_instance_buffer{0};
  std::size_t m_instance_capacity{0};
  std::size_t m_instances_resident{0};
  bool m_initialized{false};
  InstanceDrawGuard m_draw_guard{"GroundMarkerPipeline"};
};

} // namespace Render::GL::BackendPipelines
