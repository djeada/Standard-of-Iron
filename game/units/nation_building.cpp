#include "nation_building.h"

#include <memory>

#include "../core/ambient_session.h"
#include "../core/component_gameplay.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "building_spawn_setup.h"
#include "units/spawn_type.h"
#include "units/unit.h"

namespace Game::Units {

namespace {

struct NationBuildingStats {
  int health;
  float vision_range;
};

auto stats_for(SpawnType type) -> NationBuildingStats {
  switch (type) {
  case SpawnType::MercenaryCamp:
    return {1400, 20.0F};
  case SpawnType::ElephantStables:
    return {1600, 18.0F};
  case SpawnType::Fabrica:
    return {1200, 16.0F};
  case SpawnType::Castra:
    return {1000, 22.0F};
  default:
    return {1000, 16.0F};
  }
}

} // namespace

NationBuilding::NationBuilding(Engine::Core::World& world, SpawnType type)
    : Unit(world, std::string(spawn_type_name(type))) {
}

auto NationBuilding::Create(Engine::Core::World& world, const SpawnParams& params)
    -> std::unique_ptr<NationBuilding> {
  auto unit =
      std::unique_ptr<NationBuilding>(new NationBuilding(world, params.spawn_type));
  unit->init(params);
  return unit;
}

void NationBuilding::init(const SpawnParams& params) {
  auto* e = m_world->create_entity();
  m_id = e->get_id();
  auto& world = *m_world;

  const auto nation_id = resolve_nation_id(params);
  const auto stats = stats_for(params.spawn_type);

  m_t = world.emplace<Engine::Core::TransformComponent>(m_id);
  m_t->position = {params.position.x(), params.position.y(), params.position.z()};
  m_t->rotation = {0.0F, params.rotation_y, 0.0F};
  const QVector3D scale = building_transform_scale(m_type_string);
  m_t->scale = {scale.x(), scale.y(), scale.z()};

  m_u = world.emplace<Engine::Core::UnitComponent>(m_id);
  m_u->spawn_type = params.spawn_type;
  m_u->health = stats.health;
  m_u->max_health = stats.health;
  m_u->speed = 0.0F;
  m_u->owner_id = params.player_id;
  m_u->vision_range = stats.vision_range;
  m_u->nation_id = nation_id;

  if (params.ai_controlled) {
    world.emplace<Engine::Core::AIControlledComponent>(m_id);
  }

  m_r = add_building_renderable(*e, nation_id, m_type_string);

  if (is_hiring_building(params.spawn_type) &&
      !Game::Core::is_neutral_owner(m_u->owner_id)) {
    if (auto* prod = world.emplace<Engine::Core::ProductionComponent>(m_id)) {
      prod->product_type = params.spawn_type == SpawnType::ElephantStables
                               ? TroopType::Elephant
                               : TroopType::Swordsman;
      prod->max_units = 0;
      prod->manpower_available = 0;
      prod->in_progress = false;
      prod->produced_count = 0;
      prod->rally_x = m_t->position.x + 5.0F;
      prod->rally_z = m_t->position.z + 3.0F;
      prod->rally_set = true;
    }
  }

  Game::Session::services_for(world).building_collision->register_building(
      m_id,
      m_type_string,
      m_t->position.x,
      m_t->position.z,
      m_u->owner_id,
      m_t->rotation.y);

  Engine::Core::EventManager::instance().publish(Engine::Core::UnitSpawnedEvent(
      m_id, m_u->owner_id, m_u->spawn_type, params.is_initial_spawn));
}

} // namespace Game::Units
