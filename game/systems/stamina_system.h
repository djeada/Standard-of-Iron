#pragma once

#include "../core/system.h"

namespace Engine::Core {
class Entity;
class StaminaComponent;
class World;
} // namespace Engine::Core

namespace Game::Systems {

class StaminaSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;
};

[[nodiscard]] auto
ensure_run_stamina(Engine::Core::Entity& entity) -> Engine::Core::StaminaComponent*;

} // namespace Game::Systems
