#include "nation_collapse_service.h"

#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "building_collision_registry.h"
#include "capture_system.h"
#include "wall_network_service.h"

namespace Game::Systems::NationCollapse {

namespace {

void tear_down_structure(Engine::Core::World& world, Engine::Core::Entity& entity) {
  BuildingCollisionRegistry::instance().unregister_building(entity.get_id());

  const bool was_wall =
      entity.get_component<Engine::Core::WallSegmentComponent>() != nullptr;

  if (auto* renderable = entity.get_component<Engine::Core::RenderableComponent>()) {
    renderable->visible = false;
  }
  entity.add_component<Engine::Core::PendingRemovalComponent>();

  if (was_wall) {
    WallNetworkService::refresh_world(world);
  }
}

void disband_troop(Engine::Core::Entity& entity, Engine::Core::UnitComponent& unit) {
  unit.health = 0;
  Engine::Core::get_or_add_component<Engine::Core::DeathAnimationComponent>(entity);
  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitDiedEvent(entity.get_id(), unit.owner_id, unit.spawn_type));
}

} // namespace

auto has_living_commander(Engine::Core::World& world, int owner_id) -> bool {
  for (auto* entity : world.collect_entities_with<Engine::Core::CommanderComponent>()) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0) {
      return true;
    }
  }
  return false;
}

auto collapse_owner(Engine::Core::World& world, int owner_id) -> bool {
  if (Game::Core::is_neutral_owner(owner_id)) {
    return false;
  }

  std::vector<Engine::Core::Entity*> owned;
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == owner_id) {
      owned.push_back(entity);
    }
  }

  bool collapsed_anything = false;
  for (auto* entity : owned) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->spawn_type == Game::Units::SpawnType::Barracks) {
      if (unit->health > 0) {
        CaptureSystem::transfer_barrack_ownership(
            &world, entity, Game::Core::NEUTRAL_OWNER_ID);
        collapsed_anything = true;
      }
      continue;
    }

    if (Game::Units::is_building_spawn(unit->spawn_type)) {
      tear_down_structure(world, *entity);
      collapsed_anything = true;
      continue;
    }

    if (Game::Units::is_troop_spawn(unit->spawn_type) && unit->health > 0) {
      disband_troop(*entity, *unit);
      collapsed_anything = true;
    }
  }

  return collapsed_anything;
}

} // namespace Game::Systems::NationCollapse
