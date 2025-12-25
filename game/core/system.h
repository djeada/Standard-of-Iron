#pragma once

#include <memory>

namespace Engine::Core {

class World;

class System {
public:
  System() = default;
  System(const System &) = default;
  System(System &&) noexcept = default;
  auto operator=(const System &) -> System & = default;
  auto operator=(System &&) noexcept -> System & = default;
  virtual ~System() = default;
  virtual void update(World *world, float delta_time) = 0;
};

} // namespace Engine::Core
