#include "wildlife_system.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>

#include "../audio/cue_ids.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../map/map_definition.h"
#include "../systems/combat_system/combat_utils.h"
#include "../systems/combat_system/damage_application.h"
#include "../systems/command_service.h"
#include "../systems/nav_grid.h"
#include "../systems/order_service.h"
#include "../units/factory.h"
#include "../units/spawn_type.h"
#include "bird_flock.h"
#include "wildlife_terrain_probe.h"

namespace Game::Wildlife {

namespace {

constexpr float k_two_pi = 6.28318530718F;
constexpr float k_wolf_bite_range = 1.35F;
constexpr float k_civilian_threat_strength = 0.3F;
constexpr float k_civilian_quarry_preference = 0.5F;
constexpr int k_pack_gang_size = 3;
constexpr float k_pack_join_gain = 30.0F;
constexpr float k_pack_second_join_gain = 9.0F;
constexpr float k_quarry_crowd_penalty = 400.0F;
constexpr float k_wolf_bite_recovery = 0.35F;
constexpr float k_wolf_bite_flinch_seconds = 0.30F;
constexpr float k_wolf_bite_blood_chance = 0.34F;
constexpr float k_wolf_bite_blood_spread = 0.55F;
constexpr float k_move_reissue_epsilon = 0.75F;
constexpr float k_troop_threat_strength = 1.0F;
constexpr float k_spawn_scatter = 3.2F;
constexpr int k_open_point_attempts = 8;

auto next_random(std::uint32_t& state) -> float {
  state = (state * 1664525U) + 1013904223U;
  return static_cast<float>((state >> 8U) & 0xFFFFFFU) / 16777216.0F;
}

auto random_range(std::uint32_t& state, float low, float high) -> float {
  return low + ((high - low) * next_random(state));
}

auto hash_unit_interval(std::uint32_t value) -> float {
  value ^= value >> 16U;
  value *= 2246822519U;
  value ^= value >> 13U;
  value *= 3266489917U;
  value ^= value >> 16U;
  return static_cast<float>(value & 0xFFFFFFU) / 16777216.0F;
}

auto crowding_penalty(int attackers, float appetite) -> float {
  int const committed = std::max(attackers, 0);
  if (committed >= k_pack_gang_size) {

    float const excess = static_cast<float>((committed - k_pack_gang_size) + 1);
    return k_quarry_crowd_penalty * excess;
  }
  if (committed <= 0) {
    return 0.0F;
  }

  float const gain =
      committed == 1 ? k_pack_join_gain : k_pack_join_gain + k_pack_second_join_gain;
  return -gain * std::clamp(appetite, 0.0F, 1.0F);
}

auto seed_for(std::uint32_t seed, Species species, int index) -> std::uint32_t {
  std::uint32_t value = seed;
  value ^= (static_cast<std::uint32_t>(species_index(species)) + 1U) * 2654435761U;
  value ^= (static_cast<std::uint32_t>(index) + 1U) * 2246822519U;
  return value | 1U;
}

auto is_wildlife_entity(const Engine::Core::Entity& entity) -> bool {
  return entity.has_component<Engine::Core::WildlifeComponent>();
}

auto threat_strength_of(const Engine::Core::UnitComponent& unit) -> float {
  switch (unit.spawn_type) {
  case Game::Units::SpawnType::Civilian:
  case Game::Units::SpawnType::Builder:
    return k_civilian_threat_strength;
  default:
    return k_troop_threat_strength;
  }
}

auto is_civilian_spawn(Game::Units::SpawnType type) -> bool {
  return type == Game::Units::SpawnType::Civilian ||
         type == Game::Units::SpawnType::Builder;
}

auto resolve_prey(Engine::Core::World& world,
                  Engine::Core::EntityID entity_id) -> PreyRef {
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr ||
      entity->has_component<Engine::Core::PendingRemovalComponent>()) {
    return {};
  }
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (unit == nullptr || transform == nullptr || unit->health <= 0) {
    return {};
  }

  PreyRef prey;
  prey.entity = entity;
  prey.id = entity_id;
  prey.x = transform->position.x;
  prey.z = transform->position.z;
  prey.radius = std::max(0.0F, std::max(transform->scale.x, transform->scale.z) * 0.5F);
  prey.livestock = is_wildlife_entity(*entity);
  return prey;
}

} // namespace

WildlifeSystem::WildlifeSystem() = default;

WildlifeSystem::~WildlifeSystem() = default;

void WildlifeSystem::ensure_factory_registry() {
  if (m_factory_registry) {
    return;
  }
  m_factory_registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::register_built_in_units(*m_factory_registry);
}

void WildlifeSystem::configure(const Game::Map::MapDefinition& map_definition) {
  configure(map_definition.wildlife, map_definition.biome.seed);
}

void WildlifeSystem::configure(const WildlifeSettings& settings,
                               std::uint32_t map_seed) {
  m_settings = settings;
  sanitize(m_settings);
  m_seed = m_settings.seed != 0U ? m_settings.seed : (map_seed | 1U);
  m_enabled = m_settings.enabled && m_settings.any_species_enabled();
  m_groups.clear();
  m_group_runtime.clear();
  m_animals.clear();
  m_quarry.clear();
  m_threats.clear();
  m_stats.reset();
  m_next_group_id = 0U;
  m_threat_refresh = 0.0F;
  m_restored = false;
  m_spawn_pending = m_enabled;

  if (m_enabled) {
    plan_groups();
  }

  BirdFlockManager::instance().configure(m_settings.birds,
                                         m_seed ^ 0x5EEDBEEFU,
                                         m_settings.near_simulation_radius,
                                         m_settings.far_simulation_radius);
  if (!m_enabled) {
    BirdFlockManager::instance().reset();
  }
}

void WildlifeSystem::set_focus(float world_x, float world_z) noexcept {
  m_focus_x = world_x;
  m_focus_z = world_z;
  m_has_focus = true;
  BirdFlockManager::instance().set_focus(world_x, world_z);
}

void WildlifeSystem::clear_focus() noexcept {
  m_has_focus = false;
  BirdFlockManager::instance().clear_focus();
}

auto WildlifeSystem::tier_for(float world_x, float world_z) const noexcept -> Tier {
  if (!m_has_focus) {
    return Tier::Near;
  }
  float const dx = world_x - m_focus_x;
  float const dz = world_z - m_focus_z;
  float const distance_sq = (dx * dx) + (dz * dz);
  if (distance_sq <=
      m_settings.near_simulation_radius * m_settings.near_simulation_radius) {
    return Tier::Near;
  }
  if (distance_sq <=
      m_settings.far_simulation_radius * m_settings.far_simulation_radius) {
    return Tier::Far;
  }
  return Tier::Dormant;
}

auto WildlifeSystem::find_group(std::uint16_t group_id) -> GroupState* {
  for (auto& group : m_groups) {
    if (group.id == group_id) {
      return &group;
    }
  }
  return nullptr;
}

auto WildlifeSystem::pick_open_point(std::uint32_t& rng,
                                     float origin_x,
                                     float origin_z,
                                     float min_radius,
                                     float max_radius,
                                     float& out_x,
                                     float& out_z) const -> bool {
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

void WildlifeSystem::plan_groups() {
  const std::array<Species, 2> ground_species{{Species::Sheep, Species::Wolf}};
  for (Species const species : ground_species) {
    const auto& config = m_settings.for_species(species);
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
        auto const area_index = static_cast<std::size_t>(
            next_random(rng) * static_cast<float>(config.spawn_areas.size()));
        const auto& area =
            config.spawn_areas[std::min(area_index, config.spawn_areas.size() - 1U)];
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
  m_group_runtime.assign(m_groups.size(), GroupRuntime{});
}

auto WildlifeSystem::spawn_member(Engine::Core::World& world,
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
        random_range(group.rng_state, 0.0F, k_think_interval * 2.0F);
  }
  auto* unit_comp = entity->get_component<Engine::Core::UnitComponent>();
  if (unit_comp != nullptr) {
    unit_comp->speed = config.move_speed;
  }
  return unit->id();
}

void WildlifeSystem::spawn_initial_population(Engine::Core::World& world) {
  for (auto& group : m_groups) {
    const auto& config = m_settings.for_species(group.species);
    for (int member = 0; member < group.desired_size; ++member) {
      spawn_member(world, group, config);
    }
  }
}

void WildlifeSystem::release_due_packs(Engine::Core::World& world, float delta_time) {
  const auto& config = m_settings.for_species(Species::Wolf);
  if (config.waves.empty()) {
    return;
  }

  m_elapsed += delta_time;
  if (m_released_waves.size() != config.waves.size()) {
    m_released_waves.resize(config.waves.size(), false);
  }

  for (std::size_t index = 0; index < config.waves.size(); ++index) {
    if (m_released_waves[index]) {
      continue;
    }
    const auto& wave = config.waves[index];
    if (m_elapsed < wave.timing) {

      break;
    }
    m_released_waves[index] = true;

    GroupState group;
    group.id = m_next_group_id++;
    group.species = Species::Wolf;
    group.roam_radius = std::max(config.roam_radius, wave.area.radius);
    group.rng_state = seed_for(m_seed, Species::Wolf, static_cast<int>(index) + 4096);
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
      continue;
    }

    group.home_x = anchor_x;
    group.home_z = anchor_z;
    m_groups.push_back(group);
    m_group_runtime.emplace_back();

    auto& stored = m_groups.back();
    for (int member = 0; member < stored.desired_size; ++member) {
      spawn_member(world, stored, config);
    }

    Engine::Core::EventManager::instance().publish(
        Engine::Core::MissionAnnouncementEvent(
            wave.label.empty()
                ? QCoreApplication::translate("WildlifeSystem",
                                              "Wolves are moving on the valley.")
                : QString::fromStdString(wave.label)));
  }
}

void WildlifeSystem::rebuild_threats(Engine::Core::World& world) {
  m_threats.clear();
  m_quarry.clear();
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == nullptr || is_wildlife_entity(*entity)) {
      continue;
    }
    if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr || unit->health <= 0) {
      continue;
    }
    if (entity->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    ThreatSource source;
    source.x = transform->position.x;
    source.z = transform->position.z;
    source.strength = threat_strength_of(*unit);
    source.civilian = is_civilian_spawn(unit->spawn_type);
    m_threats.add(source);

    QuarryRef quarry;
    quarry.id = entity->get_id();
    quarry.x = source.x;
    quarry.z = source.z;
    quarry.civilian = source.civilian;
    m_quarry.push_back(quarry);
  }
  m_threats.finalize();
}

void WildlifeSystem::collect_animals(Engine::Core::World& world) {
  m_animals.clear();
  m_group_runtime.assign(m_groups.size(), GroupRuntime{});

  std::vector<float> sum_x(m_groups.size(), 0.0F);
  std::vector<float> sum_z(m_groups.size(), 0.0F);

  for (auto* entity : world.collect_entities_with<Engine::Core::WildlifeComponent>()) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    const auto* wildlife = entity->get_component<Engine::Core::WildlifeComponent>();
    if (unit == nullptr || transform == nullptr || wildlife == nullptr ||
        unit->health <= 0) {
      continue;
    }

    AnimalRef ref;
    ref.entity = entity;
    ref.id = entity->get_id();
    ref.x = transform->position.x;
    ref.z = transform->position.z;
    ref.group = wildlife->group_id;
    ref.species = wildlife->species;
    m_animals.push_back(ref);

    for (std::size_t index = 0; index < m_groups.size(); ++index) {
      if (m_groups[index].id != wildlife->group_id) {
        continue;
      }
      auto& runtime = m_group_runtime[index];
      runtime.alive += 1;
      runtime.alarm = std::max(runtime.alarm, wildlife->alarm_timer);
      sum_x[index] += ref.x;
      sum_z[index] += ref.z;
      break;
    }
  }

  for (std::size_t index = 0; index < m_groups.size(); ++index) {
    auto& runtime = m_group_runtime[index];
    if (runtime.alive > 0) {
      runtime.center_x = sum_x[index] / static_cast<float>(runtime.alive);
      runtime.center_z = sum_z[index] / static_cast<float>(runtime.alive);
    } else {
      runtime.center_x = m_groups[index].home_x;
      runtime.center_z = m_groups[index].home_z;
    }
  }
}

auto WildlifeSystem::begin_bite(Engine::Core::Entity& entity,
                                Engine::Core::WildlifeComponent& wildlife,
                                const PreyRef& prey,
                                float hunter_x,
                                float hunter_z) -> bool {
  if (wildlife.state_timer > 0.0F || wildlife.bite_timer > 0.0F) {
    return false;
  }
  const auto* attack = entity.get_component<Engine::Core::AttackComponent>();
  if (attack == nullptr) {
    return false;
  }

  if (Game::Systems::Combat::structure_separates_positions(
          QVector3D(hunter_x, 0.0F, hunter_z), QVector3D(prey.x, 0.0F, prey.z))) {
    return false;
  }

  wildlife.state_timer = std::max(k_wolf_bite_recovery, attack->melee_cooldown);
  wildlife.bite_timer = Engine::Core::WildlifeComponent::k_bite_animation_seconds;
  wildlife.bite_target_id = prey.id;
  wildlife.bite_impact_pending = true;
  m_stats.bites += 1U;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::AudioCueEvent(Game::Audio::Cue::k_wildlife_wolf_bite));

  auto* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (transform != nullptr) {
    float const dx = prey.x - transform->position.x;
    float const dz = prey.z - transform->position.z;
    if ((dx * dx) + (dz * dz) > 1.0e-4F) {
      transform->desired_yaw = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
      transform->has_desired_yaw = true;
    }
  }
  return true;
}

void WildlifeSystem::try_contact_bite(Engine::Core::World& world,
                                      const AnimalRef& animal,
                                      Engine::Core::WildlifeComponent& wildlife) {
  if (wildlife.focus_id == 0 || wildlife.state_timer > 0.0F ||
      wildlife.bite_timer > 0.0F) {
    return;
  }
  PreyRef const prey = resolve_prey(world, wildlife.focus_id);
  if (!prey.valid()) {
    wildlife.focus_id = 0;
    return;
  }

  float const dx = prey.x - animal.x;
  float const dz = prey.z - animal.z;
  float const reach = k_wolf_bite_range + prey.radius;
  if ((dx * dx) + (dz * dz) > reach * reach) {
    return;
  }

  begin_bite(*animal.entity, wildlife, prey, animal.x, animal.z);
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

auto WildlifeSystem::attackers_on(Engine::Core::EntityID prey_id,
                                  Engine::Core::EntityID exclude_id) const -> int {
  if (prey_id == 0) {
    return 0;
  }
  int count = 0;
  for (const auto& animal : m_animals) {
    if (animal.species != Species::Wolf || animal.entity == nullptr ||
        animal.id == exclude_id) {
      continue;
    }
    const auto* wildlife =
        animal.entity->get_component<Engine::Core::WildlifeComponent>();
    if (wildlife != nullptr && wildlife->focus_id == prey_id) {
      count += 1;
    }
  }
  return count;
}

auto WildlifeSystem::pack_slot_for(Engine::Core::EntityID prey_id,
                                   Engine::Core::EntityID hunter_id) const -> PackSlot {
  PackSlot slot;
  slot.count = 1;
  slot.index = 0;
  for (const auto& animal : m_animals) {
    if (animal.species != Species::Wolf || animal.entity == nullptr ||
        animal.id == hunter_id) {
      continue;
    }
    const auto* wildlife =
        animal.entity->get_component<Engine::Core::WildlifeComponent>();
    if (wildlife == nullptr || wildlife->focus_id != prey_id) {
      continue;
    }
    slot.count += 1;
    if (animal.id < hunter_id) {
      slot.index += 1;
    }
  }
  return slot;
}

auto WildlifeSystem::nearest_prey(float world_x,
                                  float world_z,
                                  float radius,
                                  Engine::Core::EntityID hunter_id,
                                  float appetite) const -> const AnimalRef* {
  const AnimalRef* best = nullptr;
  float best_score = 0.0F;
  for (const auto& animal : m_animals) {
    if (animal.species != Species::Sheep) {
      continue;
    }
    float const dx = animal.x - world_x;
    float const dz = animal.z - world_z;
    float const distance_sq = (dx * dx) + (dz * dz);
    if (distance_sq >= radius * radius) {
      continue;
    }
    float const score =
        distance_sq + crowding_penalty(attackers_on(animal.id, hunter_id), appetite);
    if (best != nullptr && score >= best_score) {
      continue;
    }
    best_score = score;
    best = &animal;
  }
  return best;
}

auto WildlifeSystem::nearest_quarry(float world_x,
                                    float world_z,
                                    float radius,
                                    Engine::Core::EntityID hunter_id,
                                    float appetite) const -> const QuarryRef* {
  const QuarryRef* best = nullptr;
  float best_score = 0.0F;
  for (const auto& quarry : m_quarry) {
    float const dx = quarry.x - world_x;
    float const dz = quarry.z - world_z;
    float const distance_sq = (dx * dx) + (dz * dz);
    if (distance_sq >= radius * radius) {
      continue;
    }
    float const score =
        (distance_sq * (quarry.civilian ? k_civilian_quarry_preference : 1.0F)) +
        crowding_penalty(attackers_on(quarry.id, hunter_id), appetite);
    if (best != nullptr && score >= best_score) {
      continue;
    }
    best_score = score;
    best = &quarry;
  }
  return best;
}

void WildlifeSystem::alert_group(std::uint16_t group_id, float duration) {
  for (const auto& animal : m_animals) {
    if (animal.group != group_id || animal.entity == nullptr) {
      continue;
    }
    auto* wildlife = animal.entity->get_component<Engine::Core::WildlifeComponent>();
    if (wildlife != nullptr) {
      wildlife->alarm_timer = std::max(wildlife->alarm_timer, duration);
    }
  }
}

void WildlifeSystem::rally_pack(std::uint16_t group_id,
                                Engine::Core::EntityID foe_id,
                                float duration) {
  for (const auto& animal : m_animals) {
    if (animal.group != group_id || animal.species != Species::Wolf ||
        animal.entity == nullptr) {
      continue;
    }
    auto* wildlife = animal.entity->get_component<Engine::Core::WildlifeComponent>();
    if (wildlife == nullptr) {
      continue;
    }
    wildlife->hostile_timer = std::max(wildlife->hostile_timer, duration);
    if (wildlife->aggressor_id == 0) {
      wildlife->aggressor_id = foe_id;
    }
  }
}

void WildlifeSystem::issue_move(Engine::Core::World& world,
                                Engine::Core::EntityID entity_id,
                                float world_x,
                                float world_z) {
  QVector3D const destination = Game::Systems::NavGrid::snap_to_walkable_ground(
      QVector3D(world_x, 0.0F, world_z));

  auto* entity = world.get_entity(entity_id);
  if (entity != nullptr) {
    const auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    if (movement != nullptr && movement->get_has_target()) {
      float const dx = movement->get_goal_x() - destination.x();
      float const dz = movement->get_goal_y() - destination.z();
      if ((dx * dx) + (dz * dz) <= k_move_reissue_epsilon * k_move_reissue_epsilon) {
        return;
      }
    }
  }

  Game::Systems::CommandService::MoveOptions options;
  options.kind = Game::Systems::MoveOrderKind::ScriptedMove;
  Game::Systems::CommandService::move_unit(world, entity_id, destination, options);
}

void WildlifeSystem::set_travel_speed(Engine::Core::Entity& entity,
                                      const SpeciesConfig& config,
                                      bool urgent) {
  auto* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return;
  }
  unit->speed = urgent ? config.flee_speed : config.move_speed;
}

class WildlifeSystem::NatureActionAdapter final : public NatureActions {
public:
  explicit NatureActionAdapter(WildlifeSystem& owner, Engine::Core::World& world)
      : m_owner(owner)
      , m_world(world) {}

  void move_to(const NatureContext& ctx, float world_x, float world_z) override {
    m_owner.issue_move(m_world, ctx.entity->get_id(), world_x, world_z);
  }

  [[nodiscard]] auto bypass_around_obstacle(const NatureContext& ctx,
                                            float prey_x,
                                            float prey_z,
                                            float standoff)
      -> std::optional<std::pair<float, float>> override {
    QVector3D const from(ctx.x, 0.0F, ctx.z);
    QVector3D const prey(prey_x, 0.0F, prey_z);
    if (!Game::Systems::Combat::structure_separates_positions(from, prey)) {
      return std::nullopt;
    }

    auto const around =
        Game::Systems::Combat::melee_bypass_destination(from, prey, standoff, 0.0F);
    if (!around.has_value()) {
      return std::nullopt;
    }
    return std::make_pair(around->x(), around->z());
  }

  void halt(const NatureContext& ctx) override {
    if (ctx.movement != nullptr) {
      ctx.movement->stop();
    }
  }

  void face_toward(const NatureContext& ctx, float world_x, float world_z) override {
    auto* transform = ctx.entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      return;
    }
    float const dx = world_x - transform->position.x;
    float const dz = world_z - transform->position.z;
    if ((dx * dx) + (dz * dz) < 1e-4F) {
      return;
    }
    transform->desired_yaw = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
    transform->has_desired_yaw = true;
  }

  void set_travel_speed(const NatureContext& ctx, bool urgent) override {
    m_owner.set_travel_speed(*ctx.entity, *ctx.config, urgent);
  }

  void alert_herd(std::uint16_t group_id, float duration) override {
    m_owner.alert_group(group_id, duration);
  }

  void mark_hostile(const NatureContext& ctx,
                    Engine::Core::EntityID foe_id,
                    bool rally_pack) override {
    auto& wildlife = *ctx.wildlife;
    wildlife.hostile_timer = Game::Wildlife::k_hostility_duration;
    wildlife.aggressor_id = foe_id;
    if (rally_pack) {
      m_owner.rally_pack(
          wildlife.group_id, foe_id, Game::Wildlife::k_hostility_duration);
    }
  }

  void note(NatureEvent event) override {
    switch (event) {
    case NatureEvent::Flee:
      m_owner.m_stats.flee_events += 1U;
      break;
    case NatureEvent::Hunt:
      m_owner.m_stats.hunt_events += 1U;
      Engine::Core::EventManager::instance().publish(
          Engine::Core::AudioCueEvent(Game::Audio::Cue::k_wildlife_wolf_hunt));
      break;
    }
  }

  auto pick_open_point(std::uint32_t& rng,
                       float origin_x,
                       float origin_z,
                       float min_radius,
                       float max_radius,
                       float& out_x,
                       float& out_z) -> bool override {
    return m_owner.pick_open_point(
        rng, origin_x, origin_z, min_radius, max_radius, out_x, out_z);
  }

  auto nearest_prey(const NatureContext& ctx, float radius) -> PreyRef override {
    const AnimalRef* found = m_owner.nearest_prey(
        ctx.x, ctx.z, radius, ctx.entity->get_id(), ctx.config->aggression);
    if (found == nullptr) {
      return {};
    }
    return resolve_prey(m_world, found->id);
  }

  auto nearest_quarry(const NatureContext& ctx, float radius) -> PreyRef override {
    const QuarryRef* found = m_owner.nearest_quarry(
        ctx.x, ctx.z, radius, ctx.entity->get_id(), ctx.config->aggression);
    if (found == nullptr) {
      return {};
    }
    return resolve_prey(m_world, found->id);
  }

  auto locate(Engine::Core::EntityID entity_id) -> PreyRef override {
    return resolve_prey(m_world, entity_id);
  }

  auto claim_pack_slot(const NatureContext& ctx,
                       const PreyRef& prey) -> PackSlot override {
    return m_owner.pack_slot_for(prey.id, ctx.entity->get_id());
  }

  auto nearest_pack_hunter(float world_x,
                           float world_z,
                           float radius) -> ThreatQuery override {
    ThreatQuery out;
    for (const auto& animal : m_owner.m_animals) {
      if (animal.species != Species::Wolf) {
        continue;
      }
      float const dx = animal.x - world_x;
      float const dz = animal.z - world_z;
      float const distance = std::sqrt((dx * dx) + (dz * dz));
      if (distance > radius) {
        continue;
      }
      if (!out.found || distance < out.distance) {
        out.found = true;
        out.x = animal.x;
        out.z = animal.z;
        out.distance = distance;
        out.strength = 1.0F;
      }
    }
    return out;
  }

  auto bite(const NatureContext& ctx, const PreyRef& prey) -> bool override {
    return m_owner.begin_bite(*ctx.entity, *ctx.wildlife, prey, ctx.x, ctx.z);
  }

private:
  WildlifeSystem& m_owner;
  Engine::Core::World& m_world;
};

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
  ctx.threats = &m_threats;
  ctx.herd.center_x = runtime.center_x;
  ctx.herd.center_z = runtime.center_z;
  ctx.herd.alarm = runtime.alarm;
  ctx.herd.alive = runtime.alive;
  ctx.x = transform->position.x;
  ctx.z = transform->position.z;

  NatureActionAdapter adapter(*this, world);
  NatureBrain& brain = species == Species::Wolf ? m_wolf_brain : m_sheep_brain;
  brain.tick(ctx, adapter);
}

void WildlifeSystem::update_respawns(Engine::Core::World& world, float delta_time) {
  for (std::size_t index = 0; index < m_groups.size(); ++index) {
    auto& group = m_groups[index];
    const auto& config = m_settings.for_species(group.species);
    int const alive = index < m_group_runtime.size() ? m_group_runtime[index].alive : 0;

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
      m_stats.respawns += 1U;
    }
  }
}

void WildlifeSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr || !m_enabled || delta_time <= 0.0F) {
    return;
  }

  if (m_spawn_pending) {
    m_spawn_pending = false;
    if (!m_restored) {
      spawn_initial_population(*world);
    }
  }

  release_due_packs(*world, delta_time);

  m_threat_refresh -= delta_time;
  if (m_threat_refresh <= 0.0F) {
    m_threat_refresh = k_threat_refresh_interval;
    rebuild_threats(*world);
  }

  collect_animals(*world);

  for (const auto& animal : m_animals) {
    if (animal.entity == nullptr) {
      continue;
    }
    auto* wildlife = animal.entity->get_component<Engine::Core::WildlifeComponent>();
    if (wildlife == nullptr) {
      continue;
    }

    Tier const tier = tier_for(animal.x, animal.z);
    if (tier == Tier::Dormant) {
      m_stats.dormant_skips += 1U;
      continue;
    }

    wildlife->state_timer = std::max(0.0F, wildlife->state_timer - delta_time);
    wildlife->alarm_timer = std::max(0.0F, wildlife->alarm_timer - delta_time);
    wildlife->flinch_timer = std::max(0.0F, wildlife->flinch_timer - delta_time);
    if (const auto* unit =
            animal.entity->get_component<Engine::Core::UnitComponent>()) {
      if (wildlife->watched_health >= 0 && unit->health < wildlife->watched_health) {
        wildlife->flinch_timer =
            Engine::Core::WildlifeComponent::k_flinch_animation_seconds;
      }
      wildlife->watched_health = unit->health;
    }
    wildlife->hostile_timer = std::max(0.0F, wildlife->hostile_timer - delta_time);
    float const bite_timer_before = wildlife->bite_timer;
    wildlife->bite_timer = std::max(0.0F, wildlife->bite_timer - delta_time);
    constexpr float k_bite_impact_remaining =
        Engine::Core::WildlifeComponent::k_bite_animation_seconds *
        (1.0F - Engine::Core::WildlifeComponent::k_bite_impact_phase);
    if (wildlife->bite_impact_pending && bite_timer_before > k_bite_impact_remaining &&
        wildlife->bite_timer <= k_bite_impact_remaining) {
      PreyRef const prey = resolve_prey(*world, wildlife->bite_target_id);
      auto const* wolf_transform =
          animal.entity->get_component<Engine::Core::TransformComponent>();
      auto const* attack =
          animal.entity->get_component<Engine::Core::AttackComponent>();
      if (prey.valid() && wolf_transform != nullptr && attack != nullptr) {
        float const dx = prey.x - wolf_transform->position.x;
        float const dz = prey.z - wolf_transform->position.z;
        float const contact_reach = k_wolf_bite_range + prey.radius + 0.30F;

        bool const walled_off = Game::Systems::Combat::structure_separates_positions(
            QVector3D(wolf_transform->position.x, 0.0F, wolf_transform->position.z),
            QVector3D(prey.x, 0.0F, prey.z));
        if (!walled_off && (dx * dx) + (dz * dz) <= contact_reach * contact_reach) {
          const auto damage = Game::Systems::Combat::apply_unit_damage(
              world, prey.entity, attack->melee_damage, animal.entity->get_id());

          const auto* prey_unit =
              prey.entity->get_component<Engine::Core::UnitComponent>();
          if (prey_unit != nullptr &&
              prey_unit->spawn_type == Game::Units::SpawnType::Civilian) {
            Game::Systems::Combat::add_or_extend_stagger(
                prey.entity,
                k_wolf_bite_flinch_seconds,
                Engine::Core::StaggerTier::LightFlinch);
          }

          auto const bite_salt =
              static_cast<std::uint32_t>((animal.id * 2654435761ULL) + m_stats.bites);
          if (damage.applied_damage > 0 && !damage.killed &&
              hash_unit_interval(bite_salt) < k_wolf_bite_blood_chance) {
            Game::Systems::Combat::spawn_blood_stain(
                world, prey.entity, k_wolf_bite_blood_spread, bite_salt);
          }
        }
      }
      wildlife->bite_impact_pending = false;
      wildlife->bite_target_id = 0;
    } else if (wildlife->bite_timer <= 0.0F) {
      wildlife->bite_impact_pending = false;
      wildlife->bite_target_id = 0;
    }
    if (wildlife->hostile_timer <= 0.0F) {
      wildlife->aggressor_id = 0;
    }

    if (animal.species == Species::Wolf) {
      try_contact_bite(*world, animal, *wildlife);
    }

    if (wildlife->held_timer > 0.0F) {
      wildlife->held_timer -= delta_time;
      if (auto* movement =
              animal.entity->get_component<Engine::Core::MovementComponent>();
          movement != nullptr && movement->get_has_target()) {
        movement->stop();
      }
      wildlife->think_cooldown = std::max(wildlife->think_cooldown, 0.25F);
      continue;
    }

    release_if_stalled(animal, *wildlife, delta_time);

    wildlife->think_cooldown -= delta_time;
    if (wildlife->think_cooldown > 0.0F) {
      continue;
    }

    float const multiplier = tier == Tier::Far ? k_far_think_multiplier : 1.0F;
    if (wildlife->rng_state == 0U) {
      wildlife->rng_state =
          static_cast<std::uint32_t>((animal.id * 2654435761ULL) | 1ULL);
    }
    wildlife->think_cooldown =
        (k_think_interval * multiplier) +
        random_range(wildlife->rng_state, 0.0F, k_think_interval * 0.5F);

    if (tier == Tier::Near) {
      m_stats.near_thinks += 1U;
    } else {
      m_stats.far_thinks += 1U;
    }

    GroupRuntime runtime;
    for (std::size_t index = 0; index < m_groups.size(); ++index) {
      if (m_groups[index].id == wildlife->group_id && index < m_group_runtime.size()) {
        runtime = m_group_runtime[index];
        break;
      }
    }

    const SpeciesConfig& config =
        wildlife->species == Species::Wolf ? m_settings.wolves : m_settings.sheep;
    think(*world, *animal.entity, runtime, config, wildlife->species);
  }

  update_respawns(*world, delta_time);

  BirdFlockManager::instance().update(delta_time, m_threats);
}

auto WildlifeSystem::serialize_state() const -> QJsonObject {
  QJsonObject state;
  state["enabled"] = m_enabled;
  state["seed"] = static_cast<qint64>(m_seed);
  state["next_group_id"] = static_cast<int>(m_next_group_id);
  state["elapsed"] = static_cast<double>(m_elapsed);

  QJsonArray released_waves;
  for (bool const released : m_released_waves) {
    released_waves.append(released);
  }
  state["released_waves"] = released_waves;

  QJsonArray groups;
  for (const auto& group : m_groups) {
    QJsonObject group_obj;
    group_obj["id"] = static_cast<int>(group.id);
    group_obj["species"] = QString::fromUtf8(species_name(group.species).data());
    group_obj["home_x"] = static_cast<double>(group.home_x);
    group_obj["home_z"] = static_cast<double>(group.home_z);
    group_obj["roam_radius"] = static_cast<double>(group.roam_radius);
    group_obj["desired_size"] = group.desired_size;
    group_obj["respawn_timer"] = static_cast<double>(group.respawn_timer);
    group_obj["rng_state"] = static_cast<qint64>(group.rng_state);
    groups.append(group_obj);
  }
  state["groups"] = groups;

  const auto& flock_manager = BirdFlockManager::instance();
  QJsonArray flocks;
  for (const auto& flock : flock_manager.flocks()) {
    QJsonObject flock_obj;
    flock_obj["home_x"] = static_cast<double>(flock.home_x);
    flock_obj["home_z"] = static_cast<double>(flock.home_z);
    flock_obj["roam_radius"] = static_cast<double>(flock.roam_radius);
    flock_obj["target_x"] = static_cast<double>(flock.target_x);
    flock_obj["target_z"] = static_cast<double>(flock.target_z);
    flock_obj["retarget_timer"] = static_cast<double>(flock.retarget_timer);
    flock_obj["alarm_timer"] = static_cast<double>(flock.alarm_timer);
    flock_obj["rng_state"] = static_cast<qint64>(flock.rng_state);
    flock_obj["heading_x"] = static_cast<double>(flock.heading_x);
    flock_obj["heading_z"] = static_cast<double>(flock.heading_z);
    flock_obj["respite_timer"] = static_cast<double>(flock.respite_timer);
    flock_obj["airborne_seconds"] = static_cast<double>(flock.airborne_seconds);
    flock_obj["airborne"] = flock.airborne;
    flocks.append(flock_obj);
  }
  state["flocks"] = flocks;

  QJsonArray birds;
  for (const auto& bird : flock_manager.birds()) {
    QJsonObject bird_obj;
    bird_obj["x"] = static_cast<double>(bird.x);
    bird_obj["y"] = static_cast<double>(bird.y);
    bird_obj["z"] = static_cast<double>(bird.z);
    bird_obj["yaw"] = static_cast<double>(bird.yaw);
    bird_obj["target_x"] = static_cast<double>(bird.target_x);
    bird_obj["target_z"] = static_cast<double>(bird.target_z);
    bird_obj["altitude"] = static_cast<double>(bird.altitude);
    bird_obj["target_altitude"] = static_cast<double>(bird.target_altitude);
    bird_obj["speed"] = static_cast<double>(bird.speed);
    bird_obj["phase"] = static_cast<double>(bird.phase);
    bird_obj["glide"] = static_cast<double>(bird.glide);
    bird_obj["bank"] = static_cast<double>(bird.bank);
    bird_obj["think_timer"] = static_cast<double>(bird.think_timer);
    bird_obj["state_timer"] = static_cast<double>(bird.state_timer);
    bird_obj["slot_lateral"] = static_cast<double>(bird.slot_lateral);
    bird_obj["slot_trail"] = static_cast<double>(bird.slot_trail);
    bird_obj["rng_state"] = static_cast<qint64>(bird.rng_state);
    bird_obj["flock"] = static_cast<int>(bird.flock);
    bird_obj["behavior"] = QString::fromUtf8(behavior_name(bird.behavior).data());
    bird_obj["tint"] = static_cast<int>(bird.tint);
    birds.append(bird_obj);
  }
  state["birds"] = birds;

  return state;
}

void WildlifeSystem::restore_state(const QJsonObject& state) {
  if (state.isEmpty()) {
    return;
  }

  m_enabled = state.value("enabled").toBool(m_enabled);
  m_seed = static_cast<std::uint32_t>(state.value("seed").toVariant().toULongLong());
  m_next_group_id =
      static_cast<std::uint16_t>(state.value("next_group_id").toInt(m_next_group_id));
  m_elapsed = static_cast<float>(state.value("elapsed").toDouble(0.0));

  m_released_waves.clear();
  for (const auto value : state.value("released_waves").toArray()) {
    m_released_waves.push_back(value.toBool(false));
  }

  m_groups.clear();
  const QJsonArray groups = state.value("groups").toArray();
  for (const auto value : groups) {
    const QJsonObject group_obj = value.toObject();
    GroupState group;
    group.id = static_cast<std::uint16_t>(group_obj.value("id").toInt(0));
    Species species = Species::Sheep;
    if (try_parse_species(group_obj.value("species").toString().toStdString(),
                          species)) {
      group.species = species;
    }
    group.home_x = static_cast<float>(group_obj.value("home_x").toDouble(0.0));
    group.home_z = static_cast<float>(group_obj.value("home_z").toDouble(0.0));
    group.roam_radius =
        static_cast<float>(group_obj.value("roam_radius").toDouble(14.0));
    group.desired_size = group_obj.value("desired_size").toInt(0);
    group.respawn_timer =
        static_cast<float>(group_obj.value("respawn_timer").toDouble(0.0));
    group.rng_state = static_cast<std::uint32_t>(
        group_obj.value("rng_state").toVariant().toULongLong());
    if (group.rng_state == 0U) {
      group.rng_state = 1U;
    }
    m_groups.push_back(group);
  }
  m_group_runtime.assign(m_groups.size(), GroupRuntime{});

  BirdPopulation population;
  const QJsonArray flocks = state.value("flocks").toArray();
  for (const auto value : flocks) {
    const QJsonObject flock_obj = value.toObject();
    Flock flock;
    flock.home_x = static_cast<float>(flock_obj.value("home_x").toDouble(0.0));
    flock.home_z = static_cast<float>(flock_obj.value("home_z").toDouble(0.0));
    flock.roam_radius =
        static_cast<float>(flock_obj.value("roam_radius").toDouble(30.0));
    flock.target_x = static_cast<float>(flock_obj.value("target_x").toDouble(0.0));
    flock.target_z = static_cast<float>(flock_obj.value("target_z").toDouble(0.0));
    flock.retarget_timer =
        static_cast<float>(flock_obj.value("retarget_timer").toDouble(0.0));
    flock.alarm_timer =
        static_cast<float>(flock_obj.value("alarm_timer").toDouble(0.0));
    flock.rng_state = static_cast<std::uint32_t>(
        flock_obj.value("rng_state").toVariant().toULongLong());
    flock.heading_x = static_cast<float>(flock_obj.value("heading_x").toDouble(1.0));
    flock.heading_z = static_cast<float>(flock_obj.value("heading_z").toDouble(0.0));
    flock.respite_timer =
        static_cast<float>(flock_obj.value("respite_timer").toDouble(0.0));
    flock.airborne_seconds =
        static_cast<float>(flock_obj.value("airborne_seconds").toDouble(0.0));
    flock.airborne = flock_obj.value("airborne").toBool(true);
    population.flocks.push_back(flock);
  }

  const QJsonArray birds = state.value("birds").toArray();
  for (const auto value : birds) {
    const QJsonObject bird_obj = value.toObject();
    Bird bird;
    bird.x = static_cast<float>(bird_obj.value("x").toDouble(0.0));
    bird.y = static_cast<float>(bird_obj.value("y").toDouble(0.0));
    bird.z = static_cast<float>(bird_obj.value("z").toDouble(0.0));
    bird.yaw = static_cast<float>(bird_obj.value("yaw").toDouble(0.0));
    bird.target_x = static_cast<float>(bird_obj.value("target_x").toDouble(0.0));
    bird.target_z = static_cast<float>(bird_obj.value("target_z").toDouble(0.0));
    bird.altitude = static_cast<float>(bird_obj.value("altitude").toDouble(0.0));
    bird.target_altitude =
        static_cast<float>(bird_obj.value("target_altitude").toDouble(0.0));
    bird.speed = static_cast<float>(bird_obj.value("speed").toDouble(0.0));
    bird.phase = static_cast<float>(bird_obj.value("phase").toDouble(0.0));
    bird.glide = static_cast<float>(bird_obj.value("glide").toDouble(0.0));
    bird.bank = static_cast<float>(bird_obj.value("bank").toDouble(0.0));
    bird.think_timer = static_cast<float>(bird_obj.value("think_timer").toDouble(0.0));
    bird.state_timer = static_cast<float>(bird_obj.value("state_timer").toDouble(0.0));
    bird.slot_lateral =
        static_cast<float>(bird_obj.value("slot_lateral").toDouble(0.0));
    bird.slot_trail = static_cast<float>(bird_obj.value("slot_trail").toDouble(0.0));
    bird.rng_state = static_cast<std::uint32_t>(
        bird_obj.value("rng_state").toVariant().toULongLong());
    bird.flock = static_cast<std::uint16_t>(bird_obj.value("flock").toInt(0));
    Behavior behavior = Behavior::Cruise;
    if (try_parse_behavior(bird_obj.value("behavior").toString().toStdString(),
                           behavior)) {
      bird.behavior = behavior;
    }
    bird.tint = static_cast<std::uint8_t>(bird_obj.value("tint").toInt(0));
    population.birds.push_back(bird);
  }

  BirdFlockManager::instance().restore(population);

  m_restored = true;
  m_spawn_pending = false;
}

} // namespace Game::Wildlife
