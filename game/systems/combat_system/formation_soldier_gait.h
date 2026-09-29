#pragma once

#include "../../core/component.h"

namespace Game::Systems::Combat {

[[nodiscard]] auto gait_run_speed(float march_speed) -> float;

void settle_soldier_gait(const Engine::Core::FormationSoldierPresentation* previous,
                         Engine::Core::FormationSoldierPresentation& soldier,
                         float dt,
                         float run_speed);

} // namespace Game::Systems::Combat
