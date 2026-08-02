#include "temple.h"

#include <memory>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "building_spawn_setup.h"
#include "units/unit.h"

namespace Game::Units {

namespace {

constexpr int k_temple_health = 900;
constexpr float k_temple_vision_range = 18.0F;
constexpr float k_temple_scale_factor = 1.5F;
constexpr float k_temple_scale_xz = 1.3F * k_temple_scale_factor;
constexpr float k_temple_scale_y = 1.15F * k_temple_scale_factor;

} // namespace

Temple::Temple(Engine::Core::World& world)
    : Unit(world, "temple") {
}

auto Temple::Create(Engine::Core::World& world,
                    const SpawnParams& params) -> std::unique_ptr<Temple> {
  auto unit = std::unique_ptr<Temple>(new Temple(world));
  unit->init(params);
  return unit;
}

void Temple::init(const SpawnParams& params) {
  auto* e = m_world->create_entity();
  m_id = e->get_id();

  const auto nation_id = resolve_nation_id(params);

  m_t = e->add_component<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(), params.position.z()};
  m_t->rotation = {0.0F, params.rotation_y, 0.0F};
  m_t->scale = {k_temple_scale_xz, k_temple_scale_y, k_temple_scale_xz};

  m_u = e->add_component<Engine::Core::UnitComponent>();
  m_u->spawn_type = params.spawn_type;
  m_u->health = k_temple_health;
  m_u->max_health = k_temple_health;
  m_u->speed = 0.0F;
  m_u->owner_id = params.player_id;
  m_u->vision_range = k_temple_vision_range;
  m_u->nation_id = nation_id;

  if (params.ai_controlled) {
    e->add_component<Engine::Core::AIControlledComponent>();
  }

  m_r = add_building_renderable(*e, m_u->owner_id, nation_id, m_type_string);

  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      m_id, m_type_string, m_t->position.x, m_t->position.z, m_u->owner_id);

  Engine::Core::EventManager::instance().publish(Engine::Core::UnitSpawnedEvent(
      m_id, m_u->owner_id, m_u->spawn_type, params.is_initial_spawn));
}

} // namespace Game::Units
