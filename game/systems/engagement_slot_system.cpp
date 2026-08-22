#include "engagement_slot_system.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <unordered_map>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/system_context.h"
#include "../core/world.h"
#include "command_service.h"

namespace Game::Systems {

namespace {

struct SlotOccupancy {
  std::vector<Engine::Core::EntityID> occupants;
  std::array<bool, EngagementSlotSystem::k_max_slots_per_target> occupied{};

  [[nodiscard]] auto first_open_slot() const -> std::uint8_t {
    for (std::uint8_t idx = 0; idx < occupied.size(); ++idx) {
      if (!occupied[idx]) {
        return idx;
      }
    }
    return EngagementSlotSystem::k_max_slots_per_target;
  }

  void reserve(std::uint8_t slot_index, Engine::Core::EntityID occupant) {
    if (slot_index < occupied.size()) {
      occupied[slot_index] = true;
    }
    occupants.push_back(occupant);
  }
};

} // namespace

void EngagementSlotSystem::run(Engine::Core::SystemContext& context) {
  const float delta_time = context.delta_time();
  if (delta_time <= 0.0F) {
    return;
  }

  m_diagnostics = {};

  const auto attackers = context.entities_with<Engine::Core::AttackComponent>();
  m_query_scratch.assign(attackers.begin(), attackers.end());
  std::sort(m_query_scratch.begin(), m_query_scratch.end());

  std::unordered_map<Engine::Core::EntityID, SlotOccupancy> target_slots;

  for (const Engine::Core::EntityID attacker_id : m_query_scratch) {
    const auto* atk = context.try_get<Engine::Core::AttackComponent>(attacker_id);

    bool const engaging_melee =
        atk != nullptr &&
        (atk->in_melee_lock ||
         atk->current_mode == Engine::Core::AttackComponent::CombatMode::Melee);
    if (!engaging_melee) {
      continue;
    }

    const auto* attack_target =
        context.try_get<Engine::Core::AttackTargetComponent>(attacker_id);
    if (attack_target == nullptr || attack_target->target_id == 0) {
      continue;
    }

    const auto* unit = context.try_get<Engine::Core::UnitComponent>(attacker_id);
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }

    auto* slot = context.try_get<Engine::Core::EngagementSlotComponent>(attacker_id);
    if (slot != nullptr && slot->valid && slot->target_id == attack_target->target_id) {

      slot->lease_remaining -= delta_time;
      if (slot->lease_remaining <= 0.0F) {
        slot->valid = false;
        ++m_diagnostics.slots_invalidated;
      } else if (!target_slots[slot->target_id].occupied[slot->slot_index]) {
        target_slots[slot->target_id].reserve(slot->slot_index, attacker_id);
        continue;
      } else {
        slot->valid = false;
      }
    }

    Engine::Core::EntityID const target_id = attack_target->target_id;
    if (!context.is_alive(target_id)) {
      if (slot != nullptr) {
        slot->valid = false;
      }
      continue;
    }

    const auto* target_transform =
        context.try_get<Engine::Core::TransformComponent>(target_id);
    const auto* attacker_transform =
        context.try_get<Engine::Core::TransformComponent>(attacker_id);
    if (target_transform == nullptr || attacker_transform == nullptr) {
      continue;
    }

    auto& occupancy = target_slots[target_id];
    std::uint8_t const slot_idx = occupancy.first_open_slot();
    if (slot_idx >= k_max_slots_per_target) {

      ++m_diagnostics.overflow_redirects;
      continue;
    }

    float const angle =
        (2.0F * static_cast<float>(std::numbers::pi) * static_cast<float>(slot_idx)) /
        static_cast<float>(k_max_slots_per_target);

    float const target_radius =
        CommandService::get_unit_radius(context.world(), target_id);
    float const offset_dist = target_radius + k_slot_radius_offset;

    float const anchor_x = std::cos(angle) * offset_dist;
    float const anchor_z = std::sin(angle) * offset_dist;

    if (slot == nullptr) {
      slot = context.emplace<Engine::Core::EngagementSlotComponent>(attacker_id);
    }
    if (slot != nullptr) {
      slot->target_id = target_id;
      slot->slot_index = slot_idx;
      slot->max_slots = k_max_slots_per_target;
      slot->anchor_offset_x = anchor_x;
      slot->anchor_offset_z = anchor_z;
      slot->valid = true;
      slot->lease_remaining = k_slot_lease_duration;

      occupancy.reserve(slot_idx, attacker_id);
      ++m_diagnostics.slots_allocated;
    }
  }
}

auto EngagementSlotSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<AttackComponent,
                                     AttackTargetComponent,
                                     UnitComponent,
                                     TransformComponent>{},
                               Writes<EngagementSlotComponent>{});
}

} // namespace Game::Systems
