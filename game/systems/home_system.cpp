#include "home_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Game::Systems {

void HomeSystem::update(Engine::Core::World *world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  auto home_entities = world->get_entities_with<Engine::Core::HomeComponent>();
  auto barracks_entities =
      world->get_entities_with<Engine::Core::ProductionComponent>();

  for (auto *home_entity : home_entities) {
    auto *home_comp = home_entity->get_component<Engine::Core::HomeComponent>();
    auto *home_transform =
        home_entity->get_component<Engine::Core::TransformComponent>();
    auto *home_unit = home_entity->get_component<Engine::Core::UnitComponent>();

    if (home_comp == nullptr || home_transform == nullptr ||
        home_unit == nullptr) {
      continue;
    }

    home_comp->update_cooldown -= delta_time;
    if (home_comp->update_cooldown > 0.0F) {
      continue;
    }

    home_comp->update_cooldown = k_update_interval;

    float min_distance = std::numeric_limits<float>::max();
    Engine::Core::EntityID nearest_barracks = 0;

    for (auto *barracks_entity : barracks_entities) {
      auto *barracks_transform =
          barracks_entity->get_component<Engine::Core::TransformComponent>();
      auto *barracks_unit =
          barracks_entity->get_component<Engine::Core::UnitComponent>();

      if (barracks_transform == nullptr || barracks_unit == nullptr) {
        continue;
      }

      if (barracks_unit->spawn_type != Game::Units::SpawnType::Barracks) {
        continue;
      }

      if (barracks_unit->owner_id != home_unit->owner_id) {
        continue;
      }

      float dx = barracks_transform->position.x - home_transform->position.x;
      float dz = barracks_transform->position.z - home_transform->position.z;
      float distance = std::sqrt(dx * dx + dz * dz);

      if (distance < min_distance && distance <= k_max_search_radius) {
        min_distance = distance;
        nearest_barracks = barracks_entity->get_id();
      }
    }

    Engine::Core::EntityID old_barracks = home_comp->nearest_barracks_id;
    home_comp->nearest_barracks_id = nearest_barracks;

    if (old_barracks != 0 && old_barracks != nearest_barracks) {
      auto *old_barracks_entity = world->get_entity(old_barracks);
      if (old_barracks_entity != nullptr) {
        auto *prod_comp =
            old_barracks_entity
                ->get_component<Engine::Core::ProductionComponent>();
        if (prod_comp != nullptr) {

          prod_comp->max_units = std::max(
              0, prod_comp->max_units - home_comp->population_contribution);
        }
      }
    }

    if (nearest_barracks != 0) {
      auto *barracks_entity = world->get_entity(nearest_barracks);
      if (barracks_entity != nullptr) {
        auto *prod_comp =
            barracks_entity->get_component<Engine::Core::ProductionComponent>();
        if (prod_comp != nullptr) {
          if (old_barracks != nearest_barracks) {
            prod_comp->max_units += home_comp->population_contribution;
          }
        }
      }
    }
  }
}

} // namespace Game::Systems
