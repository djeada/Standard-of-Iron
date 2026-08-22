#pragma once

#include "../core/system.h"

namespace Engine::Core {
class SystemContext;
class World;
} // namespace Engine::Core

namespace Game::Systems {

class ShowcaseRoutineSystem : public Engine::Core::System {
public:
  void run(Engine::Core::SystemContext& context) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;
};

} // namespace Game::Systems
