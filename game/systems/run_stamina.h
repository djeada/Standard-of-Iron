#pragma once

namespace Engine::Core {
class Entity;
class StaminaComponent;
} // namespace Engine::Core

namespace Game::Systems {

[[nodiscard]] auto
ensure_run_stamina(Engine::Core::Entity& entity) -> Engine::Core::StaminaComponent*;

} // namespace Game::Systems
