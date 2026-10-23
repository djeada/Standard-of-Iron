#pragma once

namespace Engine::Core {
class Entity;
} // namespace Engine::Core

namespace Game::Systems {
class ProjectileSystem;
}

namespace Game::Systems::Combat {

[[nodiscard]] auto volley_damage_on_target(const Engine::Core::Entity& attacker,
                                           const Engine::Core::Entity& target,
                                           int damage) -> int;

void spawn_rts_arrow_volley(Engine::Core::Entity* attacker,
                            Engine::Core::Entity* target,
                            ProjectileSystem* projectile_sys,
                            int damage);

} // namespace Game::Systems::Combat
