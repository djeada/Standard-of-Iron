#include "marketplace.h"

#include <memory>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "../systems/marketplace_system.h"
#include "building_spawn_setup.h"
#include "units/unit.h"

namespace Game::Units {

namespace {

constexpr float k_marketplace_scale_factor = 1.5F;
constexpr float k_marketplace_scale_xz = 1.3F * k_marketplace_scale_factor;
constexpr float k_marketplace_scale_y = 1.0F * k_marketplace_scale_factor;

} // namespace

Marketplace::Marketplace(Engine::Core::World& world)
    : Unit(world, "marketplace") {
}

auto Marketplace::Create(Engine::Core::World& world,
                         const SpawnParams& params) -> std::unique_ptr<Marketplace> {
  auto unit = std::unique_ptr<Marketplace>(new Marketplace(world));
  unit->init(params);
  return unit;
}

void Marketplace::init(const SpawnParams& params) {
  auto* e = m_world->create_entity();
  m_id = e->get_id();

  const auto nation_id = resolve_nation_id(params);

  m_t = e->add_component<Engine::Core::TransformComponent>();
  m_t->position = {params.position.x(), params.position.y(), params.position.z()};
  m_t->rotation = {0.0F, params.rotation_y, 0.0F};
  m_t->scale = {k_marketplace_scale_xz, k_marketplace_scale_y, k_marketplace_scale_xz};

  m_u = e->add_component<Engine::Core::UnitComponent>();
  m_u->spawn_type = params.spawn_type;
  m_u->health = 800;
  m_u->max_health = 800;
  m_u->speed = 0.0F;
  m_u->owner_id = params.player_id;
  m_u->vision_range = 12.0F;
  m_u->nation_id = nation_id;

  if (params.ai_controlled) {
    e->add_component<Engine::Core::AIControlledComponent>();
  }

  m_r = add_building_renderable(*e, m_u->owner_id, nation_id, m_type_string);

  Game::Systems::BuildingCollisionRegistry::instance().register_building(
      m_id, m_type_string, m_t->position.x, m_t->position.z, m_u->owner_id);
  Game::Systems::MarketplaceSystem::instance().register_marketplace(m_u->owner_id);

  Engine::Core::EventManager::instance().publish(Engine::Core::UnitSpawnedEvent(
      m_id, m_u->owner_id, m_u->spawn_type, params.is_initial_spawn));
}

} // namespace Game::Units
