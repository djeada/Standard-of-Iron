#include "resource_delivery_system.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "command_service.h"
#include "player_resource_registry.h"
#include "resource_stockpile.h"
#include "resource_types.h"
#include "units/spawn_type.h"

namespace Game::Systems {
namespace {

constexpr float k_haul_repath_interval = 1.5F;
constexpr float k_stockpile_fill_smoothing = 3.0F;
constexpr const char* k_deposit_cue = "build.placement_confirmed";

auto is_live_depot(Engine::Core::Entity* entity, int owner_id) -> bool {
  if (entity == nullptr) {
    return false;
  }
  if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
    return false;
  }
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->spawn_type != Game::Units::SpawnType::Barracks) {
    return false;
  }
  if (unit->owner_id != owner_id || unit->health <= 0) {
    return false;
  }
  return entity->get_component<Engine::Core::TransformComponent>() != nullptr;
}

auto find_nearest_depot(Engine::Core::World* world,
                        int owner_id,
                        float from_x,
                        float from_z) -> Engine::Core::Entity* {
  Engine::Core::Entity* nearest = nullptr;
  float nearest_dist_sq = std::numeric_limits<float>::max();

  for (auto* candidate : world->get_entities_with<Engine::Core::UnitComponent>()) {
    if (!is_live_depot(candidate, owner_id)) {
      continue;
    }
    const auto* transform =
        candidate->get_component<Engine::Core::TransformComponent>();
    float const dx = transform->position.x - from_x;
    float const dz = transform->position.z - from_z;
    float const dist_sq = (dx * dx) + (dz * dz);
    if (dist_sq < nearest_dist_sq) {
      nearest_dist_sq = dist_sq;
      nearest = candidate;
    }
  }

  return nearest;
}

auto drop_point_for(const Engine::Core::TransformComponent& depot) -> StockpilePoint {
  return stockpile_drop_point(depot.position.x, depot.position.z, depot.rotation.y);
}

void credit_load(int owner_id, const Engine::Core::ResourceCarryComponent& carry) {
  for (ResourceType const type : k_all_resource_types) {
    int const amount = carry.amounts.get(type);
    if (amount > 0) {
      PlayerResourceRegistry::instance().add_harvested(owner_id, type, amount);
    }
  }
}

void approach_fill(float& current, float target, float delta_time) {
  float const step = std::clamp(k_stockpile_fill_smoothing * delta_time, 0.0F, 1.0F);
  current += (target - current) * step;
  if (std::fabs(target - current) < 0.001F) {
    current = target;
  }
}

void sync_stockpile_displays(Engine::Core::World* world, float delta_time) {
  auto& resources = PlayerResourceRegistry::instance();

  for (auto* entity : world->get_entities_with<Engine::Core::UnitComponent>()) {
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->spawn_type != Game::Units::SpawnType::Barracks) {
      continue;
    }

    auto* stockpile = entity->get_component<Engine::Core::StockpileComponent>();
    if (stockpile == nullptr) {
      stockpile = entity->add_component<Engine::Core::StockpileComponent>();
      if (stockpile == nullptr) {
        continue;
      }
    }

    bool const owned = !Game::Core::is_neutral_owner(unit->owner_id);
    float const wood_target =
        owned ? stockpile_fill_ratio(resources.get(unit->owner_id, ResourceType::Wood))
              : 0.0F;
    float const stone_target =
        owned ? stockpile_fill_ratio(resources.get(unit->owner_id, ResourceType::Stone))
              : 0.0F;
    float const iron_target =
        owned ? stockpile_fill_ratio(resources.get(unit->owner_id, ResourceType::Iron))
              : 0.0F;

    approach_fill(stockpile->wood_fill, wood_target, delta_time);
    approach_fill(stockpile->stone_fill, stone_target, delta_time);
    approach_fill(stockpile->iron_fill, iron_target, delta_time);

    if (stockpile->deposit_flash > 0.0F) {
      stockpile->deposit_flash = std::max(0.0F, stockpile->deposit_flash - delta_time);
    }
  }
}

auto hauler_is_free_to_walk(const Engine::Core::Entity& hauler) -> bool {
  const auto* builder =
      hauler.get_component<Engine::Core::BuilderProductionComponent>();
  if (builder != nullptr &&
      (builder->in_progress || builder->has_construction_site ||
       builder->has_task_target || builder->structure_task_entity_id != 0 ||
       !builder->queued_construction_site_ids.empty())) {
    return false;
  }
  if (hauler.get_component<Engine::Core::AttackTargetComponent>() != nullptr) {
    return false;
  }
  const auto* movement = hauler.get_component<Engine::Core::MovementComponent>();
  return movement != nullptr && !movement->get_has_target();
}

} // namespace

void ResourceDeliverySystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  sync_stockpile_displays(world, delta_time);

  std::vector<Engine::Core::EntityID> unloaded;
  auto haulers = world->get_entities_with<Engine::Core::ResourceCarryComponent>();

  for (auto* hauler : haulers) {
    if (hauler == nullptr) {
      continue;
    }

    auto* carry = hauler->get_component<Engine::Core::ResourceCarryComponent>();
    const auto* unit = hauler->get_component<Engine::Core::UnitComponent>();
    const auto* transform = hauler->get_component<Engine::Core::TransformComponent>();
    if (carry == nullptr || unit == nullptr || transform == nullptr || carry->empty()) {
      unloaded.push_back(hauler->get_id());
      continue;
    }

    auto* depot = world->get_entity(carry->depot_entity_id);
    if (!carry->has_depot || !is_live_depot(depot, unit->owner_id)) {
      depot = find_nearest_depot(
          world, unit->owner_id, transform->position.x, transform->position.z);
      if (depot == nullptr) {

        credit_load(unit->owner_id, *carry);
        unloaded.push_back(hauler->get_id());
        continue;
      }
      carry->depot_entity_id = depot->get_id();
      carry->has_depot = true;
      carry->haul_repath_cooldown = 0.0F;
    }

    const auto* depot_transform =
        depot->get_component<Engine::Core::TransformComponent>();
    auto const drop = drop_point_for(*depot_transform);
    carry->depot_x = drop.x;
    carry->depot_z = drop.z;

    float const dx = drop.x - transform->position.x;
    float const dz = drop.z - transform->position.z;
    float const dist_sq = (dx * dx) + (dz * dz);

    float const depot_dx = depot_transform->position.x - transform->position.x;
    float const depot_dz = depot_transform->position.z - transform->position.z;
    float const depot_dist_sq = (depot_dx * depot_dx) + (depot_dz * depot_dz);

    carry->haul_seconds += delta_time;
    bool const out_of_patience =
        carry->haul_seconds >= k_stockpile_haul_patience_seconds;

    if (dist_sq <= k_stockpile_drop_radius * k_stockpile_drop_radius ||
        (out_of_patience && depot_dist_sq <= k_stockpile_depot_arrival_radius *
                                                 k_stockpile_depot_arrival_radius)) {
      credit_load(unit->owner_id, *carry);
      if (auto* stockpile = depot->get_component<Engine::Core::StockpileComponent>()) {
        stockpile->deposit_flash = k_stockpile_deposit_flash_seconds;
      }
      Engine::Core::EventManager::instance().publish(
          Engine::Core::AudioCueEvent(k_deposit_cue));
      unloaded.push_back(hauler->get_id());
      continue;
    }

    carry->haul_repath_cooldown =
        std::max(0.0F, carry->haul_repath_cooldown - delta_time);
    if (carry->haul_repath_cooldown > 0.0F || !hauler_is_free_to_walk(*hauler)) {
      continue;
    }

    QVector3D target = CommandService::snap_to_walkable_ground(
        QVector3D(drop.x, transform->position.y, drop.z));

    float const snap_dx = target.x() - drop.x;
    float const snap_dz = target.z() - drop.z;
    if (((snap_dx * snap_dx) + (snap_dz * snap_dz)) >
        (k_stockpile_drop_radius * k_stockpile_drop_radius)) {
      target = CommandService::snap_to_walkable_ground(
          QVector3D(depot_transform->position.x,
                    transform->position.y,
                    depot_transform->position.z));
    }
    CommandService::move_unit(
        *world, hauler->get_id(), target, {.kind = MoveOrderKind::ScriptedMove});
    carry->haul_repath_cooldown = k_haul_repath_interval;
  }

  for (auto const id : unloaded) {
    if (auto* entity = world->get_entity(id)) {
      entity->remove_component<Engine::Core::ResourceCarryComponent>();
    }
  }
}

} // namespace Game::Systems
