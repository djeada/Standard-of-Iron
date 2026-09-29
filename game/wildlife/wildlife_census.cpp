#include "wildlife_census.h"

#include <algorithm>
#include <cmath>

#include "../core/component.h"
#include "../core/world.h"
#include "../units/spawn_type.h"

namespace Game::Wildlife {

namespace {

constexpr float k_civilian_threat_strength = 0.3F;
constexpr float k_civilian_quarry_preference = 0.5F;
constexpr int k_pack_gang_size = 3;
constexpr float k_pack_join_gain = 30.0F;
constexpr float k_pack_second_join_gain = 9.0F;
constexpr float k_quarry_crowd_penalty = 400.0F;
constexpr float k_troop_threat_strength = 1.0F;

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

} // namespace

auto is_civilian_spawn(Game::Units::SpawnType type) -> bool {
  return type == Game::Units::SpawnType::Civilian ||
         type == Game::Units::SpawnType::Builder;
}

void WildlifeCensus::reset() {
  m_group_runtime.clear();
  m_animals.clear();
  m_quarry.clear();
  m_threats.clear();
}

auto WildlifeCensus::tier_for(float world_x,
                              float world_z,
                              float near_radius,
                              float far_radius) const -> Tier {
  if (m_interest.empty() || m_interest.any_within(world_x, world_z, near_radius)) {
    return Tier::Near;
  }
  return m_interest.any_within(world_x, world_z, far_radius) ? Tier::Far
                                                             : Tier::Dormant;
}

void WildlifeCensus::rebuild_threats(Engine::Core::World& world) {
  m_threats.clear();
  m_quarry.clear();
  m_interest.clear();
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

    ThreatSource anchor;
    anchor.x = transform->position.x;
    anchor.z = transform->position.z;
    anchor.strength = 1.0F;
    anchor.civilian = false;
    m_interest.add(anchor);

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
  m_interest.finalize();
}

void WildlifeCensus::collect_animals(Engine::Core::World& world,
                                     const std::vector<GroupState>& groups) {
  m_animals.clear();
  m_group_runtime.assign(groups.size(), GroupRuntime{});

  std::vector<float> sum_x(groups.size(), 0.0F);
  std::vector<float> sum_z(groups.size(), 0.0F);

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

    for (std::size_t index = 0; index < groups.size(); ++index) {
      if (groups[index].id != wildlife->group_id) {
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

  for (std::size_t index = 0; index < groups.size(); ++index) {
    auto& runtime = m_group_runtime[index];
    if (runtime.alive > 0) {
      runtime.center_x = sum_x[index] / static_cast<float>(runtime.alive);
      runtime.center_z = sum_z[index] / static_cast<float>(runtime.alive);
    } else {
      runtime.center_x = groups[index].home_x;
      runtime.center_z = groups[index].home_z;
    }
  }
}

auto WildlifeCensus::attackers_on(Engine::Core::EntityID prey_id,
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

auto WildlifeCensus::pack_slot_for(Engine::Core::EntityID prey_id,
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

auto WildlifeCensus::nearest_prey(float world_x,
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

auto WildlifeCensus::nearest_quarry(float world_x,
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

void WildlifeCensus::alert_group(std::uint16_t group_id, float duration) {
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

void WildlifeCensus::rally_pack(std::uint16_t group_id,
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

auto WildlifeCensus::nearest_pack_hunter(float world_x,
                                         float world_z,
                                         float radius) const -> ThreatQuery {
  ThreatQuery out;
  for (const auto& animal : m_animals) {
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

auto WildlifeCensus::runtime_of(std::size_t group_index) const -> const GroupRuntime* {
  return group_index < m_group_runtime.size() ? &m_group_runtime[group_index] : nullptr;
}

auto WildlifeCensus::alive_in(std::size_t group_index) const -> int {
  return group_index < m_group_runtime.size() ? m_group_runtime[group_index].alive : 0;
}

auto WildlifeCensus::runtime_for_group(const std::vector<GroupState>& groups,
                                       std::uint16_t group_id) const -> GroupRuntime {
  for (std::size_t index = 0; index < groups.size(); ++index) {
    if (groups[index].id == group_id && index < m_group_runtime.size()) {
      return m_group_runtime[index];
    }
  }
  return {};
}

} // namespace Game::Wildlife
