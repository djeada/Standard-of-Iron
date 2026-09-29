#pragma once

#include "movement_system.h"
#include "movement_system_duel_footwork.h"
#include "movement_system_mover.h"

namespace Game::Systems {

class MovementSystem::Gates {
public:
  static void hold_direct_control(Mover& mover);

  static void step_hold_mode(Mover& mover);

  static void step_melee_lock(Mover& mover, const DuelFootwork& footwork);

  static void step_builder_bypass(Mover& mover);
};

} // namespace Game::Systems
