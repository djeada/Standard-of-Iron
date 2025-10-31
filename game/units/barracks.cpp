#include "barracks.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "../systems/troop_profile_service.h"
#include "../visuals/team_colors.h"
#include "troop_config.h"
#include "units/troop_type.h"
#include "units/unit.h"
#include <memory>

namespace Game::Units {

Barracks::Barracks(Engine::Core::World &world) : Unit(world, "barracks") {}

auto Barracks::Create(Engine::Core::World &world,
                      const SpawnParams &params) -> std::unique_ptr<Barracks> {
  auto unit = std::unique_ptr<Barracks>(new Barracks(world));
  unit->init(params);
  return unit;
}

void Barracks::init(const SpawnParams &params) {
  auto *e = m_world->createEntity();
  m_id = e->getId();

  const auto nation_id = resolve_nation_id(params);

  m_t = e->addComponent<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(),
                   params.position.z()};
  m_t->scale = {1.8F, 1.2F, 1.8F};

  m_r = e->addComponent<Engine::Core::RenderableComponent>("", "");
  m_r->visible = true;
  m_r->mesh = Engine::Core::RenderableComponent::MeshKind::Cube;

  m_u = e->addComponent<Engine::Core::UnitComponent>();
  m_u->spawn_type = params.spawn_type;
  m_u->health = 2000;
  m_u->max_health = 2000;
  m_u->speed = 0.0F;
  m_u->owner_id = params.player_id;
  m_u->vision_range = 22.0F;
  m_u->nation_id = nation_id;

  if (params.aiControlled) {
    e->addComponent<Engine::Core::AIControlledComponent>();
  } else {
  }

  QVector3D const tc = Game::Visuals::team_colorForOwner(m_u->owner_id);
  m_r->color[0] = tc.x();
  m_r->color[1] = tc.y();
  m_r->color[2] = tc.z();

  e->addComponent<Engine::Core::BuildingComponent>();

  Game::Systems::BuildingCollisionRegistry::instance().registerBuilding(
      m_id, m_type_string, m_t->position.x, m_t->position.z, m_u->owner_id);

  if (!Game::Core::isNeutralOwner(m_u->owner_id)) {
    if (auto *prod = e->addComponent<Engine::Core::ProductionComponent>()) {
      prod->product_type = TroopType::Archer;
      prod->buildTime = 10.0F;
      prod->maxUnits = params.maxPopulation;
      prod->inProgress = false;
      prod->timeRemaining = 0.0F;
      prod->producedCount = 0;
      prod->rallyX = m_t->position.x + 4.0F;
      prod->rallyZ = m_t->position.z + 2.0F;
      prod->rallySet = true;

      const auto profile =
          Game::Systems::TroopProfileService::instance().get_profile(
              nation_id, prod->product_type);
      prod->buildTime = profile.production.build_time;
      prod->villagerCost = profile.individuals_per_unit;
    }
  }

  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitSpawnedEvent(m_id, m_u->owner_id, m_u->spawn_type));
}

} // namespace Game::Units
