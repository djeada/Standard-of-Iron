#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "pipeline_interface.h"
#include "render/gl/frame_environment.h"
#include "render/gl/shader.h"

namespace Render::GL {
class ShaderCache;
class Texture;
class Buffer;
class VertexArray;
class RiggedMesh;
struct RiggedCreatureCmd;
} // namespace Render::GL

namespace Render::GL::BackendPipelines {

class RiggedCharacterPipeline final : public IPipeline {
public:
  RiggedCharacterPipeline(const GL::IFrameEnvironment* frame_environment,
                          GL::ShaderCache* shader_cache)
      : m_frame_environment(frame_environment)
      , m_shader_cache(shader_cache) {}
  ~RiggedCharacterPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  auto draw(const RiggedCreatureCmd& cmd,
            const QMatrix4x4& view_proj,
            const QVector3D& camera_position = {}) -> bool;

  [[nodiscard]] auto shader() const -> GL::Shader* { return m_shader; }

  [[nodiscard]] auto last_bound_shader() const -> GL::Shader* {
    return m_last_bound_shader;
  }

  void set_wear_volume(unsigned int texture) { m_wear_volume = texture; }

  struct Uniforms {
    GL::Shader::UniformHandle view_proj{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle variation_scale{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle color{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wear_params{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle alpha{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle use_texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle texture{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle material_id{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle role_colors{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle role_color_count{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle light_dir{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle ambient_strength{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle camera_position{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle wear_volume{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle has_wear_volume{GL::Shader::InvalidUniform};
  };

  [[nodiscard]] auto uniforms() const -> const Uniforms& { return m_uniforms; }

  static constexpr std::size_t k_variant_count = 5;

private:
  const GL::IFrameEnvironment* m_frame_environment = nullptr;
  GL::ShaderCache* m_shader_cache = nullptr;
  GL::Shader* m_shader = nullptr;
  Uniforms m_uniforms{};
  std::array<GL::Shader*, k_variant_count> m_variant_shaders{};
  std::array<Uniforms, k_variant_count> m_variant_uniforms{};

  [[nodiscard]] static auto variant_for_material(int material_id) -> std::size_t;

  GL::Shader* m_last_bound_shader = nullptr;
  unsigned int m_wear_volume = 0;
  static constexpr std::size_t k_palette_ring_slots = 64;

  unsigned int m_palette_ubo = 0;
  std::size_t m_palette_ubo_capacity_bytes = 0;
  std::size_t m_palette_slot_stride_bytes = 0;
  std::size_t m_palette_ring_cursor = 0;
  std::uint64_t m_palette_ring_orphans = 0;
  std::vector<float> m_palette_scratch;
};

} // namespace Render::GL::BackendPipelines
