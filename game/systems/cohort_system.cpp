#include "cohort_system.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/system_context.h"
#include "../core/world.h"

namespace Game::Systems {

namespace {

auto is_idle_or_guarding(Engine::Core::SystemContext& context,
                         Engine::Core::EntityID entity_id) -> bool {
  const auto* movement = context.try_get<Engine::Core::MovementComponent>(entity_id);
  if (movement != nullptr && movement->get_has_target()) {
    return false;
  }
  const auto* atk = context.try_get<Engine::Core::AttackComponent>(entity_id);
  return atk == nullptr || !atk->in_melee_lock;
}

} // namespace

void CohortSystem::run(Engine::Core::SystemContext& context) {
  const float delta_time = context.delta_time();
  if (delta_time <= 0.0F) {
    return;
  }

  m_diagnostics = {};

  m_reform_timer -= delta_time;
  bool const should_reform = m_reform_timer <= 0.0F;
  if (should_reform) {
    m_reform_timer = k_reform_interval;
  }

  if (should_reform) {

    for (auto [entity_id, membership] :
         context.view<Engine::Core::CohortMembershipComponent>()) {
      (void)entity_id;
      membership.cohort_id = 0;
      membership.cohort_activated = false;
    }

    struct UnitInfo {
      Engine::Core::EntityID entity_id;
      float x;
      float z;
      int owner_id;
    };
    std::vector<UnitInfo> candidates;
    for (auto [entity_id, unit, transform] :
         context
             .view<Engine::Core::UnitComponent, Engine::Core::TransformComponent>()) {
      if (unit.health <= 0) {
        continue;
      }
      if (context.has<Engine::Core::BuildingComponent>(entity_id) ||
          context.has<Engine::Core::PendingRemovalComponent>(entity_id)) {
        continue;
      }
      if (!is_idle_or_guarding(context, entity_id)) {
        continue;
      }

      candidates.push_back(
          {entity_id, transform.position.x, transform.position.z, unit.owner_id});
    }

    std::unordered_set<std::size_t> assigned;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
      if (assigned.contains(i)) {
        continue;
      }

      std::uint32_t const cohort_id = m_next_cohort_id++;
      std::vector<std::size_t> members;
      members.push_back(i);
      assigned.insert(i);

      for (std::size_t j = i + 1;
           j < candidates.size() && members.size() < k_max_cohort_size;
           ++j) {
        if (assigned.contains(j)) {
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

      for (std::size_t const idx : members) {
        auto* membership = context.emplace<Engine::Core::CohortMembershipComponent>(
            candidates[idx].entity_id);
        if (membership != nullptr) {
          membership->cohort_id = cohort_id;
          membership->cohort_activated = false;
        }
      }

      ++m_diagnostics.cohorts_formed;
      m_diagnostics.units_in_cohorts += static_cast<std::uint32_t>(members.size());
    }
  }

  std::unordered_set<std::uint32_t> activated_cohorts;
  for (auto [entity_id, membership] :
       context.view<Engine::Core::CohortMembershipComponent>()) {
    if (membership.cohort_id == 0) {
      continue;
    }
    if (context.has<Engine::Core::AttackTargetComponent>(entity_id)) {
      activated_cohorts.insert(membership.cohort_id);
    }
  }

  for (auto [entity_id, membership] :
       context.view<Engine::Core::CohortMembershipComponent>()) {
    (void)entity_id;
    if (membership.cohort_id == 0 ||
        !activated_cohorts.contains(membership.cohort_id)) {
      continue;
    }
    if (!membership.cohort_activated) {
      membership.cohort_activated = true;
      ++m_diagnostics.cohorts_activated;
    }
  }
}

auto CohortSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<UnitComponent,
                                     TransformComponent,
                                     MovementComponent,
                                     AttackComponent,
                                     AttackTargetComponent,
                                     BuildingComponent,
                                     PendingRemovalComponent>{},
                               Writes<CohortMembershipComponent>{});
}

} // namespace Game::Systems
