#include "production_system.h"

#include "core/world.h"
#include "production_system_builder.h"
#include "production_system_site_ghosts.h"
#include "production_system_training.h"

namespace Game::Systems {

void ProductionSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  ProductionTasks::tick_completion_effects(*world, delta_time);
  ProductionTasks::advance_unit_training(*world, delta_time);
  ProductionTasks::advance_builders(*world, delta_time);
  ProductionTasks::sync_site_ghosts(*world);
}

} // namespace Game::Systems
