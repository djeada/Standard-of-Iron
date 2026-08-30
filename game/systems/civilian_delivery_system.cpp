#include "civilian_delivery_system.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "building_collision_registry.h"
#include "player_feedback.h"
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

constexpr float k_delivery_settle_radius = 9.0F;

auto is_at_barracks_delivery_edge(
    const Engine::Core::TransformComponent& civilian_transform,
    const Engine::Core::TransformComponent& barracks_transform) -> bool {
  auto const size = BuildingCollisionRegistry::get_building_size("barracks");
  float const half_width = size.width * 0.5F;
  float const half_depth = size.depth * 0.5F;
  float const edge_dx = std::max(
      std::fabs(civilian_transform.position.x - barracks_transform.position.x) -
          half_width,
      0.0F);
  float const edge_dz = std::max(
      std::fabs(civilian_transform.position.z - barracks_transform.position.z) -
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
  std::vector<Engine::Core::EntityID> to_release;

  for (auto [civilian_id, delivery_ref] :
       world->view<Engine::Core::CivilianDeliveryComponent>()) {
    auto* delivery = &delivery_ref;
    const auto* civilian_unit =
        world->try_get<Engine::Core::UnitComponent>(civilian_id);
    const auto* civilian_transform =
        world->try_get<Engine::Core::TransformComponent>(civilian_id);

    if ((civilian_unit == nullptr) || (civilian_transform == nullptr) ||
        (civilian_unit->spawn_type != Game::Units::SpawnType::Civilian) ||
        (delivery->target_barracks_id == 0)) {
      to_release.push_back(civilian_id);
      continue;
    }

    const Engine::Core::EntityID barracks_id = delivery->target_barracks_id;
    const auto* barracks_unit =
        world->try_get<Engine::Core::UnitComponent>(barracks_id);
    const auto* barracks_transform =
        world->try_get<Engine::Core::TransformComponent>(barracks_id);
    auto* barracks_prod =
        world->try_get<Engine::Core::ProductionComponent>(barracks_id);

    if ((barracks_unit == nullptr) || (barracks_transform == nullptr) ||
        (barracks_prod == nullptr) ||
        !Game::Units::is_recruitment_building(barracks_unit->spawn_type) ||
        (barracks_unit->owner_id != civilian_unit->owner_id)) {
      to_release.push_back(civilian_id);
      continue;
    }

    const auto* civilian_movement =
        world->try_get<Engine::Core::MovementComponent>(civilian_id);
    const bool walk_is_over =
        civilian_movement == nullptr || !civilian_movement->get_has_target();
    const float to_barracks_x =
        civilian_transform->position.x - barracks_transform->position.x;
    const float to_barracks_z =
        civilian_transform->position.z - barracks_transform->position.z;
    const bool settled_beside_it =
        walk_is_over &&
        ((to_barracks_x * to_barracks_x) + (to_barracks_z * to_barracks_z)) <=
            (k_delivery_settle_radius * k_delivery_settle_radius);

    if (!is_at_barracks_delivery_edge(*civilian_transform, *barracks_transform) &&
        !settled_beside_it) {
      continue;
    }

    if (barracks_prod->manpower_available + k_civilian_delivery_reserve_grant >
        barracks_prod->max_units) {
      to_release.push_back(civilian_id);
      continue;
    }

    grant_manpower(barracks_unit->owner_id,
                   barracks_id,
                   *barracks_prod,
                   k_civilian_delivery_reserve_grant,
                   barracks_prod->max_units);
    to_remove.push_back(civilian_id);
  }

  for (auto const id : to_release) {
    world->remove<Engine::Core::CivilianDeliveryComponent>(id);
  }

  for (auto const id : to_remove) {
    world->destroy_entity(id);
  }
}

} // namespace Game::Systems
