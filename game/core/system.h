#pragma once

#include <memory>

namespace Engine::Core {

class World;

class System {
public:
  virtual ~System() = default;
  virtual void update(World *world, float deltaTime) = 0;
};

} // namespace Engine::Core
