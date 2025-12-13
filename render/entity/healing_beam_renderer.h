#pragma once

#include "../gl/mesh.h"
#include "../gl/shader.h"
#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <memory>

namespace Game::Systems {
class HealingBeamSystem;
class HealingBeam;
} // namespace Game::Systems

namespace Render::GL {
class Backend;
class Camera;
class Renderer;
class ResourceManager;

/**
 * @brief Renderer for magical healing beam visual effects.
 *
 * Renders glowing, animated energy beams from healers to their targets.
 * Uses GPU shaders for impressive magical effects including:
 * - Curved arc path with bezier interpolation
 * - Spiral twisting animation
 * - Flowing energy particles
 * - Golden-green magical glow
 */
class HealingBeamRenderer : protected QOpenGLFunctions_3_3_Core {
public:
  HealingBeamRenderer();
  ~HealingBeamRenderer();

  HealingBeamRenderer(const HealingBeamRenderer &) = delete;
  auto operator=(const HealingBeamRenderer &) -> HealingBeamRenderer & = delete;
  HealingBeamRenderer(HealingBeamRenderer &&) = delete;
  auto operator=(HealingBeamRenderer &&) -> HealingBeamRenderer & = delete;

  /**
   * @brief Initialize OpenGL resources.
   * @param backend The rendering backend for shader access.
   * @return true if initialization succeeded.
   */
  auto initialize(Backend *backend) -> bool;

  /**
   * @brief Render all active healing beams.
   * @param beam_system The system containing active beams.
   * @param cam The camera for view/projection matrices.
   * @param animation_time Current animation time for shader effects.
   */
  void render(const Game::Systems::HealingBeamSystem *beam_system,
              const Camera &cam, float animation_time);

  /**
   * @brief Clean up OpenGL resources.
   */
  void shutdown();

private:
  void render_beam(const Game::Systems::HealingBeam &beam, const Camera &cam,
                   float animation_time);
  auto create_beam_mesh() -> std::unique_ptr<Mesh>;

  Backend *m_backend{nullptr};
  Shader *m_beam_shader{nullptr};
  std::unique_ptr<Mesh> m_beam_mesh;
  bool m_initialized{false};

  // Cached uniform handles
  Shader::UniformHandle m_u_mvp{Shader::InvalidUniform};
  Shader::UniformHandle m_u_model{Shader::InvalidUniform};
  Shader::UniformHandle m_u_time{Shader::InvalidUniform};
  Shader::UniformHandle m_u_progress{Shader::InvalidUniform};
  Shader::UniformHandle m_u_start_pos{Shader::InvalidUniform};
  Shader::UniformHandle m_u_end_pos{Shader::InvalidUniform};
  Shader::UniformHandle m_u_beam_width{Shader::InvalidUniform};
  Shader::UniformHandle m_u_heal_color{Shader::InvalidUniform};
  Shader::UniformHandle m_u_alpha{Shader::InvalidUniform};
};

/**
 * @brief Free function to render healing beams, similar to render_arrows.
 *
 * This function integrates with the scene rendering pipeline.
 */
void render_healing_beams(Renderer *renderer, ResourceManager *resources,
                          const Game::Systems::HealingBeamSystem &beam_system);

} // namespace Render::GL
