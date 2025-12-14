#pragma once

#include "../shader.h"
#include "pipeline_interface.h"
#include <GL/gl.h>
#include <QMatrix4x4>
#include <QVector3D>
#include <vector>

namespace Engine::Core {
class World;
}

namespace Render::GL {
class ShaderCache;
class Backend;
class Camera;

namespace BackendPipelines {

/**
 * @brief Data for rendering a single healer aura effect.
 */
struct HealerAuraData {
  QVector3D position;    // World position of healer
  float radius;          // Healing range / aura radius
  float intensity;       // Glow intensity (0-1)
  QVector3D color;       // Aura color
  bool is_active;        // Whether actively healing
};

/**
 * @brief Pipeline for rendering magical aura effects around healers.
 *
 * Renders glowing, animated dome/ring effects around healer units.
 * Shows their healing range and current healing state.
 */
class HealerAuraPipeline final : public IPipeline {
public:
  explicit HealerAuraPipeline(GL::Backend *backend,
                              GL::ShaderCache *shaderCache)
      : m_backend(backend), m_shaderCache(shaderCache) {}
  ~HealerAuraPipeline() override { shutdown(); }

  auto initialize() -> bool override;
  void shutdown() override;
  void cache_uniforms() override;
  [[nodiscard]] auto is_initialized() const -> bool override;

  /**
   * @brief Collect healer data from the world for rendering.
   */
  void collect_healers(Engine::Core::World *world);

  /**
   * @brief Render all collected healer auras.
   */
  void render(const Camera &cam, float animation_time);

  /**
   * @brief Render a single healer aura with given parameters.
   * Called from backend execute() when processing HealerAuraCmd.
   */
  void render_single_aura(const QVector3D &position, const QVector3D &color,
                          float radius, float intensity, float time,
                          const QMatrix4x4 &view_proj);

  /**
   * @brief Clear collected healer data.
   */
  void clear_data() { m_healerData.clear(); }

private:
  void render_aura(const HealerAuraData &data, const Camera &cam,
                   float animation_time);
  auto create_dome_geometry() -> bool;
  void shutdown_geometry();

  GL::Backend *m_backend = nullptr;
  GL::ShaderCache *m_shaderCache = nullptr;
  GL::Shader *m_auraShader = nullptr;

  // Geometry for dome/hemisphere
  GLuint m_vao = 0;
  GLuint m_vertexBuffer = 0;
  GLuint m_indexBuffer = 0;
  GLsizei m_indexCount = 0;

  // Collected healer data for this frame
  std::vector<HealerAuraData> m_healerData;

  // Cached uniform handles
  struct AuraUniforms {
    GL::Shader::UniformHandle mvp{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle model{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle time{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle auraRadius{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle intensity{GL::Shader::InvalidUniform};
    GL::Shader::UniformHandle auraColor{GL::Shader::InvalidUniform};
  };

  AuraUniforms m_uniforms;
};

} // namespace BackendPipelines
} // namespace Render::GL
