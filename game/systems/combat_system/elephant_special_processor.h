#pragma once

namespace Engine::Core {
class ElephantKnockbackCooldownComponent;
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Systems::Combat {

struct CombatQueryContext;

[[nodiscard]] auto apply_elephant_stomp_impact(Engine::Core::World* world,
                                               Engine::Core::Entity* elephant) -> bool;

void expire_knockback_cooldowns(
    Engine::Core::ElephantKnockbackCooldownComponent& cooldowns, float delta_time);

void process_elephant_specials(Engine::Core::World* world,
                               const CombatQueryContext& query_context,
                               float delta_time);

} // namespace Game::Systems::Combat
