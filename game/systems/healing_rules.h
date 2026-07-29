#pragma once

namespace Engine::Core {
class Entity;
}

namespace Game::Systems::HealingRules {

[[nodiscard]] auto
maximum_recoverable_health(const Engine::Core::Entity& target) -> int;

[[nodiscard]] auto can_receive_healing(const Engine::Core::Entity& target) -> bool;

} // namespace Game::Systems::HealingRules
