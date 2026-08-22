#pragma once

#include "../core/system.h"

namespace Engine::Core {
class SystemContext;
class World;
} // namespace Engine::Core

namespace Game::Systems {

class HealingSystem : public Engine::Core::System {
public:
  void run(Engine::Core::SystemContext& context) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;

private:
  static void process_healing(Engine::Core::SystemContext& context);
};

} // namespace Game::Systems
