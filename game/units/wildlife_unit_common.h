#pragma once

#include "../wildlife/wildlife_species.h"
#include "troop_type.h"
#include "unit.h"

namespace Engine::Core {
class Entity;
}

namespace Game::Units {

struct WildlifeUnitComponents {
  Engine::Core::TransformComponent* transform{nullptr};
  Engine::Core::RenderableComponent* renderable{nullptr};
  Engine::Core::UnitComponent* unit{nullptr};
  Engine::Core::MovementComponent* movement{nullptr};
  Engine::Core::AttackComponent* attack{nullptr};
};

auto setup_wildlife_unit(Engine::Core::Entity& entity,
                         const SpawnParams& params,
                         Game::Wildlife::Species species,
                         TroopType troop_type) -> WildlifeUnitComponents;

} // namespace Game::Units
