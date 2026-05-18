#include "civilian_delivery_system.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "building_collision_registry.h"
#include "units/spawn_type.h"

namespace Game::Systems {
namespace {

auto barracks_delivery_clearance() -> float {
  float const unit_radius =
      std::max(Game::Units::TroopConfig::instance().get_selection_ring_size(
                   Game::Units::SpawnType::Civilian) *
                   0.5F,
               0.5F);
  return BuildingCollisionRegistry::get_grid_padding() + unit_radius + 0.75F;
}

auto is_at_barracks_delivery_edge(
    const Engine::Core::TransformComponent& civilian_transform,
    const Engine::Core::TransformComponent& barracks_transform) -> bool {
  auto const size = BuildingCollisionRegistry::get_building_size("barracks");
  float const half_width = size.width * 0.5F;
  float const half_depth = size.depth * 0.5F;
  float const edge_dx =
      std::max(std::fabs(civilian_transform.position.x -
                         barracks_transform.position.x) -
                   half_width,
               0.0F);
  float const edge_dz =
      std::max(std::fabs(civilian_transform.position.z -
                         barracks_transform.position.z) -
                   half_depth,
               0.0F);

  return std::max(edge_dx, edge_dz) <= barracks_delivery_clearance();
}

} // namespace

void CivilianDeliverySystem::update(Engine::Core::World* world, float) {
  if (world == nullptr) {
    return;
  }

  std::vector<Engine::Core::EntityID> to_remove;
  auto civilians = world->get_entities_with<Engine::Core::CivilianDeliveryComponent>();

  for (auto* civilian_entity : civilians) {
    if (civilian_entity == nullptr) {
      continue;
    }

    auto* delivery =
        civilian_entity->get_component<Engine::Core::CivilianDeliveryComponent>();
    auto* civilian_unit = civilian_entity->get_component<Engine::Core::UnitComponent>();
    auto* civilian_transform =
        civilian_entity->get_component<Engine::Core::TransformComponent>();

    if ((delivery == nullptr) || (civilian_unit == nullptr) ||
        (civilian_transform == nullptr) ||
        (civilian_unit->spawn_type != Game::Units::SpawnType::Civilian) ||
        (delivery->target_barracks_id == 0)) {
      civilian_entity->remove_component<Engine::Core::CivilianDeliveryComponent>();
      continue;
    }

    auto* barracks_entity = world->get_entity(delivery->target_barracks_id);
    auto* barracks_unit =
        barracks_entity ? barracks_entity->get_component<Engine::Core::UnitComponent>()
                        : nullptr;
    auto* barracks_transform =
        barracks_entity
            ? barracks_entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    auto* barracks_prod =
        barracks_entity
            ? barracks_entity->get_component<Engine::Core::ProductionComponent>()
            : nullptr;

    if ((barracks_entity == nullptr) || (barracks_unit == nullptr) ||
        (barracks_transform == nullptr) || (barracks_prod == nullptr) ||
        (barracks_unit->spawn_type != Game::Units::SpawnType::Barracks) ||
        (barracks_unit->owner_id != civilian_unit->owner_id)) {
      civilian_entity->remove_component<Engine::Core::CivilianDeliveryComponent>();
      continue;
    }

    if (!is_at_barracks_delivery_edge(*civilian_transform, *barracks_transform)) {
      continue;
    }

    if (barracks_prod->manpower_available +
            k_civilian_delivery_population_grant >
        barracks_prod->max_units) {
      civilian_entity->remove_component<Engine::Core::CivilianDeliveryComponent>();
      continue;
    }

    barracks_prod->manpower_available += k_civilian_delivery_population_grant;
    to_remove.push_back(civilian_entity->get_id());
  }

  for (auto const id : to_remove) {
    world->destroy_entity(id);
  }
}

} // namespace Game::Systems
