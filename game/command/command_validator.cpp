#include "command_validator.h"

#include <algorithm>

#include "../core/component.h"
#include "../core/world.h"
#include "../session/session_context.h"
#include "../systems/owner_registry.h"

namespace Game::Command {

namespace {

auto owners_for(Engine::Core::World& world) -> Game::Systems::OwnerRegistry& {
  if (auto* session = Game::Session::SessionContext::for_world(world)) {
    return session->owners();
  }
  return Game::Session::SessionContext::active().owners();
}

auto is_commandable(Engine::Core::World& world,
                    Engine::Core::EntityID id,
                    int owner_id) -> bool {
  auto* entity = world.get_entity(id);
  if (entity == nullptr) {
    return false;
  }
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && unit->health > 0 && unit->owner_id == owner_id;
}

auto filter_subjects(Engine::Core::World& world,
                     int owner_id,
                     std::vector<Engine::Core::EntityID>& units) -> bool {
  units.erase(std::remove_if(units.begin(),
                             units.end(),
                             [&](Engine::Core::EntityID id) {
                               return !is_commandable(world, id, owner_id);
                             }),
              units.end());
  return !units.empty();
}

auto building_owner(Engine::Core::World& world,
                    Engine::Core::EntityID id) -> const Engine::Core::UnitComponent* {
  auto* entity = world.get_entity(id);
  return entity != nullptr ? entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
}

auto validate_move(Engine::Core::World& world,
                   int owner_id,
                   Move& payload) -> Rejection {
  if (payload.targets.empty()) {
    return Rejection::MalformedPayload;
  }

  std::vector<Engine::Core::EntityID> kept_units;
  std::vector<QVector3D> kept_targets;
  std::vector<float> kept_facings;
  kept_units.reserve(payload.units.size());
  kept_targets.reserve(payload.units.size());
  kept_facings.reserve(payload.facing_angles.size());
  for (std::size_t i = 0; i < payload.units.size(); ++i) {
    if (!is_commandable(world, payload.units[i], owner_id)) {
      continue;
    }
    kept_units.push_back(payload.units[i]);
    kept_targets.push_back(i < payload.targets.size() ? payload.targets[i]
                                                      : payload.targets.back());
    if (i < payload.facing_angles.size()) {
      kept_facings.push_back(payload.facing_angles[i]);
    }
  }
  if (kept_units.empty()) {
    return Rejection::NoSubjects;
  }
  payload.units = std::move(kept_units);
  payload.targets = std::move(kept_targets);
  payload.facing_angles = std::move(kept_facings);
  return Rejection::None;
}

auto validate_attack(Engine::Core::World& world,
                     int owner_id,
                     AttackTarget& payload) -> Rejection {
  auto* target = world.get_entity(payload.target);
  if (target == nullptr) {
    return Rejection::DeadTarget;
  }
  const auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    return Rejection::DeadTarget;
  }
  if (!owners_for(world).are_enemies(owner_id, target_unit->owner_id)) {
    return Rejection::FriendlyTarget;
  }
  return filter_subjects(world, owner_id, payload.units) ? Rejection::None
                                                         : Rejection::NoSubjects;
}

auto validate_building_order(Engine::Core::World& world,
                             int owner_id,
                             Engine::Core::EntityID building) -> Rejection {
  const auto* unit = building_owner(world, building);
  if (unit == nullptr) {
    return Rejection::MissingBuilding;
  }
  if (unit->owner_id != owner_id) {
    return Rejection::NotOwnedBuilding;
  }
  return Rejection::None;
}

} // namespace

auto rejection_name(Rejection rejection) -> const char* {
  switch (rejection) {
  case Rejection::None:
    return "accepted";
  case Rejection::NoOwner:
    return "no-owner";
  case Rejection::NoSubjects:
    return "no-subjects";
  case Rejection::DeadTarget:
    return "dead-target";
  case Rejection::FriendlyTarget:
    return "friendly-target";
  case Rejection::MissingBuilding:
    return "missing-building";
  case Rejection::NotOwnedBuilding:
    return "not-owned-building";
  case Rejection::MalformedPayload:
    return "malformed-payload";
  }
  return "unknown";
}

auto validate(Engine::Core::World& world, const Command& command) -> Validation {
  Validation result;
  result.command = command;

  if (command.owner_id <= 0) {
    result.rejection = Rejection::NoOwner;
    return result;
  }

  const int owner_id = command.owner_id;

  result.rejection = std::visit(
      [&](auto& payload) -> Rejection {
        using T = std::decay_t<decltype(payload)>;

        if constexpr (std::is_same_v<T, Move>) {
          return validate_move(world, owner_id, payload);
        } else if constexpr (std::is_same_v<T, AttackTarget>) {
          return validate_attack(world, owner_id, payload);
        } else if constexpr (std::is_same_v<T, SetRallyPoint> ||
                             std::is_same_v<T, Produce>) {
          return validate_building_order(world, owner_id, payload.building);
        } else {
          return filter_subjects(world, owner_id, payload.units)
                     ? Rejection::None
                     : Rejection::NoSubjects;
        }
      },
      result.command.payload);

  return result;
}

} // namespace Game::Command
