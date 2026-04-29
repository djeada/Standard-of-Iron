#pragma once

namespace Engine::Core {
class World;
}

namespace Game::Systems::Combat {

struct CombatQueryContext;

void process_attacks(Engine::Core::World *world,
                     const CombatQueryContext &query_context, float delta_time);

} // namespace Game::Systems::Combat
