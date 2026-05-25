#include "cohort_system.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"

namespace Game::Systems {

namespace {

auto is_idle_or_guarding(Engine::Core::Entity* entity) -> bool {
  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  if (movement != nullptr && movement->has_target) {
    return false;
  }
  auto* atk = entity->get_component<Engine::Core::AttackComponent>();
  if (atk != nullptr && atk->in_melee_lock) {
    return false;
  }
  return true;
}

} // namespace

void CohortSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr || delta_time <= 0.0F) {
    return;
  }

  m_diagnostics = {};

  // Reform cohorts periodically.
  m_reform_timer -= delta_time;
  bool const should_reform = m_reform_timer <= 0.0F;
  if (should_reform) {
    m_reform_timer = k_reform_interval;
  }

  auto entities = world->get_entities_with<Engine::Core::UnitComponent>();

  // Phase 1: Reform cohorts if timer elapsed.
  if (should_reform) {
    // Clear old cohort assignments.
    for (auto* entity : entities) {
      auto* membership =
          entity->get_component<Engine::Core::CohortMembershipComponent>();
      if (membership != nullptr) {
        membership->cohort_id = 0;
        membership->cohort_activated = false;
      }
    }

    // Group by owner + idle status.
    struct UnitInfo {
      Engine::Core::Entity* entity;
      float x;
      float z;
      int owner_id;
    };
    std::vector<UnitInfo> candidates;
    for (auto* entity : entities) {
      auto* unit = entity->get_component<Engine::Core::UnitComponent>();
      if (unit == nullptr || unit->health <= 0) {
        continue;
      }
      if (entity->has_component<Engine::Core::BuildingComponent>()) {
        continue;
      }
      if (entity->has_component<Engine::Core::PendingRemovalComponent>()) {
        continue;
      }
      if (!is_idle_or_guarding(entity)) {
        continue;
      }

      auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      if (transform == nullptr) {
        continue;
      }

      candidates.push_back(
          {entity, transform->position.x, transform->position.z, unit->owner_id});
    }

    // Simple greedy clustering by proximity.
    std::unordered_set<std::size_t> assigned;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
      if (assigned.count(i) > 0) {
        continue;
      }

      std::uint32_t cohort_id = m_next_cohort_id++;
      std::vector<std::size_t> members;
      members.push_back(i);
      assigned.insert(i);

      for (std::size_t j = i + 1;
           j < candidates.size() && members.size() < k_max_cohort_size; ++j) {
        if (assigned.count(j) > 0) {
          continue;
        }
        if (candidates[j].owner_id != candidates[i].owner_id) {
          continue;
        }
        float const dx = candidates[j].x - candidates[i].x;
        float const dz = candidates[j].z - candidates[i].z;
        if (dx * dx + dz * dz <= k_cohort_radius * k_cohort_radius) {
          members.push_back(j);
          assigned.insert(j);
        }
      }

      for (std::size_t idx : members) {
        auto* membership = candidates[idx].entity->get_component<
            Engine::Core::CohortMembershipComponent>();
        if (membership == nullptr) {
          membership = candidates[idx].entity->add_component<
              Engine::Core::CohortMembershipComponent>();
        }
        if (membership != nullptr) {
          membership->cohort_id = cohort_id;
          membership->cohort_activated = false;
        }
      }

      ++m_diagnostics.cohorts_formed;
      m_diagnostics.units_in_cohorts +=
          static_cast<std::uint32_t>(members.size());
    }
  }

  // Phase 2: Propagate activation.
  // If any unit in a cohort has detected an enemy (has AttackTargetComponent),
  // activate the entire cohort.
  std::unordered_set<std::uint32_t> activated_cohorts;
  for (auto* entity : entities) {
    auto* membership =
        entity->get_component<Engine::Core::CohortMembershipComponent>();
    if (membership == nullptr || membership->cohort_id == 0) {
      continue;
    }
    if (entity->has_component<Engine::Core::AttackTargetComponent>()) {
      activated_cohorts.insert(membership->cohort_id);
    }
  }

  for (auto* entity : entities) {
    auto* membership =
        entity->get_component<Engine::Core::CohortMembershipComponent>();
    if (membership == nullptr || membership->cohort_id == 0) {
      continue;
    }
    if (activated_cohorts.count(membership->cohort_id) > 0) {
      if (!membership->cohort_activated) {
        membership->cohort_activated = true;
        ++m_diagnostics.cohorts_activated;
      }
    }
  }
}

} // namespace Game::Systems
