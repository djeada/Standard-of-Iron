#include "formation_move_dispatch_system.h"

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

} // namespace Game::Systems
