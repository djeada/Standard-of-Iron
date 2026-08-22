#include "formation_move_dispatch_system.h"

#include "../core/component.h"
#include "../formation/army_formation_registry.h"
#include "command_service.h"

namespace Game::Systems {

void FormationMoveDispatchSystem::update(Engine::Core::World* world, float) {
  if (world == nullptr) {
    return;
  }
  auto& registry = Game::Formation::ArmyFormationRegistry::instance();
  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr || !formation->moves_pending) {
      continue;
    }
    formation->moves_pending = false;
    for (const auto& slot : formation->slot_list) {
      if (slot.occupant == 0U || slot.status == Game::Formation::SlotStatus::Blocked) {
        continue;
      }
      CommandService::move_unit(*world, slot.occupant, slot.world_position);
    }
  }
}

auto FormationMoveDispatchSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<UnitComponent, BuildingComponent, PendingRemovalComponent>{},
      Writes<MovementComponent, TransformComponent, AttackComponent>{});
}

} // namespace Game::Systems
