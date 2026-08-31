#include "barracks.h"

#include <algorithm>
#include <memory>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "../systems/civilian_delivery_system.h"
#include "../systems/nation_registry.h"
#include "../systems/production_service.h"
#include "../systems/troop_profile_service.h"
#include "building_spawn_setup.h"
#include "troop_config.h"
#include "units/troop_type.h"
#include "units/unit.h"

namespace Game::Units {

namespace {

auto manpower_ceiling_for(Game::Systems::NationID nation_id, int authored) -> int {

  const auto* nation = Game::Systems::NationRegistry::instance().get_nation(nation_id);
  if (nation == nullptr) {
    return authored;
  }
  int dearest = 0;
  for (const auto& troop : nation->available_troops) {
    if (is_commander_troop(troop.unit_type) ||
        Game::Systems::recruiting_building_for(troop.unit_type) !=
            SpawnType::Barracks) {
      continue;
    }
    dearest = std::max(dearest, troop.cost);
  }
  return std::max(authored, dearest + Game::Systems::k_civilian_delivery_reserve_grant);
}

} // namespace

Barracks::Barracks(Engine::Core::World& world)
    : Unit(world, "barracks") {
}

auto Barracks::Create(Engine::Core::World& world,
                      const SpawnParams& params) -> std::unique_ptr<Barracks> {
  auto unit = std::unique_ptr<Barracks>(new Barracks(world));
  unit->init(params);
  return unit;
}

void Barracks::init(const SpawnParams& params) {
  auto* e = m_world->create_entity();
  m_id = e->get_id();

  const auto nation_id = resolve_nation_id(params);

  m_t = e->add_component<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(), params.position.z()};
  m_t->rotation = {0.0F, params.rotation_y, 0.0F};
  m_t->scale = {1.8F, 1.2F, 1.8F};

  m_u = e->add_component<Engine::Core::UnitComponent>();
  m_u->spawn_type = params.spawn_type;
  m_u->health = 2000;
  m_u->max_health = 2000;
  m_u->speed = 0.0F;
  m_u->owner_id = params.player_id;
  m_u->vision_range = 22.0F;
  m_u->nation_id = nation_id;

  if (params.ai_controlled) {
    e->add_component<Engine::Core::AIControlledComponent>();
  }

  m_r = add_building_renderable(*e, nation_id, m_type_string);

  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      m_id,
      m_type_string,
      m_t->position.x,
      m_t->position.z,
      m_u->owner_id,
      m_t->rotation.y);

  if (!Game::Core::is_neutral_owner(m_u->owner_id) && params.enables_production) {
    if (auto* prod = e->add_component<Engine::Core::ProductionComponent>()) {
      prod->product_type = TroopType::Archer;
      prod->build_time = 10.0F;
      prod->max_units = params.max_population;
      prod->manpower_ceiling = manpower_ceiling_for(nation_id, params.max_population);
      prod->manpower_available = params.is_initial_spawn ? params.max_population : 0;
      prod->in_progress = false;
      prod->time_remaining = 0.0F;
      prod->produced_count = 0;
      prod->rally_x = m_t->position.x + 4.0F;
      prod->rally_z = m_t->position.z + 2.0F;
      prod->rally_set = true;

      const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
          nation_id, prod->product_type);
      prod->build_time = profile.production.build_time;
      prod->villager_cost = profile.production.cost;
    }
  }

  Engine::Core::EventManager::instance().publish(Engine::Core::UnitSpawnedEvent(
      m_id, m_u->owner_id, m_u->spawn_type, params.is_initial_spawn));
}

} // namespace Game::Units
