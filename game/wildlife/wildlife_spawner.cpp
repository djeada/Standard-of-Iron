#include "wildlife_spawner.h"

#include <QCoreApplication>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>

#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../map/map_definition.h"
#include "../units/factory.h"
#include "../units/spawn_type.h"
#include "wildlife_rng.h"
#include "wildlife_system.h"
#include "wildlife_terrain_probe.h"

namespace Game::Wildlife {

namespace {

constexpr float k_two_pi = 6.28318530718F;
constexpr float k_spawn_scatter = 3.2F;
constexpr int k_open_point_attempts = 8;

auto seed_for(std::uint32_t seed, Species species, int index) -> std::uint32_t {
  std::uint32_t value = seed;
  value ^= (static_cast<std::uint32_t>(species_index(species)) + 1U) * 2654435761U;
  value ^= (static_cast<std::uint32_t>(index) + 1U) * 2246822519U;
  return value | 1U;
}

} // namespace

WildlifeSpawner::~WildlifeSpawner() = default;

auto pick_open_point(std::uint32_t& rng,
                     float origin_x,
                     float origin_z,
                     float min_radius,
                     float max_radius,
                     float& out_x,
                     float& out_z) -> bool {
  const auto& terrain = terrain_service_probe();
  WorldBounds const bounds = terrain.bounds();
  for (int attempt = 0; attempt < k_open_point_attempts; ++attempt) {
    float const angle = random_range(rng, 0.0F, k_two_pi);
    float const reach = random_range(rng, min_radius, std::max(min_radius, max_radius));
    float const x =
        std::clamp(origin_x + (std::cos(angle) * reach), bounds.min_x, bounds.max_x);
    float const z =
        std::clamp(origin_z + (std::sin(angle) * reach), bounds.min_z, bounds.max_z);
    if (terrain.is_blocked(x, z)) {
      continue;
    }
    out_x = x;
    out_z = z;
    return true;
  }
  return false;
}

void WildlifeSpawner::ensure_factory_registry() {
  if (m_factory_registry) {
    return;
  }
  m_factory_registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::register_built_in_units(*m_factory_registry);
}

void WildlifeSpawner::configure(const WildlifeSettings& settings,
                                std::uint32_t seed,
                                bool enabled) {
  m_seed = seed;
  m_groups.clear();
  m_next_group_id = 0U;
  if (enabled) {
    plan_groups(settings);
  }
}

void WildlifeSpawner::plan_groups(const WildlifeSettings& settings) {
  const std::array<Species, 2> ground_species{{Species::Sheep, Species::Wolf}};
  for (Species const species : ground_species) {
    const auto& config = settings.for_species(species);
    if (!config.enabled || config.group_count <= 0) {
      continue;
    }
    for (int index = 0; index < config.group_count; ++index) {
      std::uint32_t rng = seed_for(m_seed, species, index);

      GroupState group;
      group.id = m_next_group_id++;
      group.species = species;
      group.roam_radius = config.roam_radius;
      group.rng_state = rng;
      group.desired_size = config.clamped_group_size(
          static_cast<std::uint32_t>(next_random(rng) * 4096.0F));

      float anchor_x = 0.0F;
      float anchor_z = 0.0F;
      float origin_x = 0.0F;
      float origin_z = 0.0F;
      float reach = 0.0F;
      if (!config.spawn_areas.empty()) {
        (void)next_random(rng);
        const auto& area = config.spawn_areas[static_cast<std::size_t>(index) %
                                              config.spawn_areas.size()];
        origin_x = area.x;
        origin_z = area.z;
        reach = area.radius;
      } else {
        WorldBounds const bounds = terrain_service_probe().bounds();
        origin_x = (bounds.min_x + bounds.max_x) * 0.5F;
        origin_z = (bounds.min_z + bounds.max_z) * 0.5F;
        reach =
            std::min(bounds.max_x - bounds.min_x, bounds.max_z - bounds.min_z) * 0.35F;
      }

      bool anchored = false;
      for (float scale : {1.0F, 1.8F, 3.0F}) {
        if (pick_open_point(
                rng, origin_x, origin_z, 0.0F, reach * scale, anchor_x, anchor_z)) {
          anchored = true;
          break;
        }
      }
      if (!anchored) {
        continue;
      }

      group.home_x = anchor_x;
      group.home_z = anchor_z;
      group.rng_state = rng | 1U;
      m_groups.push_back(group);
    }
  }
}

auto WildlifeSpawner::spawn_member(Engine::Core::World& world,
                                   GroupState& group,
                                   const SpeciesConfig& config)
    -> Engine::Core::EntityID {
  ensure_factory_registry();

  float spawn_x = group.home_x;
  float spawn_z = group.home_z;
  if (!pick_open_point(group.rng_state,
                       group.home_x,
                       group.home_z,
                       0.0F,
                       k_spawn_scatter,
                       spawn_x,
                       spawn_z) &&
      !pick_open_point(group.rng_state,
                       group.home_x,
                       group.home_z,
                       0.0F,
                       k_spawn_scatter * 3.0F,
                       spawn_x,
                       spawn_z)) {
    return 0;
  }

  const auto& terrain = terrain_service_probe();
  Game::Units::SpawnParams params;
  params.position =
      QVector3D(spawn_x, terrain.ground_height(spawn_x, spawn_z), spawn_z);
  params.player_id = Game::Core::NEUTRAL_OWNER_ID;
  params.spawn_type = group.species == Species::Wolf ? Game::Units::SpawnType::Wolf
                                                     : Game::Units::SpawnType::Sheep;
  params.rotation_y = random_range(group.rng_state, 0.0F, 360.0F);
  params.ai_controlled = false;
  params.is_initial_spawn = false;

  auto unit = m_factory_registry->create(params.spawn_type, world, params);
  if (!unit) {
    return 0;
  }

  auto* entity = world.get_entity(unit->id());
  if (entity == nullptr) {
    return 0;
  }
  auto* wildlife = entity->get_component<Engine::Core::WildlifeComponent>();
  if (wildlife != nullptr) {
    wildlife->group_id = group.id;
    wildlife->home_x = group.home_x;
    wildlife->home_z = group.home_z;
    wildlife->roam_radius = group.roam_radius;
    wildlife->anchor_assigned = true;
    wildlife->rng_state = group.rng_state | 1U;
    wildlife->think_cooldown =
        random_range(group.rng_state, 0.0F, WildlifeSystem::k_think_interval * 2.0F);
  }
  auto* unit_comp = entity->get_component<Engine::Core::UnitComponent>();
  if (unit_comp != nullptr) {
    unit_comp->speed = config.move_speed;
  }
  return unit->id();
}

void WildlifeSpawner::spawn_initial_population(Engine::Core::World& world,
                                               const WildlifeSettings& settings) {
  for (auto& group : m_groups) {
    const auto& config = settings.for_species(group.species);
    for (int member = 0; member < group.desired_size; ++member) {
      spawn_member(world, group, config);
    }
  }
}

void WildlifeSpawner::release_due_packs(Engine::Core::World& world,
                                        const WildlifeSettings& settings,
                                        float delta_time) {
  const auto& config = settings.for_species(Species::Wolf);
  if (config.waves.empty()) {
    return;
  }

  m_elapsed += static_cast<double>(delta_time);
  if (m_released_waves.size() != config.waves.size()) {
    m_released_waves.resize(config.waves.size(), false);
  }

  for (std::size_t index = 0; index < config.waves.size(); ++index) {
    if (m_released_waves[index]) {
      continue;
    }
    if (m_elapsed < static_cast<double>(config.waves[index].timing)) {
      break;
    }
    m_released_waves[index] = true;
    release_pack(world, config, index);
  }
}

void WildlifeSpawner::release_pack(Engine::Core::World& world,
                                   const SpeciesConfig& config,
                                   std::size_t wave_index) {
  const auto& wave = config.waves[wave_index];

  GroupState group;
  group.id = m_next_group_id++;
  group.species = Species::Wolf;
  group.roam_radius = std::max(config.roam_radius, wave.area.radius);
  group.rng_state =
      seed_for(m_seed, Species::Wolf, static_cast<int>(wave_index) + 4096);
  group.desired_size = wave.pack_size;

  float anchor_x = wave.area.x;
  float anchor_z = wave.area.z;
  bool anchored = false;
  for (float scale : {1.0F, 1.8F, 3.0F}) {
    if (pick_open_point(group.rng_state,
                        wave.area.x,
                        wave.area.z,
                        0.0F,
                        wave.area.radius * scale,
                        anchor_x,
                        anchor_z)) {
      anchored = true;
      break;
    }
  }
  if (!anchored) {
    return;
  }

  group.home_x = anchor_x;
  group.home_z = anchor_z;
  m_groups.push_back(group);

  auto& stored = m_groups.back();
  for (int member = 0; member < stored.desired_size; ++member) {
    spawn_member(world, stored, config);
  }

  Engine::Core::EventManager::instance().publish(Engine::Core::MissionAnnouncementEvent(
      wave.label.empty() ? QCoreApplication::translate(
                               "WildlifeSystem", "Wolves are moving on the valley.")
                         : QString::fromStdString(wave.label)));
}

void WildlifeSpawner::update_respawns(Engine::Core::World& world,
                                      const WildlifeSettings& settings,
                                      const WildlifeCensus& census,
                                      WildlifeStats& stats,
                                      float delta_time) {
  for (std::size_t index = 0; index < m_groups.size(); ++index) {
    auto& group = m_groups[index];
    const auto& config = settings.for_species(group.species);
    int const alive = census.alive_in(index);

    if (alive >= group.desired_size) {
      group.respawn_timer = config.respawn_delay;
      continue;
    }
    if (!config.respawn) {
      continue;
    }

    group.respawn_timer -= delta_time;
    if (group.respawn_timer > 0.0F) {
      continue;
    }
    group.respawn_timer = config.respawn_delay;
    if (spawn_member(world, group, config) != 0) {
      stats.respawns += 1U;
    }
  }
}

void WildlifeSpawner::store_into(WildlifeSaveData& data) const {
  data.seed = m_seed;
  data.next_group_id = m_next_group_id;
  data.elapsed = m_elapsed;
  data.released_waves = m_released_waves;
  data.groups = m_groups;
}

void WildlifeSpawner::restore_from(const WildlifeSaveData& data) {
  m_seed = data.seed;
  m_next_group_id = data.next_group_id;
  m_elapsed = data.elapsed;
  m_released_waves = data.released_waves;
  m_groups = data.groups;
}

} // namespace Game::Wildlife
