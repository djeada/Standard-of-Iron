#include "civilian_delivery_system.h"

#include "../core/component.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "building_collision_registry.h"
#include "units/spawn_type.h"
#include <algorithm>
#include <vector>

namespace Game::Systems {
namespace {

auto barracks_delivery_radius() -> float {
  auto const size = BuildingCollisionRegistry::get_building_size("barracks");
  float const unit_radius =
      Game::Units::TroopConfig::instance().get_selection_ring_size(
          Game::Units::SpawnType::Civilian) *
      0.5F;
  return std::max(size.width, size.depth) * 0.5F +
         BuildingCollisionRegistry::get_grid_padding() + unit_radius + 0.5F;
}

} // namespace

void CivilianDeliverySystem::update(Engine::Core::World *world, float) {
  if (world == nullptr) {
    return;
  }

  std::vector<Engine::Core::EntityID> to_remove;
  auto civilians =
      world->get_entities_with<Engine::Core::CivilianDeliveryComponent>();

  for (auto *civilian_entity : civilians) {
    if (civilian_entity == nullptr) {
      continue;
    }

    auto *delivery =
        civilian_entity
            ->get_component<Engine::Core::CivilianDeliveryComponent>();
    auto *civilian_unit =
        civilian_entity->get_component<Engine::Core::UnitComponent>();
    auto *civilian_transform =
        civilian_entity->get_component<Engine::Core::TransformComponent>();

    if ((delivery == nullptr) || (civilian_unit == nullptr) ||
        (civilian_transform == nullptr) ||
        (civilian_unit->spawn_type != Game::Units::SpawnType::Civilian) ||
        (delivery->target_barracks_id == 0)) {
      civilian_entity
          ->remove_component<Engine::Core::CivilianDeliveryComponent>();
      continue;
    }

    auto *barracks_entity = world->get_entity(delivery->target_barracks_id);
    auto *barracks_unit =
        barracks_entity
            ? barracks_entity->get_component<Engine::Core::UnitComponent>()
            : nullptr;
    auto *barracks_transform =
        barracks_entity
            ? barracks_entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    auto *barracks_prod =
        barracks_entity
            ? barracks_entity
                  ->get_component<Engine::Core::ProductionComponent>()
            : nullptr;

    if ((barracks_entity == nullptr) || (barracks_unit == nullptr) ||
        (barracks_transform == nullptr) || (barracks_prod == nullptr) ||
        (barracks_unit->spawn_type != Game::Units::SpawnType::Barracks) ||
        (barracks_unit->owner_id != civilian_unit->owner_id)) {
      civilian_entity
          ->remove_component<Engine::Core::CivilianDeliveryComponent>();
      continue;
    }

    float const dx =
        civilian_transform->position.x - barracks_transform->position.x;
    float const dz =
        civilian_transform->position.z - barracks_transform->position.z;
    float const dist_sq = dx * dx + dz * dz;

    float const delivery_radius = barracks_delivery_radius();
    if (dist_sq > (delivery_radius * delivery_radius)) {
      continue;
    }

    int const manpower_value =
        Game::Units::TroopConfig::instance().get_production_cost(
            Game::Units::SpawnType::Civilian);
    barracks_prod->manpower_available += manpower_value;
    to_remove.push_back(civilian_entity->get_id());
  }

  for (auto const id : to_remove) {
    world->destroy_entity(id);
  }
}

} // namespace Game::Systems
