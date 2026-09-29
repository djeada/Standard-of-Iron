#include "attack_eligibility.h"

#include <algorithm>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../healing_rules.h"
#include "combat_types.h"
#include "combat_utils.h"
#include "target_rules.h"

namespace Game::Systems::Combat {

namespace {

auto matches_heal_affinity(const Engine::Core::Entity* target,
                           Engine::Core::HealerComponent::TargetAffinity affinity)
    -> bool {
  if (target == nullptr) {
    return false;
  }

  bool const target_is_undead = target->has_component<Engine::Core::UndeadComponent>();
  switch (affinity) {
  case Engine::Core::HealerComponent::TargetAffinity::UndeadAllies:
    return target_is_undead;
  case Engine::Core::HealerComponent::TargetAffinity::LivingAllies:
  default:
    return !target_is_undead;
  }
}

} // namespace

auto should_prioritize_healing(Engine::Core::Entity* healer,
                               const CombatQueryContext& query_context) -> bool {
  auto* healer_component = healer->get_component<Engine::Core::HealerComponent>();
  auto* healer_unit = healer->get_component<Engine::Core::UnitComponent>();
  auto* healer_transform = healer->get_component<Engine::Core::TransformComponent>();
  if (query_context.world == nullptr || healer_component == nullptr ||
      healer_unit == nullptr || healer_transform == nullptr ||
      !healer_component->suppress_attack_while_healing ||
      healer_component->time_since_last_heal < healer_component->healing_cooldown ||
      in_rts_melee_lock(healer)) {
    return false;
  }

  float const range = healer_component->healing_range;
  bool found = false;
  query_context.world->spatial_index().for_each_in_radius(
      healer_transform->position.x,
      healer_transform->position.z,
      range + k_combat_query_stale_margin,
      [&](const Engine::Core::WorldSpatialIndex::Entry& entry) {
        auto* target = found ? nullptr : query_context.find_entity(entry.id);
        if (target == nullptr ||
            target->has_component<Engine::Core::PendingRemovalComponent>()) {
          return;
        }
        auto const& position =
            target->get_component<Engine::Core::TransformComponent>()->position;
        float const dx = position.x - healer_transform->position.x;
        float const dz = position.z - healer_transform->position.z;
        found = dx * dx + dz * dz <= range * range &&
                target->get_component<Engine::Core::UnitComponent>()->owner_id ==
                    healer_unit->owner_id &&
                matches_heal_affinity(target, healer_component->target_affinity) &&
                HealingRules::can_receive_healing(*target);
      });
  return found;
}

FormationRanks::FormationRanks(Engine::Core::World& world)
    : m_world(world) {
  for (auto [member_id, member_mode, member_unit] :
       world
           .view<Engine::Core::FormationModeComponent, Engine::Core::UnitComponent>()) {
    (void)member_unit;
    if (member_mode.formation_id == 0 || member_mode.stable_rank < 0) {
      continue;
    }
    m_members[member_mode.formation_id].push_back(
        Member{.rank = member_mode.stable_rank, .id = member_id});
  }
  for (auto& [formation_id, members] : m_members) {
    (void)formation_id;
    std::sort(members.begin(), members.end(), [](const Member& a, const Member& b) {
      return a.rank != b.rank ? a.rank < b.rank : a.id < b.id;
    });
  }
}

auto FormationRanks::is_reserve(Engine::Core::Entity* entity) const -> bool {
  auto const* mode = entity->get_component<Engine::Core::FormationModeComponent>();
  if (mode == nullptr || !mode->active || mode->formation_id == 0 ||
      mode->stable_rank < 0) {
    return false;
  }
  auto const found = m_members.find(mode->formation_id);
  if (found == m_members.end()) {
    return false;
  }
  for (const Member& member : found->second) {
    if (member.rank >= mode->stable_rank) {
      return false;
    }
    auto const* member_mode =
        m_world.try_get<Engine::Core::FormationModeComponent>(member.id);
    auto const* member_unit = m_world.try_get<Engine::Core::UnitComponent>(member.id);
    if (member_mode != nullptr && member_unit != nullptr && member_unit->health > 0 &&
        member_mode->active && member_mode->formation_id == mode->formation_id &&
        member_mode->stable_rank >= 0 && member_mode->stable_rank < mode->stable_rank) {
      return true;
    }
  }
  return false;
}
} // namespace Game::Systems::Combat
