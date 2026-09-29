#pragma once

namespace Engine::Core {
class Entity;
} // namespace Engine::Core

namespace Game::Systems {
class ProjectileSystem;
}

namespace Game::Systems::Combat {

void spawn_rts_arrow_volley(Engine::Core::Entity* attacker,
                            Engine::Core::Entity* target,
                            ProjectileSystem* projectile_sys,
                            int damage);

} // namespace Game::Systems::Combat
