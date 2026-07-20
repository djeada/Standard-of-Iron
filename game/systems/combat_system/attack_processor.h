#pragma once

#include "../../core/entity.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

struct CombatQueryContext;

void process_attacks(Engine::Core::World* world,
                     const CombatQueryContext& query_context,
                     float delta_time);

[[nodiscard]] bool release_rts_arrow_volley(Engine::Core::World& world,
                                            Engine::Core::Entity& attacker,
                                            Engine::Core::EntityID target_id,
                                            int damage);

} // namespace Game::Systems::Combat
