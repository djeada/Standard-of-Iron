#include "defense_tower.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "../visuals/team_colors.h"
#include "units/unit.h"
#include <memory>

namespace Game::Units {

DefenseTower::DefenseTower(Engine::Core::World &world)
    : Unit(world, "defense_tower") {}

auto DefenseTower::create(
    Engine::Core::World &world,
    const SpawnParams &params) -> std::unique_ptr<DefenseTower> {
  auto unit = std::unique_ptr<DefenseTower>(new DefenseTower(world));
  unit->init(params);
  return unit;
}

void DefenseTower::init(const SpawnParams &params) {
  auto *e = m_world->create_entity();
  m_id = e->get_id();

  const auto nation_id = resolve_nation_id(params);

  m_t = e->add_component<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(),
                   params.position.z()};
  m_t->scale = {1.0F, 2.0F, 1.0F};

  m_r = e->add_component<Engine::Core::RenderableComponent>("", "");
  m_r->visible = true;
  m_r->mesh = Engine::Core::RenderableComponent::MeshKind::Cube;

  m_u = e->add_component<Engine::Core::UnitComponent>();
  m_u->spawn_type = SpawnType::DefenseTower;
  m_u->health = 1500;
  m_u->max_health = 1500;
  m_u->speed = 0.0F;
  m_u->owner_id = params.player_id;
  m_u->vision_range = 18.0F;
  m_u->nation_id = nation_id;

  if (params.ai_controlled) {
    e->add_component<Engine::Core::AIControlledComponent>();
  }

  QVector3D const tc = Game::Visuals::team_colorForOwner(m_u->owner_id);
  m_r->color[0] = tc.x();
  m_r->color[1] = tc.y();
  m_r->color[2] = tc.z();

  e->add_component<Engine::Core::BuildingComponent>();

  m_atk = e->add_component<Engine::Core::AttackComponent>();
  m_atk->range = 16.0F;
  m_atk->damage = 25;
  m_atk->cooldown = 2.0F;
  m_atk->can_ranged = true;
  m_atk->can_melee = false;
  m_atk->preferred_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
  m_atk->current_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
  m_atk->max_height_difference = 4.0F;

  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      m_id, m_type_string, m_t->position.x, m_t->position.z, m_u->owner_id);

  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitSpawnedEvent(m_id, m_u->owner_id, m_u->spawn_type));
}

} // namespace Game::Units
