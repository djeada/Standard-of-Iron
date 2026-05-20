#pragma once

#include "../../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::RpgCombat {

struct CommanderDamageResult {
  int effective_damage{0};
  bool blocked{false};
  bool perfect_guarded{false};
  bool guard_broken{false};
  bool killed{false};
};

CommanderDamageResult deal_commander_attack_damage(Engine::Core::World* world,
                                                   Engine::Core::Entity* target,
                                                   int raw_damage,
                                                   Engine::Core::EntityID commander_id);

CommanderDamageResult deal_damage_to_rpg_commander(Engine::Core::World* world,
                                                   Engine::Core::Entity* commander,
                                                   int raw_damage,
                                                   Engine::Core::EntityID attacker_id);

} // namespace Game::Systems::RpgCombat
