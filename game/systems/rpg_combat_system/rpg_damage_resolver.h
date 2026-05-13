#pragma once

#include "../../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::RpgCombat {

struct RpgDamageResult {
  int effective_damage{0};
  bool is_crit{false};
  bool killed{false};
};

RpgDamageResult resolve_rpg_damage(Engine::Core::World *world,
                                   Engine::Core::Entity *target, int raw_damage,
                                   Engine::Core::EntityID attacker_id);

} // namespace Game::Systems::RpgCombat
