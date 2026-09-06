#include "formation_move_dispatch_system.h"

#include "../core/component_gameplay.h"
#include "../formation/army_formation_registry.h"
#include "command_service.h"

namespace Game::Systems {

void FormationMoveDispatchSystem::update(Engine::Core::World* world, float) {
  if (world == nullptr) {
    return;
  }
  auto& registry = Game::Formation::ArmyFormationRegistry::for_world(*world);
  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr || !formation->moves_pending) {
      continue;
    }
    formation->moves_pending = false;
    std::vector<CommandService::MoveIntent> intents;
    intents.reserve(formation->slot_list.size());
    for (const auto& slot : formation->slot_list) {
      if (slot.occupant == 0U || slot.status == Game::Formation::SlotStatus::Blocked) {
        continue;
      }
      intents.push_back({.unit_id = slot.occupant,
                         .target = slot.world_position,
                         .facing_angle = slot.facing});
    }
    CommandService::move_units(
        *world,
        intents,
        {.kind = MoveOrderKind::FormationMove, .preserve_formation_mode = true});
  }
}

auto FormationMoveDispatchSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<UnitComponent, BuildingComponent, PendingRemovalComponent>{},
      Writes<MovementComponent, TransformComponent, AttackComponent>{});
}

} // namespace Game::Systems
