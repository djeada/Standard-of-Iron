#include "movement_pipeline.h"

#include "../core/world.h"

namespace Game::Systems {

void MovementPipeline::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  m_route_follow.update(world, delta_time);
  m_avoidance.update(world, delta_time);
  m_motor.update(world, delta_time);
}

auto MovementPipeline::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<UnitComponent,
                                     BuildingComponent,
                                     CommanderComponent,
                                     ElephantComponent,
                                     RpgCommanderActionComponent,
                                     MovementIntentComponent,
                                     FormationModeComponent,
                                     PendingRemovalComponent>{},
                               Writes<MovementComponent,
                                      MovementFactsComponent,
                                      TransformComponent,
                                      AttackComponent,
                                      StaminaComponent,
                                      TerrainContextComponent,
                                      MotionPresentationComponent,
                                      GuardModeComponent,
                                      HoldModeComponent,
                                      BuilderProductionComponent>{});
}

} // namespace Game::Systems
