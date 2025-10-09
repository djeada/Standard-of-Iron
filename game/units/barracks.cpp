#include "barracks.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "../visuals/team_colors.h"

namespace Game {
namespace Units {

Barracks::Barracks(Engine::Core::World &world) : Unit(world, "barracks") {}

std::unique_ptr<Barracks> Barracks::Create(Engine::Core::World &world,
                                           const SpawnParams &params) {
  auto unit = std::unique_ptr<Barracks>(new Barracks(world));
  unit->init(params);
  return unit;
}

void Barracks::init(const SpawnParams &params) {
  auto *e = m_world->createEntity();
  m_id = e->getId();

  m_t = e->addComponent<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(),
                   params.position.z()};
  m_t->scale = {1.8f, 1.2f, 1.8f};

  m_r = e->addComponent<Engine::Core::RenderableComponent>("", "");
  m_r->visible = true;
  m_r->mesh = Engine::Core::RenderableComponent::MeshKind::Cube;

  m_u = e->addComponent<Engine::Core::UnitComponent>();
  m_u->unitType = m_type;
  m_u->health = 2000;
  m_u->maxHealth = 2000;
  m_u->speed = 0.0f;
  m_u->ownerId = params.playerId;
  m_u->visionRange = 22.0f;

  if (params.aiControlled) {
    e->addComponent<Engine::Core::AIControlledComponent>();
  }

  QVector3D tc = Game::Visuals::teamColorForOwner(m_u->ownerId);
  m_r->color[0] = tc.x();
  m_r->color[1] = tc.y();
  m_r->color[2] = tc.z();

  e->addComponent<Engine::Core::BuildingComponent>();

  Game::Systems::BuildingCollisionRegistry::instance().registerBuilding(
      m_id, m_type, m_t->position.x, m_t->position.z, m_u->ownerId);

  if (auto *prod = e->addComponent<Engine::Core::ProductionComponent>()) {
    prod->productType = "archer";
    prod->buildTime = 10.0f;
    prod->maxUnits = 100;
    prod->inProgress = false;
    prod->timeRemaining = 0.0f;
    prod->producedCount = 0;
    prod->rallyX = m_t->position.x + 4.0f;
    prod->rallyZ = m_t->position.z + 2.0f;
    prod->rallySet = true;
  }
}

} // namespace Units
} // namespace Game
