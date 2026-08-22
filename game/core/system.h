#pragma once

#include <memory>

#include "system_schedule.h"

namespace Engine::Core {

class SystemContext;
class World;

class System {
public:
  System() = default;
  System(const System&) = default;
  System(System&&) noexcept = default;
  auto operator=(const System&) -> System& = default;
  auto operator=(System&&) noexcept -> System& = default;
  virtual ~System() = default;

  virtual void update(World* world, float delta_time);

  virtual void run(SystemContext& context);

  [[nodiscard]] virtual auto phase() const -> SystemPhase {
    return SystemPhase::Combat;
  }

  [[nodiscard]] virtual auto access() const -> SystemAccess {
    return SystemAccess::everything();
  }
};

} // namespace Engine::Core
