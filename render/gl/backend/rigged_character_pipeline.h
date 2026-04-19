#pragma once

#include "../shader.h"
#include "pipeline_interface.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Render::GL {
class ShaderCache;
class Backend;
class Texture;
class Buffer;
class VertexArray;
class RiggedMesh;
struct RiggedCreatureCmd;
} // namespace Render::GL

namespace Render::GL::BackendPipelines {

class RiggedCharacterPipeline final : public IPipeline {
public:
  static constexpr std::size_t k_max_bones = 64;

  static constexpr std::size_t k_instanced_batch_ceiling = 64;

  static constexpr std::size_t k_instanced_batch_floor = 4;

  RiggedCharacterPipeline(GL::Backend *backend, GL::ShaderCache *shader_cache)
      : m_backend(backend), m_shader_cache(shader_cache) {}
  ~RiggedCharacterPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  auto draw(const RiggedCreatureCmd &cmd, const QMatrix4x4 &view_proj) -> bool;

  auto draw_instanced(const RiggedCreatureCmd *cmds, std::size_t count,
                      const QMatrix4x4 &view_proj) -> bool;

  [[nodiscard]] auto shader() const -> GL::Shader * { return m_shader; }
  [[nodiscard]] auto instanced_shader() const -> GL::Shader * {
    return m_instanced_shader;
  }
  [[nodiscard]] auto max_instances_per_batch() const noexcept -> std::size_t {
    return m_max_instances_per_batch;
  }

  [[nodiscard]] static constexpr auto palette_range_bytes_for_instanced_shader(
      std::size_t shader_batch_size) noexcept -> std::size_t {
    return shader_batch_size * k_max_bones * sizeof(float) * 16U;
  }

  void reset_batch_stats() noexcept {
    m_batch_sizes.clear();
    m_last_instance_count = 0;
  }
  [[nodiscard]] auto
  batch_sizes_for_test() const noexcept -> const std::vector<std::size_t> & {
    return m_batch_sizes;
  }
  [[nodiscard]] auto
  last_instance_count_for_test() const noexcept -> std::size_t {
    return m_last_instance_count;
  }

  static void compute_groups(const RiggedCreatureCmd *cmds, std::size_t count,
                             std::size_t cap,
                             std::vector<std::size_t> &out_groups);

  struct Uniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle variation_scale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle use_texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle material_id{GL::Shader::InvalidUniform};
  };

  [[nodiscard]] auto uniforms() const -> const Uniforms & { return m_uniforms; }

private:
  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shader_cache = nullptr;
  GL::Shader *m_shader = nullptr;
  GL::Shader *m_instanced_shader = nullptr;

  std::unique_ptr<Shader> m_instanced_shader_storage;
  Uniforms m_uniforms{};

  GL::Shader::UniformHandle m_instanced_view_proj{GL::Shader::InvalidUniform};

  std::size_t m_max_instances_per_batch = 0;

  unsigned int m_instance_vbo = 0;
  std::size_t m_instance_vbo_capacity_bytes = 0;

  unsigned int m_palette_ubo = 0;
  std::size_t m_palette_ubo_capacity_bytes = 0;
  std::vector<float> m_palette_scratch;

  struct InstancedVaoEntry {
    unsigned int vao = 0;
  };
  std::unordered_map<void *, InstancedVaoEntry> m_instanced_vaos;

  std::vector<std::size_t> m_batch_sizes;
  std::size_t m_last_instance_count = 0;

  struct InstanceAttrib {
    float world[16];
    float color_alpha[4];
    float variation_material[4];
  };
  std::vector<InstanceAttrib> m_instance_scratch;

  auto build_instanced_shader_source() -> bool;
  auto ensure_instance_vbo(std::size_t bytes_needed) -> bool;
  auto ensure_instanced_vao(RiggedMesh &mesh) -> unsigned int;
};

} // namespace Render::GL::BackendPipelines
