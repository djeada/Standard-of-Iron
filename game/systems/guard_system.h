#pragma once

#include "../core/system.h"

namespace Engine::Core {
class SystemContext;
} // namespace Engine::Core

namespace Game::Systems {

class GuardSystem : public Engine::Core::System {
public:
  GuardSystem() = default;
  ~GuardSystem() override = default;

  void run(Engine::Core::SystemContext& context) override;

  [[nodiscard]] auto access() const -> Engine::Core::SystemAccess override;
};

} // namespace Game::Systems
