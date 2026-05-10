#pragma once

#include "../decoration_gpu.h"
#include "../draw_queue.h"
#include "../frame_budget.h"
#include "../i_render_backend.h"
#include "../world_chunk.h"
#include "camera.h"
#include "persistent_buffer.h"
#include "resources.h"
#include "shader.h"
#include "shader_cache.h"
#include <QOpenGLFunctions_3_3_Core>
#include <QVector2D>
#include <QVector3D>
#include <array>
#include <memory>
#include <vector>

namespace Render::GL::BackendPipelines {
class CylinderPipeline;
class VegetationPipeline;
class TerrainPipeline;
class CharacterPipeline;
class RiggedCharacterPipeline;
class WaterPipeline;
class EffectsPipeline;
class PrimitiveBatchPipeline;
class BannerPipeline;
class HealingBeamPipeline;
class HealerAuraPipeline;
class CombatDustPipeline;
class RainPipeline;
class ModeIndicatorPipeline;
class MeshInstancingPipeline;
} // namespace Render::GL::BackendPipelines

namespace Render::GL {

class Backend : public IRenderBackend, protected QOpenGLFunctions_3_3_Core {
public:
  friend class BackendPipelines::CylinderPipeline;
  friend class BackendPipelines::VegetationPipeline;

  Backend();
  explicit Backend(ShaderQuality quality);
  ~Backend() override;

  Backend(const Backend &) = delete;
  auto operator=(const Backend &) -> Backend & = delete;
  Backend(Backend &&) = delete;
  auto operator=(Backend &&) -> Backend & = delete;

  void initialize() override;
  void begin_frame() override;
  void set_viewport(int w, int h) override;
  void set_clear_color(float r, float g, float b, float a) override;
  void set_animation_time(float time) noexcept override {
    m_animation_time = time;
  }
  void execute(const DrawQueue &queue, const Camera &cam) override;

  [[nodiscard]] auto resources() const -> ResourceManager * override {
    return m_resources.get();
  }

  [[nodiscard]] auto shader(const QString &name) const -> Shader * override {
    return m_shader_cache ? m_shader_cache->get(name) : nullptr;
  }

  [[nodiscard]] auto supports_shaders() const -> bool override { return true; }
  [[nodiscard]] auto shader_quality() const -> ShaderQuality override {
    return m_shader_quality;
  }
  void set_shader_quality(ShaderQuality q) noexcept { m_shader_quality = q; }
  auto get_or_load_shader(const QString &name, const QString &vert_path,
                          const QString &frag_path) -> Shader * {
    if (!m_shader_cache) {
      return nullptr;
    }
    return m_shader_cache->load(name, vert_path, frag_path);
  }

  [[nodiscard]] auto banner_mesh() const -> Mesh *;

  [[nodiscard]] auto banner_shader() const -> Shader *;

  [[nodiscard]] auto troop_shadow_shader() const noexcept -> Shader * {
    return m_shadow_shader;
  }

  [[nodiscard]] auto
  healing_beam_pipeline() -> BackendPipelines::HealingBeamPipeline * {
    return m_healing_beam_pipeline.get();
  }

  [[nodiscard]] auto
  healer_aura_pipeline() -> BackendPipelines::HealerAuraPipeline * {
    return m_healer_aura_pipeline.get();
  }

  [[nodiscard]] auto
  combat_dust_pipeline() -> BackendPipelines::CombatDustPipeline * {
    return m_combat_dust_pipeline.get();
  }

  [[nodiscard]] auto rain_pipeline() -> BackendPipelines::RainPipeline * {
    return m_rain_pipeline.get();
  }

  [[nodiscard]] auto
  mode_indicator_pipeline() -> BackendPipelines::ModeIndicatorPipeline * {
    return m_mode_indicator_pipeline.get();
  }

  void enable_depth_test(bool enable) {
    if (enable) {
      glEnable(GL_DEPTH_TEST);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
  }
  void set_depth_func(GLenum func) { glDepthFunc(func); }
  void set_depth_mask(bool write) { glDepthMask(write ? GL_TRUE : GL_FALSE); }

  void enable_blend(bool enable) {
    if (enable) {
      glEnable(GL_BLEND);
    } else {
      glDisable(GL_BLEND);
    }
  }
  void set_blend_func(GLenum src, GLenum dst) { glBlendFunc(src, dst); }

  void enable_polygon_offset(bool enable) {
    if (enable) {
      glEnable(GL_POLYGON_OFFSET_FILL);
    } else {
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
  }
  void set_polygon_offset(float factor, float units) {
    glPolygonOffset(factor, units);
  }

  void set_frame_budget(const Render::FrameBudgetConfig &config) override {
    m_frame_budget_config = config;
  }
  [[nodiscard]] auto
  frame_tracker() const -> const Render::FrameTimeTracker * override {
    return &m_frame_tracker;
  }

private:
  int m_viewport_width{0};
  int m_viewport_height{0};
  std::array<float, 4> m_clear_color{0.65F, 0.69F, 0.67F, 1.0F};
  std::unique_ptr<ShaderCache> m_shader_cache;
  std::unique_ptr<ResourceManager> m_resources;
  std::unique_ptr<BackendPipelines::CylinderPipeline> m_cylinder_pipeline;
  std::unique_ptr<BackendPipelines::VegetationPipeline> m_vegetation_pipeline;
  std::unique_ptr<BackendPipelines::TerrainPipeline> m_terrain_pipeline;
  std::unique_ptr<BackendPipelines::CharacterPipeline> m_character_pipeline;
  std::unique_ptr<BackendPipelines::RiggedCharacterPipeline>
      m_rigged_character_pipeline;
  std::unique_ptr<BackendPipelines::WaterPipeline> m_water_pipeline;
  std::unique_ptr<BackendPipelines::EffectsPipeline> m_effects_pipeline;
  std::unique_ptr<BackendPipelines::PrimitiveBatchPipeline>
      m_primitive_batch_pipeline;
  std::unique_ptr<BackendPipelines::BannerPipeline> m_banner_pipeline;
  std::unique_ptr<BackendPipelines::HealingBeamPipeline>
      m_healing_beam_pipeline;
  std::unique_ptr<BackendPipelines::HealerAuraPipeline> m_healer_aura_pipeline;
  std::unique_ptr<BackendPipelines::CombatDustPipeline> m_combat_dust_pipeline;
  std::unique_ptr<BackendPipelines::RainPipeline> m_rain_pipeline;
  std::unique_ptr<BackendPipelines::ModeIndicatorPipeline>
      m_mode_indicator_pipeline;
  std::unique_ptr<BackendPipelines::MeshInstancingPipeline>
      m_mesh_instancing_pipeline;

  Shader *m_basic_shader = nullptr;
  Shader *m_grid_shader = nullptr;
  Shader *m_shadow_shader = nullptr;

  Shader *m_last_bound_shader = nullptr;
  Texture *m_last_bound_texture = nullptr;
  bool m_depth_testEnabled = true;
  bool m_blend_enabled = false;
  float m_animation_time = 0.0F;
  GLuint m_frame_ubo{0};

  Render::FrameBudgetConfig m_frame_budget_config;
  Render::FrameTimeTracker m_frame_tracker;
  ShaderQuality m_shader_quality{ShaderQuality::Full};
};

} // namespace Render::GL
