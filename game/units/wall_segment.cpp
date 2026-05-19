#include "wall_segment.h"

#include <memory>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "building_spawn_setup.h"
#include "units/unit.h"

namespace Game::Units {

WallSegment::WallSegment(Engine::Core::World& world)
    : Unit(world, "wall_segment") {
}

auto WallSegment::create(Engine::Core::World& world,
                         const SpawnParams& params) -> std::unique_ptr<WallSegment> {
  auto unit = std::unique_ptr<WallSegment>(new WallSegment(world));
  unit->init(params);
  return unit;
}

void WallSegment::init(const SpawnParams& params) {
  auto* e = m_world->create_entity();
  m_id = e->get_id();

  const auto nation_id = resolve_nation_id(params);

  m_t = e->add_component<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(), params.position.z()};
  m_t->scale = {1.0F, 1.0F, 1.0F};

  m_u = e->add_component<Engine::Core::UnitComponent>();
  m_u->spawn_type = SpawnType::WallSegment;
  m_u->health = 800;
  m_u->max_health = 800;
  m_u->speed = 0.0F;
  m_u->owner_id = params.player_id;
  m_u->vision_range = 0.0F;
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
