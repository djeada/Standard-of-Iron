#include "target_rules.h"

#include "../../core/ambient_session.h"
#include "../../core/component_core.h"
#include "../../core/component_economy.h"
#include "../../core/component_gameplay.h"
#include "../../core/component_structures.h"
#include "../../core/entity.h"
#include "../../core/world.h"
#include "../owner_registry.h"

namespace Game::Systems::Combat {

auto is_passive_wildlife_target(Engine::Core::Entity* target) -> bool {
  if (target == nullptr) {
    return false;
  }
  const auto* wildlife = target->get_component<Engine::Core::WildlifeComponent>();
  return (wildlife != nullptr) && !wildlife->is_hostile();
}

auto owners_are_hostile(const OwnerRegistry& owners, int owner_a, int owner_b) -> bool {
  return owner_a != owner_b && !owners.are_allies(owner_a, owner_b);
}

auto evaluate_target(Engine::Core::Entity* target,
                     bool owners_are_hostile,
                     TargetQuery query) -> TargetRefusal {
  if (target == nullptr) {
    return TargetRefusal::NoTarget;
  }
  if (target->has_component<Engine::Core::PendingRemovalComponent>()) {
    return TargetRefusal::NoTarget;
  }

  const auto* unit = target->get_component<Engine::Core::UnitComponent>();
  if ((unit == nullptr) || (unit->health <= 0)) {
    return TargetRefusal::NoTarget;
  }

  if (!owners_are_hostile) {
    return TargetRefusal::SelfOrAllied;
  }

  if (!query.allow_buildings &&
      target->has_component<Engine::Core::BuildingComponent>()) {
    return TargetRefusal::Structure;
  }

  if (query.intent == EngagementIntent::AutoAcquired &&
      is_passive_wildlife_target(target)) {
    return TargetRefusal::Passive;
  }

  return TargetRefusal::None;
}

auto evaluate_target(const OwnerRegistry& owners,
                     int attacker_owner_id,
                     Engine::Core::Entity* target,
                     TargetQuery query) -> TargetRefusal {
  const auto* unit = target != nullptr
                         ? target->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
  const bool hostile =
      unit != nullptr && owners_are_hostile(owners, attacker_owner_id, unit->owner_id);
  return evaluate_target(target, hostile, query);
}

auto evaluate_target(int attacker_owner_id,
                     Engine::Core::Entity* target,
                     TargetQuery query) -> TargetRefusal {
  return evaluate_target(OwnerRegistry::instance(), attacker_owner_id, target, query);
}

auto may_attack(const OwnerRegistry& owners,
                int attacker_owner_id,
                Engine::Core::Entity* target,
                TargetQuery query) -> bool {
  return evaluate_target(owners, attacker_owner_id, target, query) ==
         TargetRefusal::None;
}

auto may_attack(int attacker_owner_id,
                Engine::Core::Entity* target,
                TargetQuery query) -> bool {
  return evaluate_target(attacker_owner_id, target, query) == TargetRefusal::None;
}

auto may_attack(const Engine::Core::UnitComponent* attacker,
                Engine::Core::Entity* target,
                TargetQuery query) -> bool {
  return attacker != nullptr && may_attack(attacker->owner_id, target, query);
}

auto collect_hostile_contacts(const Engine::Core::World& world,
                              int owner_id) -> std::vector<Engine::Core::Entity*> {
  const auto& owners = *Game::Session::services_for(world).owners;
  std::vector<Engine::Core::Entity*> result;
  result.reserve(world.entity_count());
  world.for_each_entity([&](Engine::Core::Entity& entity) {
    if (evaluate_target(owners,
                        owner_id,
                        &entity,
                        {.intent = EngagementIntent::AutoAcquired,
                         .allow_buildings = true}) == TargetRefusal::None) {
      result.push_back(&entity);
    }
  });
  return result;
}

auto target_refusal_key(TargetRefusal refusal) -> std::string_view {
  switch (refusal) {
  case TargetRefusal::None:
    return "valid";
  case TargetRefusal::NoTarget:
    return "no_target";
  case TargetRefusal::SelfOrAllied:
    return "self_or_allied";
  case TargetRefusal::Passive:
    return "passive";
  case TargetRefusal::Structure:
    return "structure";
  }
  return "no_target";
}

} // namespace Game::Systems::Combat
