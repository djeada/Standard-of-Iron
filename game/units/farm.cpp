#include "farm.h"

#include <memory>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "../systems/harvest_yields.h"
#include "building_spawn_setup.h"
#include "units/unit.h"

namespace Game::Units {

namespace {

constexpr float k_farm_scale_factor = 5.1F;
constexpr float k_farm_scale_xz = 1.4F * k_farm_scale_factor;
constexpr float k_farm_scale_y = 1.0F * k_farm_scale_factor;

} // namespace

Farm::Farm(Engine::Core::World& world)
    : Unit(world, "farm") {
}

auto Farm::Create(Engine::Core::World& world,
                  const SpawnParams& params) -> std::unique_ptr<Farm> {
  auto unit = std::unique_ptr<Farm>(new Farm(world));
  unit->init(params);
  return unit;
}

void Farm::init(const SpawnParams& params) {
  auto* e = m_world->create_entity();
  m_id = e->get_id();

  const auto nation_id = resolve_nation_id(params);

  m_t = e->add_component<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(), params.position.z()};
  m_t->rotation = {0.0F, params.rotation_y, 0.0F};
  m_t->scale = {k_farm_scale_xz, k_farm_scale_y, k_farm_scale_xz};

  m_u = e->add_component<Engine::Core::UnitComponent>();
  m_u->spawn_type = params.spawn_type;
  m_u->health = 600;
  m_u->max_health = 600;
  m_u->speed = 0.0F;
  m_u->owner_id = params.player_id;
  m_u->vision_range = 10.0F;
  m_u->nation_id = nation_id;

  if (params.ai_controlled) {
    e->add_component<Engine::Core::AIControlledComponent>();
  }

  m_r = add_building_renderable(*e, nation_id, m_type_string);

  if (auto* farm = e->add_component<Engine::Core::FarmComponent>()) {
    farm->growth = 0.0F;
    farm->cycle_seconds = Game::Systems::k_farm_growth_cycle_seconds;
  }

  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      m_id, m_type_string, m_t->position.x, m_t->position.z, m_u->owner_id);

  Engine::Core::EventManager::instance().publish(Engine::Core::UnitSpawnedEvent(
      m_id, m_u->owner_id, m_u->spawn_type, params.is_initial_spawn));
}

} // namespace Game::Units
