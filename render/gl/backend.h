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
    m_animationTime = time;
  }
  void execute(const DrawQueue &queue, const Camera &cam) override;

  [[nodiscard]] auto resources() const -> ResourceManager * override {
    return m_resources.get();
  }

  [[nodiscard]] auto shader(const QString &name) const -> Shader * override {
    return m_shaderCache ? m_shaderCache->get(name) : nullptr;
  }

  [[nodiscard]] auto supports_shaders() const -> bool override { return true; }
  [[nodiscard]] auto shader_quality() const -> ShaderQuality override {
    return m_shader_quality;
  }
  void set_shader_quality(ShaderQuality q) noexcept { m_shader_quality = q; }
  auto get_or_load_shader(const QString &name, const QString &vert_path,
                          const QString &frag_path) -> Shader * {
    if (!m_shaderCache) {
      return nullptr;
    }
    return m_shaderCache->load(name, vert_path, frag_path);
  }

  [[nodiscard]] auto banner_mesh() const -> Mesh *;

  [[nodiscard]] auto banner_shader() const -> Shader *;

  [[nodiscard]] auto
  healing_beam_pipeline() -> BackendPipelines::HealingBeamPipeline * {
    return m_healingBeamPipeline.get();
  }

  [[nodiscard]] auto
  healer_aura_pipeline() -> BackendPipelines::HealerAuraPipeline * {
    return m_healerAuraPipeline.get();
  }

  [[nodiscard]] auto
  combat_dust_pipeline() -> BackendPipelines::CombatDustPipeline * {
    return m_combatDustPipeline.get();
  }

  [[nodiscard]] auto rain_pipeline() -> BackendPipelines::RainPipeline * {
    return m_rainPipeline.get();
  }

  [[nodiscard]] auto
  mode_indicator_pipeline() -> BackendPipelines::ModeIndicatorPipeline * {
    return m_modeIndicatorPipeline.get();
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
  int m_viewportWidth{0};
  int m_viewportHeight{0};
  std::array<float, 4> m_clearColor{0.2F, 0.3F, 0.3F, 0.0F};
  std::unique_ptr<ShaderCache> m_shaderCache;
  std::unique_ptr<ResourceManager> m_resources;
  std::unique_ptr<BackendPipelines::CylinderPipeline> m_cylinderPipeline;
  std::unique_ptr<BackendPipelines::VegetationPipeline> m_vegetationPipeline;
  std::unique_ptr<BackendPipelines::TerrainPipeline> m_terrainPipeline;
  std::unique_ptr<BackendPipelines::CharacterPipeline> m_characterPipeline;
  std::unique_ptr<BackendPipelines::RiggedCharacterPipeline>
      m_riggedCharacterPipeline;
  std::unique_ptr<BackendPipelines::WaterPipeline> m_waterPipeline;
  std::unique_ptr<BackendPipelines::EffectsPipeline> m_effectsPipeline;
  std::unique_ptr<BackendPipelines::PrimitiveBatchPipeline>
      m_primitiveBatchPipeline;
  std::unique_ptr<BackendPipelines::BannerPipeline> m_bannerPipeline;
  std::unique_ptr<BackendPipelines::HealingBeamPipeline> m_healingBeamPipeline;
  std::unique_ptr<BackendPipelines::HealerAuraPipeline> m_healerAuraPipeline;
  std::unique_ptr<BackendPipelines::CombatDustPipeline> m_combatDustPipeline;
  std::unique_ptr<BackendPipelines::RainPipeline> m_rainPipeline;
  std::unique_ptr<BackendPipelines::ModeIndicatorPipeline>
      m_modeIndicatorPipeline;
  std::unique_ptr<BackendPipelines::MeshInstancingPipeline>
      m_meshInstancingPipeline;

  Shader *m_basicShader = nullptr;
  Shader *m_gridShader = nullptr;
  Shader *m_shadowShader = nullptr;

  Shader *m_lastBoundShader = nullptr;
  Texture *m_lastBoundTexture = nullptr;
  bool m_depth_testEnabled = true;
  bool m_blendEnabled = false;
  float m_animationTime = 0.0F;

  Render::FrameBudgetConfig m_frame_budget_config;
  Render::FrameTimeTracker m_frame_tracker;
  ShaderQuality m_shader_quality{ShaderQuality::Full};
};

} // namespace Render::GL
