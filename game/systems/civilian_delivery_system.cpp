#include "civilian_delivery_system.h"

#include "../core/component.h"
#include "../core/world.h"
#include "../units/troop_config.h"
#include "units/spawn_type.h"
#include <vector>

namespace Game::Systems {

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
        civilian_entity->get_component<Engine::Core::CivilianDeliveryComponent>();
    auto *civilian_unit =
        civilian_entity->get_component<Engine::Core::UnitComponent>();
    auto *civilian_transform =
        civilian_entity->get_component<Engine::Core::TransformComponent>();

    if ((delivery == nullptr) || (civilian_unit == nullptr) ||
        (civilian_transform == nullptr) ||
        (civilian_unit->spawn_type != Game::Units::SpawnType::Civilian) ||
        (delivery->target_barracks_id == 0)) {
      civilian_entity->remove_component<Engine::Core::CivilianDeliveryComponent>();
      continue;
    }

    auto *barracks_entity = world->get_entity(delivery->target_barracks_id);
    auto *barracks_unit = barracks_entity
                              ? barracks_entity->get_component<Engine::Core::UnitComponent>()
                              : nullptr;
    auto *barracks_transform =
        barracks_entity
            ? barracks_entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    auto *barracks_prod =
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

    float const dx = civilian_transform->position.x - barracks_transform->position.x;
    float const dz = civilian_transform->position.z - barracks_transform->position.z;
    float const dist_sq = dx * dx + dz * dz;

    if (dist_sq > (k_delivery_radius * k_delivery_radius)) {
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
