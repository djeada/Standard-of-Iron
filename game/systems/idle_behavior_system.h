#pragma once

#include "../core/system.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

/// System that manages idle behavior animations for units
/// Implements a layered idle behavior system with:
/// - Micro idles: Always-on subtle movements (breathing, weight shift)
/// - Ambient idles: Occasional personality-driven actions
/// - Group idles: Rare contextual interactions between nearby units
class IdleBehaviorSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;
};

} // namespace Game::Systems
