#pragma once

namespace Engine::Core {
class World;
class Entity;
} // namespace Engine::Core

namespace Game::Systems::Combat {

[[nodiscard]] auto
commander_link_still_swinging(Engine::Core::Entity* attacker) -> bool;

void begin_rts_melee_action(Engine::Core::World& world,
                            Engine::Core::Entity* attacker,
                            Engine::Core::Entity* target,
                            int damage);

void begin_rts_bow_action(Engine::Core::World& world,
                          Engine::Core::Entity* attacker,
                          Engine::Core::Entity* target,
                          int damage,
                          float duration);

[[nodiscard]] auto resolve_melee_swing_cadence(Engine::Core::Entity* attacker,
                                               Engine::Core::Entity* target,
                                               float cooldown) -> float;

} // namespace Game::Systems::Combat
