#pragma once

#include "../core/system.h"

namespace Game::Systems {

class ResourceDeliverySystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;
};

} // namespace Game::Systems
