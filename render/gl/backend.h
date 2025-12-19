#pragma once

#include "../draw_queue.h"
#include "../ground/grass_gpu.h"
#include "../ground/plant_gpu.h"
#include "../ground/stone_gpu.h"
#include "../ground/terrain_gpu.h"
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
class WaterPipeline;
class EffectsPipeline;
class PrimitiveBatchPipeline;
class BannerPipeline;
class HealingBeamPipeline;
class HealerAuraPipeline;
class CombatDustPipeline;
class RainPipeline;
class ModeIndicatorPipeline;
} // namespace Render::GL::BackendPipelines

namespace Render::GL {

class Backend : protected QOpenGLFunctions_3_3_Core {
public:
  friend class BackendPipelines::CylinderPipeline;
  friend class BackendPipelines::VegetationPipeline;

  Backend();
  ~Backend() override;

  Backend(const Backend &) = delete;
  auto operator=(const Backend &) -> Backend & = delete;
  Backend(Backend &&) = delete;
  auto operator=(Backend &&) -> Backend & = delete;

  void initialize();
  void begin_frame();
  void setViewport(int w, int h);
  void set_clear_color(float r, float g, float b, float a);
  void set_animation_time(float time) { m_animationTime = time; }
  void execute(const DrawQueue &queue, const Camera &cam);

  [[nodiscard]] auto resources() const -> ResourceManager * {
    return m_resources.get();
  }

  [[nodiscard]] auto shader(const QString &name) const -> Shader * {
    return m_shaderCache ? m_shaderCache->get(name) : nullptr;
  }
  auto get_or_load_shader(const QString &name, const QString &vertPath,
                          const QString &fragPath) -> Shader * {
    if (!m_shaderCache) {
      return nullptr;
    }
    return m_shaderCache->load(name, vertPath, fragPath);
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

  void set_riverbank_visibility(bool enabled, Texture *texture,
                              const QVector2D &size, float tile_size,
                              float explored_alpha) {
    m_riverbankVisibility.enabled = enabled && (texture != nullptr);
    m_riverbankVisibility.texture = texture;
    m_riverbankVisibility.size = size;
    m_riverbankVisibility.tile_size = tile_size;
    m_riverbankVisibility.explored_alpha = explored_alpha;
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

  Shader *m_basicShader = nullptr;
  Shader *m_gridShader = nullptr;

  Shader *m_lastBoundShader = nullptr;
  Texture *m_lastBoundTexture = nullptr;
  bool m_depth_testEnabled = true;
  bool m_blendEnabled = false;
  float m_animationTime = 0.0F;

  struct {
    Texture *texture = nullptr;
    QVector2D size{0.0F, 0.0F};
    float tile_size = 1.0F;
    float explored_alpha = 0.6F;
    bool enabled = false;
  } m_riverbankVisibility;
};

} // namespace Render::GL
