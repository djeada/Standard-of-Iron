#pragma once

namespace Engine::Core {
class CombatStateComponent;
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Systems::Combat {

void process_authored_combat_action(
    Engine::Core::World* world,
    Engine::Core::Entity& entity,
    Engine::Core::CombatStateComponent* presentation_state,
    float delta_time);

}
