#pragma once

#include "../core/system.h"

namespace Engine::Core {
class SystemContext;
} // namespace Engine::Core

namespace Game::Systems {

class PatrolSystem : public Engine::Core::System {
public:
  PatrolSystem() = default;
  ~PatrolSystem() override = default;

  void run(Engine::Core::SystemContext& context) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;
};

} // namespace Game::Systems
