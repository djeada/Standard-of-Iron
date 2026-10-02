#include "wall_ladder.h"

#include <memory>

#include "../core/component_gameplay.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../systems/navigation/wall_network_service.h"
#include "building_spawn_setup.h"
#include "units/unit.h"

namespace Game::Units {

WallLadder::WallLadder(Engine::Core::World& world)
    : Unit(world, "wall_ladder") {
}

auto WallLadder::create(Engine::Core::World& world,
                        const SpawnParams& params) -> std::unique_ptr<WallLadder> {
  auto unit = std::unique_ptr<WallLadder>(new WallLadder(world));
  unit->init(params);
  return unit;
}

void WallLadder::init(const SpawnParams& params) {
  auto* e = m_world->create_entity();
  m_id = e->get_id();

  const auto nation_id = resolve_nation_id(params);

  m_t = e->add_component<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(), params.position.z()};
  m_t->rotation = {0.0F, params.rotation_y, 0.0F};
  m_t->scale = {1.0F, 1.0F, 1.0F};

  m_u = e->add_component<Engine::Core::UnitComponent>();
  m_u->spawn_type = SpawnType::WallLadder;
  m_u->health = 160;
  m_u->max_health = 160;
  m_u->speed = 0.0F;
  m_u->owner_id = params.player_id;
  m_u->vision_range = 0.0F;
  m_u->nation_id = nation_id;

  if (params.ai_controlled) {
    e->add_component<Engine::Core::AIControlledComponent>();
  }

  m_r = add_building_renderable(*e, nation_id, m_type_string);

  // Ladders do not join the wall network or block movement; refreshing the
  // network is what hangs them on their wall.
  Game::Systems::WallNetworkService::refresh_world(*m_world);

  Engine::Core::EventManager::instance().publish(Engine::Core::UnitSpawnedEvent(
      m_id, m_u->owner_id, m_u->spawn_type, params.is_initial_spawn));
}

} // namespace Game::Units
