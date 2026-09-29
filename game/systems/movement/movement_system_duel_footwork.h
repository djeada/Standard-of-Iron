#pragma once

#include "movement_system_mover.h"

namespace Game::Systems {

class DuelFootwork {
public:
  void advance(float delta_time) { m_clock += delta_time; }

  [[nodiscard]] auto apply(Mover& mover,
                           Engine::Core::AttackComponent& attack) const -> bool;

private:
  float m_clock{0.0F};
};

} // namespace Game::Systems
