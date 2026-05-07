#include "home.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "../systems/troop_profile_service.h"
#include "building_spawn_setup.h"
#include "units/troop_type.h"
#include "units/unit.h"
#include <memory>

namespace Game::Units {

Home::Home(Engine::Core::World &world) : Unit(world, "home") {}

auto Home::Create(Engine::Core::World &world,
                  const SpawnParams &params) -> std::unique_ptr<Home> {
  auto unit = std::unique_ptr<Home>(new Home(world));
  unit->init(params);
  return unit;
}

void Home::init(const SpawnParams &params) {
  auto *e = m_world->create_entity();
  m_id = e->get_id();

  const auto nation_id = resolve_nation_id(params);

  m_t = e->add_component<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(),
                   params.position.z()};
  m_t->scale = {1.2F, 1.0F, 1.2F};

  m_u = e->add_component<Engine::Core::UnitComponent>();
  m_u->spawn_type = params.spawn_type;
  m_u->health = 1000;
  m_u->max_health = 1000;
  m_u->speed = 0.0F;
  m_u->owner_id = params.player_id;
  m_u->vision_range = 15.0F;
  m_u->nation_id = nation_id;

  if (params.ai_controlled) {
    e->add_component<Engine::Core::AIControlledComponent>();
  }

  m_r = add_building_renderable(*e, m_u->owner_id, nation_id, m_type_string);

  auto *home_comp = e->add_component<Engine::Core::HomeComponent>();
  if (home_comp != nullptr) {
    home_comp->population_contribution = 50;
    home_comp->update_cooldown = 0.0F;
    home_comp->family_generation_interval = 12.0F;
    home_comp->family_generation_cooldown = home_comp->family_generation_interval;
    home_comp->family_manpower_value = 8;
  }

  if (!Game::Core::isNeutralOwner(m_u->owner_id)) {
    if (auto *prod = e->add_component<Engine::Core::ProductionComponent>()) {
      prod->product_type = TroopType::Civilian;
      prod->in_progress = false;
      prod->time_remaining = 0.0F;
      prod->produced_count = 0;
      prod->max_units = 10000;
      prod->manpower_available = 0;
      prod->rally_x = m_t->position.x + 2.0F;
      prod->rally_z = m_t->position.z + 1.0F;
      prod->rally_set = true;

      const auto profile =
          Game::Systems::TroopProfileService::instance().get_profile(
              nation_id, prod->product_type);
      prod->build_time = profile.production.build_time;
      prod->villager_cost = profile.production.cost;
    }
  }

  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      m_id, m_type_string, m_t->position.x, m_t->position.z, m_u->owner_id);

  Engine::Core::EventManager::instance().publish(Engine::Core::UnitSpawnedEvent(
      m_id, m_u->owner_id, m_u->spawn_type, params.is_initial_spawn));
}

} // namespace Game::Units
