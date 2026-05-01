#include "home.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "building_spawn_setup.h"
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
  }

  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      m_id, m_type_string, m_t->position.x, m_t->position.z, m_u->owner_id);

  Engine::Core::EventManager::instance().publish(Engine::Core::UnitSpawnedEvent(
      m_id, m_u->owner_id, m_u->spawn_type, params.is_initial_spawn));
}

} // namespace Game::Units
