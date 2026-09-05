#pragma once

#include "../core/system.h"
#include "recruitment_rules.h"

namespace Game::Systems {

class CivilianDeliverySystem : public Engine::Core::System {
public:
  void update(Engine::Core::World* world, float delta_time) override;
};

} // namespace Game::Systems
