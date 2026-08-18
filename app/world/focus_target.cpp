#include "app/world/focus_target.h"

#include <QCoreApplication>

#include <algorithm>
#include <unordered_map>

#include "game/core/component.h"
#include "game/core/world.h"

namespace App::Core {

namespace {

auto attack_target_of(Engine::Core::World* world,
                      Engine::Core::EntityID id) -> Engine::Core::EntityID {
  auto* entity = world->get_entity(id);
  if (entity == nullptr) {
    return Engine::Core::NULL_ENTITY;
  }
  const auto* attack = entity->get_component<Engine::Core::AttackTargetComponent>();
  return attack != nullptr ? attack->target_id : Engine::Core::NULL_ENTITY;
}

} // namespace

auto resolve_focus_entity(Engine::Core::World* world,
                          const std::vector<Engine::Core::EntityID>& selection,
                          Engine::Core::EntityID inspected,
                          int local_owner_id) -> Engine::Core::EntityID {
  if (world == nullptr) {
    return Engine::Core::NULL_ENTITY;
  }
  if (selection.size() == 1) {
    auto* entity = world->get_entity(selection.front());
    if (entity != nullptr && entity->has_component<Engine::Core::BuildingComponent>()) {
      const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit != nullptr && unit->owner_id == local_owner_id) {
        return selection.front();
      }
    }
  }
  if (inspected != Engine::Core::NULL_ENTITY) {
    auto* entity = world->get_entity(inspected);
    const auto* unit = entity != nullptr
                           ? entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
    if (unit != nullptr && unit->health > 0) {
      return inspected;
    }
  }
  return Engine::Core::NULL_ENTITY;
}

auto primary_attack_target(Engine::Core::World* world,
                           const std::vector<Engine::Core::EntityID>& selection)
    -> Engine::Core::EntityID {
  if (world == nullptr || selection.empty()) {
    return Engine::Core::NULL_ENTITY;
  }
  std::unordered_map<Engine::Core::EntityID, int> votes;
  for (const auto id : selection) {
    const auto target = attack_target_of(world, id);
    if (target == Engine::Core::NULL_ENTITY) {
      continue;
    }
    auto* target_entity = world->get_entity(target);
    const auto* target_unit =
        target_entity != nullptr
            ? target_entity->get_component<Engine::Core::UnitComponent>()
            : nullptr;
    if (target_unit == nullptr || target_unit->health <= 0) {
      continue;
    }
    ++votes[target];
  }
  Engine::Core::EntityID best = Engine::Core::NULL_ENTITY;
  int best_votes = 0;
  for (const auto& [target, count] : votes) {
    if (count > best_votes || (count == best_votes && target < best)) {
      best = target;
      best_votes = count;
    }
  }
  return best;
}

auto count_units_attacking(Engine::Core::World* world,
                           Engine::Core::EntityID target,
                           int owner_id) -> int {
  if (world == nullptr || target == Engine::Core::NULL_ENTITY) {
    return 0;
  }
  int count = 0;
  for (auto* entity : world->get_entities_with<Engine::Core::AttackTargetComponent>()) {
    const auto* attack = entity->get_component<Engine::Core::AttackTargetComponent>();
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (attack == nullptr || unit == nullptr || unit->health <= 0) {
      continue;
    }
    if (attack->target_id == target && (owner_id == 0 || unit->owner_id == owner_id)) {
      ++count;
    }
  }
  return count;
}

auto count_selection_attacking(Engine::Core::World* world,
                               const std::vector<Engine::Core::EntityID>& selection,
                               Engine::Core::EntityID target) -> int {
  if (world == nullptr || target == Engine::Core::NULL_ENTITY) {
    return 0;
  }
  return static_cast<int>(
      std::count_if(selection.begin(), selection.end(), [&](Engine::Core::EntityID id) {
        return attack_target_of(world, id) == target;
      }));
}

auto count_enemies_attacking(Engine::Core::World* world,
                             Engine::Core::EntityID target,
                             int local_owner_id) -> int {
  if (world == nullptr || target == Engine::Core::NULL_ENTITY) {
    return 0;
  }
  int count = 0;
  for (auto* entity : world->get_entities_with<Engine::Core::AttackTargetComponent>()) {
    const auto* attack = entity->get_component<Engine::Core::AttackTargetComponent>();
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (attack == nullptr || unit == nullptr || unit->health <= 0) {
      continue;
    }
    if (attack->target_id == target && unit->owner_id != local_owner_id) {
      ++count;
    }
  }
  return count;
}

auto building_display_name(Game::Units::SpawnType type) -> QString {
  switch (type) {
  case Game::Units::SpawnType::Barracks:
    return QCoreApplication::translate("FocusTarget", "Barracks");
  case Game::Units::SpawnType::DefenseTower:
    return QCoreApplication::translate("FocusTarget", "Defense tower");
  case Game::Units::SpawnType::Home:
    return QCoreApplication::translate("FocusTarget", "Home");
  case Game::Units::SpawnType::WallSegment:
    return QCoreApplication::translate("FocusTarget", "Wall");
  case Game::Units::SpawnType::WallGate:
    return QCoreApplication::translate("FocusTarget", "Gate");
  case Game::Units::SpawnType::Marketplace:
    return QCoreApplication::translate("FocusTarget", "Marketplace");
  case Game::Units::SpawnType::Temple:
    return QCoreApplication::translate("FocusTarget", "Temple");
  case Game::Units::SpawnType::Farm:
    return QCoreApplication::translate("FocusTarget", "Farm");
  default:
    break;
  }
  return {};
}

auto focus_target_to_variant(const FocusTargetInfo& info) -> QVariantMap {
  QVariantMap map;
  map[QStringLiteral("valid")] = info.valid;
  map[QStringLiteral("id")] = QVariant::fromValue<qulonglong>(info.id);
  map[QStringLiteral("name")] = info.name;
  map[QStringLiteral("nation")] = info.nation;
  map[QStringLiteral("typeKey")] = info.type_key;
  map[QStringLiteral("ownerId")] = info.owner_id;
  map[QStringLiteral("isBuilding")] = info.is_building;
  map[QStringLiteral("isEnemy")] = info.is_enemy;
  map[QStringLiteral("isOwn")] = info.is_own;
  map[QStringLiteral("health")] = info.health;
  map[QStringLiteral("maxHealth")] = info.max_health;
  map[QStringLiteral("healthRatio")] = info.health_ratio;
  map[QStringLiteral("activity")] = info.activity;
  map[QStringLiteral("activityState")] = info.activity_state;
  map[QStringLiteral("attackedBySelection")] = info.attacked_by_selection;
  map[QStringLiteral("attackedByLocal")] = info.attacked_by_local;
  map[QStringLiteral("attackersIncoming")] = info.attackers_incoming;
  return map;
}

} // namespace App::Core
