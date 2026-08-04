#pragma once

#include "../../core/entity.h"
#include "rpg_bow_aim.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::CombatActions {
struct CombatActionDefinition;
}

namespace Game::Systems::RpgCombat {

struct LoosedArrow {
  bool released{false};
  Engine::Core::EntityID target_id{0};
  std::uint16_t soldier_slot{
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot};
  int damage{0};
  float power{0.0F};
  BowShot shot;
};

auto loose_aimed_arrow(Engine::Core::World& world,
                       Engine::Core::Entity& commander,
                       const Game::Systems::CombatActions::CombatActionDefinition&
                           definition) -> LoosedArrow;

} // namespace Game::Systems::RpgCombat
