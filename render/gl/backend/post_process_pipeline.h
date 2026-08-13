#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <array>

#include "pipeline_interface.h"
#include "render/gl/shader.h"
#include "render/gl/shader_cache.h"

namespace Render::GL::BackendPipelines {

class PostProcessPipeline final : public IPipeline {
public:
  explicit PostProcessPipeline(GL::ShaderCache* shader_cache);
  ~PostProcessPipeline() override;

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override { return m_initialized; }

  [[nodiscard]] auto begin_scene(int width, int height) -> bool;
  void draw_sky(const QMatrix4x4& view_projection, const QVector3D& camera_position);
  void set_depth_range(float near_plane, float far_plane) noexcept;
  void resolve_scene();

  [[nodiscard]] auto is_capturing() const noexcept -> bool { return m_capturing; }
  [[nodiscard]] auto scene_is_high_dynamic_range() const noexcept -> bool {
    return m_scene_is_float;
  }

private:
  struct RenderTarget {
    unsigned int fbo{0};
    unsigned int color{0};
    int width{0};
    int height{0};
  };

  auto ensure_targets(int width, int height) -> bool;
  void release_targets();
  auto create_color_target(RenderTarget& target,
                           int width,
                           int height,
                           unsigned int internal_format) -> bool;
  void draw_fullscreen();
  void blur_bloom_once();

  static constexpr int k_bloom_divisor = 3;
  static constexpr float k_bloom_threshold = 0.98F;
  static constexpr float k_bloom_knee = 0.35F;
  static constexpr float k_bloom_intensity = 0.24F;
  static constexpr float k_vignette_strength = 0.14F;
  static constexpr float k_ground_ao_radius = 0.55F;
  static constexpr float k_ground_ao_strength = 0.62F;

  GL::ShaderCache* m_shader_cache;
  GL::Shader* m_bright_shader{nullptr};
  GL::Shader* m_blur_shader{nullptr};
  GL::Shader* m_composite_shader{nullptr};
  GL::Shader* m_fxaa_shader{nullptr};
  GL::Shader* m_sky_shader{nullptr};

  RenderTarget m_scene{};
  RenderTarget m_composite{};
  std::array<RenderTarget, 2> m_bloom{};
  unsigned int m_scene_depth{0};
  float m_near_plane{1.0F};
  float m_far_plane{200.0F};
  unsigned int m_empty_vao{0};

  int m_output_fbo{0};
  std::array<int, 4> m_output_viewport{{0, 0, 0, 0}};
  bool m_capturing{false};
  bool m_scene_is_float{false};
  bool m_initialized{false};
  bool m_targets_failed{false};
};

} // namespace Render::GL::BackendPipelines
