#include "wildlife_system.h"

#include <algorithm>
#include <cmath>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../map/map_definition.h"
#include "bird_flock.h"
#include "wildlife_nature_actions.h"
#include "wildlife_persistence.h"
#include "wildlife_rng.h"

namespace Game::Wildlife {

WildlifeSystem::WildlifeSystem() = default;

WildlifeSystem::~WildlifeSystem() = default;

void WildlifeSystem::configure(const Game::Map::MapDefinition& map_definition) {
  configure(map_definition.wildlife, map_definition.biome.seed);
}

void WildlifeSystem::configure(const WildlifeSettings& settings,
                               std::uint32_t map_seed) {
  m_settings = settings;
  sanitize(m_settings);
  std::uint32_t const seed = m_settings.seed != 0U ? m_settings.seed : (map_seed | 1U);
  m_enabled = m_settings.enabled && m_settings.any_species_enabled();
  m_census.reset();
  m_stats.reset();
  m_threat_refresh = 0.0F;
  m_restored = false;
  m_spawn_pending = m_enabled;

  m_spawner.configure(m_settings, seed, m_enabled);

  BirdFlockManager::instance().configure(m_settings.birds,
                                         seed ^ 0x5EEDBEEFU,
                                         m_settings.near_simulation_radius,
                                         m_settings.far_simulation_radius);
  if (!m_enabled) {
    BirdFlockManager::instance().reset();
  }
}

void WildlifeSystem::set_cosmetic_focus(float world_x, float world_z) noexcept {
  BirdFlockManager::instance().set_focus(world_x, world_z);
}

void WildlifeSystem::clear_cosmetic_focus() noexcept {
  BirdFlockManager::instance().clear_focus();
}

void WildlifeSystem::release_if_stalled(const AnimalRef& animal,
                                        Engine::Core::WildlifeComponent& wildlife,
                                        float delta_time) {
  auto* movement = animal.entity->get_component<Engine::Core::MovementComponent>();
  bool const meant_to_hold = wildlife.behavior == Behavior::Graze ||
                             wildlife.bite_timer > 0.0F || wildlife.flinch_timer > 0.0F;
  if (movement == nullptr || meant_to_hold) {
    wildlife.stall_timer = 0.0F;
    wildlife.last_x = animal.x;
    wildlife.last_z = animal.z;
    return;
  }

  float const dx = animal.x - wildlife.last_x;
  float const dz = animal.z - wildlife.last_z;
  wildlife.last_x = animal.x;
  wildlife.last_z = animal.z;

  if ((dx * dx) + (dz * dz) > k_stall_step_epsilon * k_stall_step_epsilon) {
    wildlife.stall_timer = 0.0F;
    return;
  }

  wildlife.stall_timer += delta_time;
  if (wildlife.stall_timer < Engine::Core::WildlifeComponent::k_stall_release_seconds) {
    return;
  }

  wildlife.stall_timer = 0.0F;
  wildlife.stalled = true;
  movement->stop();
  wildlife.think_cooldown = 0.0F;
  m_stats.stall_releases += 1U;
}

void WildlifeSystem::think(Engine::Core::World& world,
                           Engine::Core::Entity& entity,
                           const GroupRuntime& runtime,
                           const SpeciesConfig& config,
                           Species species) {
  auto* wildlife = entity.get_component<Engine::Core::WildlifeComponent>();
  auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  auto* movement = entity.get_component<Engine::Core::MovementComponent>();
  if (wildlife == nullptr || transform == nullptr || movement == nullptr) {
    return;
  }

  NatureContext ctx;
  ctx.world = &world;
  ctx.entity = &entity;
  ctx.wildlife = wildlife;
  ctx.movement = movement;
  ctx.config = &config;
  ctx.threats = &m_census.threats();
  ctx.herd.center_x = runtime.center_x;
  ctx.herd.center_z = runtime.center_z;
  ctx.herd.alarm = runtime.alarm;
  ctx.herd.alive = runtime.alive;
  ctx.x = transform->position.x;
  ctx.z = transform->position.z;

  WildlifeNatureActions adapter(world, m_census, m_predation, m_stats);
  NatureBrain& brain = species == Species::Wolf ? m_wolf_brain : m_sheep_brain;
  brain.tick(ctx, adapter);
}

void WildlifeSystem::refresh_census(Engine::Core::World& world, float delta_time) {
  m_threat_refresh -= delta_time;
  if (m_threat_refresh <= 0.0F) {
    m_threat_refresh = k_threat_refresh_interval;
    m_census.rebuild_threats(world);
  }
  m_census.collect_animals(world, m_spawner.groups());
}

void WildlifeSystem::think_if_due(Engine::Core::World& world,
                                  const AnimalRef& animal,
                                  Engine::Core::WildlifeComponent& wildlife,
                                  Tier tier,
                                  float delta_time) {
  wildlife.think_cooldown -= delta_time;
  if (wildlife.think_cooldown > 0.0F) {
    return;
  }

  float const multiplier = tier == Tier::Far ? k_far_think_multiplier : 1.0F;
  if (wildlife.rng_state == 0U) {
    wildlife.rng_state = static_cast<std::uint32_t>((animal.id * 2654435761ULL) | 1ULL);
  }
  wildlife.think_cooldown =
      (k_think_interval * multiplier) +
      random_range(wildlife.rng_state, 0.0F, k_think_interval * 0.5F);

  if (tier == Tier::Near) {
    m_stats.near_thinks += 1U;
  } else {
    m_stats.far_thinks += 1U;
  }

  GroupRuntime const runtime =
      m_census.runtime_for_group(m_spawner.groups(), wildlife.group_id);
  const SpeciesConfig& config =
      wildlife.species == Species::Wolf ? m_settings.wolves : m_settings.sheep;
  think(world, *animal.entity, runtime, config, wildlife.species);
}

void WildlifeSystem::update_animal(Engine::Core::World& world,
                                   const AnimalRef& animal,
                                   float delta_time) {
  auto* wildlife = animal.entity->get_component<Engine::Core::WildlifeComponent>();
  if (wildlife == nullptr) {
    return;
  }

  Tier const tier = m_census.tier_for(animal.x,
                                      animal.z,
                                      m_settings.near_simulation_radius,
                                      m_settings.far_simulation_radius);
  if (tier == Tier::Dormant) {
    m_stats.dormant_skips += 1U;
    return;
  }

  wildlife->state_timer = std::max(0.0F, wildlife->state_timer - delta_time);
  wildlife->alarm_timer = std::max(0.0F, wildlife->alarm_timer - delta_time);
  wildlife->flinch_timer = std::max(0.0F, wildlife->flinch_timer - delta_time);
  if (const auto* unit = animal.entity->get_component<Engine::Core::UnitComponent>()) {
    if (wildlife->watched_health >= 0 && unit->health < wildlife->watched_health) {
      wildlife->flinch_timer =
          Engine::Core::WildlifeComponent::k_flinch_animation_seconds;
    }
    wildlife->watched_health = unit->health;
  }
  wildlife->hostile_timer = std::max(0.0F, wildlife->hostile_timer - delta_time);
  m_predation.advance_bite(world, animal, *wildlife, delta_time);
  if (wildlife->hostile_timer <= 0.0F) {
    wildlife->aggressor_id = 0;
  }

  if (wildlife->bite_timer > 0.0F) {
    if (auto* movement =
            world.try_get<Engine::Core::MovementComponent>(animal.entity->get_id())) {
      movement->stop();
    }
    return;
  }

  if (animal.species == Species::Wolf) {
    m_predation.try_contact_bite(world, animal, *wildlife);
    if (wildlife->bite_timer > 0.0F) {
      return;
    }
  }

  if (wildlife->held_timer > 0.0F) {
    wildlife->held_timer -= delta_time;
    if (auto* movement =
            animal.entity->get_component<Engine::Core::MovementComponent>();
        movement != nullptr && movement->get_has_target()) {
      movement->stop();
    }
    wildlife->think_cooldown = std::max(wildlife->think_cooldown, 0.25F);
    return;
  }

  release_if_stalled(animal, *wildlife, delta_time);
  think_if_due(world, animal, *wildlife, tier, delta_time);
}

void WildlifeSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr || !m_enabled || delta_time <= 0.0F) {
    return;
  }

  if (m_spawn_pending) {
    m_spawn_pending = false;
    if (!m_restored) {
      m_spawner.spawn_initial_population(*world, m_settings);
    }
  }

  m_spawner.release_due_packs(*world, m_settings, delta_time);
  refresh_census(*world, delta_time);

  for (const auto& animal : m_census.animals()) {
    if (animal.entity != nullptr) {
      update_animal(*world, animal, delta_time);
    }
  }

  m_spawner.update_respawns(*world, m_settings, m_census, m_stats, delta_time);

  BirdFlockManager::instance().update(delta_time, m_census.threats());
}

auto WildlifeSystem::serialize_state() const -> QJsonObject {
  WildlifeSaveData data;
  data.enabled = m_enabled;
  m_spawner.store_into(data);
  const auto& flock_manager = BirdFlockManager::instance();
  data.birds.flocks = flock_manager.flocks();
  data.birds.birds = flock_manager.birds();
  return encode_wildlife_state(data);
}

void WildlifeSystem::restore_state(const QJsonObject& state) {
  if (state.isEmpty()) {
    return;
  }

  WildlifeSaveData current;
  current.enabled = m_enabled;
  m_spawner.store_into(current);
  WildlifeSaveData const restored = decode_wildlife_state(state, current);

  m_enabled = restored.enabled;
  m_spawner.restore_from(restored);
  BirdFlockManager::instance().restore(restored.birds);

  m_restored = true;
  m_spawn_pending = false;
}

} // namespace Game::Wildlife
