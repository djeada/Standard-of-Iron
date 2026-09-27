#include "formation_move_dispatch_system.h"

#include "../core/component_combat.h"
#include "../core/component_gameplay.h"
#include "../core/world.h"
#include "../formation/army_formation_registry.h"
#include "command_service.h"

namespace Game::Systems {

namespace {

auto is_fighting(Engine::Core::World& world, Engine::Core::EntityID id) -> bool {
  const auto* target = world.try_get<Engine::Core::AttackTargetComponent>(id);
  const auto* attack = world.try_get<Engine::Core::AttackComponent>(id);
  return (target != nullptr && target->target_id != 0U) ||
         (attack != nullptr && attack->in_melee_lock);
}

} // namespace

void FormationMoveDispatchSystem::update(Engine::Core::World* world, float) {
  if (world == nullptr) {
    return;
  }
  auto& registry = Game::Formation::ArmyFormationRegistry::for_world(*world);
  for (auto const id : registry.group_ids()) {
    auto* formation = registry.find(id);
    if (formation == nullptr) {
      continue;
    }
    if (!formation->stragglers.empty()) {
      std::vector<CommandService::MoveIntent> returns;
      for (auto const member : formation->stragglers) {
        if (const auto* slot = formation->find_slot_for(member)) {
          returns.push_back({.unit_id = member,
                             .target = slot->world_position,
                             .facing_angle = slot->facing});
        }
      }
      formation->stragglers.clear();
      CommandService::move_units(
          *world,
          returns,
          {.kind = MoveOrderKind::FormationMove, .preserve_formation_mode = true});
    }
    if (!formation->moves_pending) {
      continue;
    }
    formation->moves_pending = false;
    std::vector<CommandService::MoveIntent> intents;
    intents.reserve(formation->slot_list.size());
    for (const auto& slot : formation->slot_list) {
      if (slot.occupant == 0U || slot.status == Game::Formation::SlotStatus::Blocked ||
          !formation->has_member(slot.occupant) || is_fighting(*world, slot.occupant)) {
        continue;
      }
      auto const target = Game::Formation::ArmyFormationRuntime::morph_target(
          *formation, slot.occupant);
      intents.push_back({.unit_id = slot.occupant,
                         .target = target.value_or(slot.world_position),
                         .facing_angle = slot.facing});
    }
    CommandService::move_units(
        *world,
        intents,
        {.kind = MoveOrderKind::FormationMove,
         .preserve_formation_mode = true,
         .follow_formation_slots = formation->maintains_formation()});
  }
}

auto FormationMoveDispatchSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<UnitComponent,
            BuildingComponent,
            PendingRemovalComponent,
            AttackTargetComponent>{},
      Writes<MovementComponent, TransformComponent, AttackComponent>{});
}

} // namespace Game::Systems
