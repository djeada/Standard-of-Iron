#include "home_system.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../core/component.h"
#include "../core/world.h"
#include "../units/spawn_type.h"

namespace Game::Systems {

namespace {

auto home_manpower_capacity(const Engine::Core::ProductionComponent* home_prod) -> int {
  if (home_prod == nullptr) {
    return 0;
  }

  int const committed_civilians = home_prod->produced_count +
                                  (home_prod->in_progress ? 1 : 0) +
                                  static_cast<int>(home_prod->production_queue.size());
  int const remaining_recruit_slots =
      std::max(0, home_prod->max_units - committed_civilians);
  int const civilian_cost = std::max(0, home_prod->villager_cost);
  return remaining_recruit_slots * civilian_cost;
}

} // namespace

void HomeSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  auto home_entities = world->get_entities_with<Engine::Core::HomeComponent>();
  auto barracks_entities =
      world->get_entities_with<Engine::Core::ProductionComponent>();

  for (auto* home_entity : home_entities) {
    auto* home_comp = home_entity->get_component<Engine::Core::HomeComponent>();
    auto* home_transform =
        home_entity->get_component<Engine::Core::TransformComponent>();
    auto* home_unit = home_entity->get_component<Engine::Core::UnitComponent>();
    auto* home_prod = home_entity->get_component<Engine::Core::ProductionComponent>();

    if (home_comp == nullptr || home_transform == nullptr || home_unit == nullptr) {
      continue;
    }

    home_comp->update_cooldown -= delta_time;
    home_comp->family_generation_cooldown -= delta_time;
    if (home_comp->update_cooldown > 0.0F) {
      continue;
    }

    home_comp->update_cooldown = k_update_interval;

    float min_distance = std::numeric_limits<float>::max();
    Engine::Core::EntityID nearest_barracks = 0;

    for (auto* barracks_entity : barracks_entities) {
      auto* barracks_transform =
          barracks_entity->get_component<Engine::Core::TransformComponent>();
      auto* barracks_unit =
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

    home_comp->nearest_barracks_id = nearest_barracks;

    if ((home_prod != nullptr) && (home_comp->family_generation_interval > 0.0F) &&
        (home_comp->family_manpower_value > 0) &&
        (home_comp->family_generation_cooldown <= 0.0F)) {
      home_prod->manpower_available =
          std::min(home_manpower_capacity(home_prod),
                   home_prod->manpower_available + home_comp->family_manpower_value);
      home_comp->family_generation_cooldown = home_comp->family_generation_interval;
    }
  }
}

} // namespace Game::Systems
