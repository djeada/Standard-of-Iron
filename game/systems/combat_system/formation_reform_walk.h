#pragma once

#include "../../core/component.h"

namespace Game::Systems::Combat {

auto walk_to_new_slot(Engine::Core::SquadReformComponent& reform,
                      const Engine::Core::TransformComponent& actor,
                      float squad_speed,
                      float delta_time,
                      const Engine::Core::FormationSoldierPresentation* previous,
                      Engine::Core::FormationSoldierPresentation& directive) -> bool;

} // namespace Game::Systems::Combat
