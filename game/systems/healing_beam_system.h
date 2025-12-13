#pragma once

#include "../core/i_system.h"
#include "healing_beam.h"
#include <QVector3D>
#include <memory>
#include <vector>

namespace Engine::Core {
class World;
}

namespace Game::Systems {

/**
 * @brief System for managing magical healing beam visual effects.
 *
 * This system handles the lifecycle of healing beams - spawning them
 * when healing occurs and updating their animation progress.
 * The actual rendering is done by HealingBeamRenderer.
 */
class HealingBeamSystem : public Engine::Core::ISystem {
public:
  HealingBeamSystem() = default;
  ~HealingBeamSystem() override = default;

  void update(Engine::Core::World *world, float delta_time) override;

  /**
   * @brief Spawn a new healing beam from healer to target.
   *
   * @param healer_pos Position of the healer (world space)
   * @param target_pos Position of the heal target (world space)
   * @param color Beam color (default: golden-green)
   * @param duration How long the beam lasts
   */
  void spawn_beam(const QVector3D &healer_pos, const QVector3D &target_pos,
                  const QVector3D &color = QVector3D(0.3F, 1.0F, 0.5F),
                  float duration = 0.8F);

  /**
   * @brief Get all active healing beams for rendering.
   */
  auto get_beams() const -> const std::vector<std::unique_ptr<HealingBeam>> & {
    return m_beams;
  }

  /**
   * @brief Get number of active beams.
   */
  auto get_beam_count() const -> size_t { return m_beams.size(); }

  /**
   * @brief Clear all beams (e.g., on scene change).
   */
  void clear() { m_beams.clear(); }

private:
  std::vector<std::unique_ptr<HealingBeam>> m_beams;
};

} // namespace Game::Systems
