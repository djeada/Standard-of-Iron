#include "engagement_slot_system.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_map>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "command_service.h"

namespace Game::Systems {

namespace {

struct SlotOccupancy {
  std::vector<Engine::Core::EntityID> occupants;
};

} // namespace

void EngagementSlotSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr || delta_time <= 0.0F) {
    return;
  }

  m_diagnostics = {};

  // Collect all entities that have attack targets and are in melee mode.
  auto attackers = world->get_entities_with<Engine::Core::AttackComponent>();

  // Track slot occupancy per target.
  std::unordered_map<Engine::Core::EntityID, SlotOccupancy> target_slots;

  for (auto* attacker : attackers) {
    auto* atk = attacker->get_component<Engine::Core::AttackComponent>();
    if (atk == nullptr || !atk->in_melee_lock) {
      continue;
    }

    auto* attack_target =
        attacker->get_component<Engine::Core::AttackTargetComponent>();
    if (attack_target == nullptr || attack_target->target_id == 0) {
      continue;
    }

    auto* unit = attacker->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }

    // Check if already has a valid slot.
    auto* slot = attacker->get_component<Engine::Core::EngagementSlotComponent>();
    if (slot != nullptr && slot->valid && slot->target_id == attack_target->target_id) {
      // Tick lease.
      slot->lease_remaining -= delta_time;
      if (slot->lease_remaining <= 0.0F) {
        slot->valid = false;
        ++m_diagnostics.slots_invalidated;
      } else {
        target_slots[slot->target_id].occupants.push_back(attacker->get_id());
        continue;
      }
    }

    // Need to allocate or reallocate a slot.
    Engine::Core::EntityID target_id = attack_target->target_id;
    auto* target = world->get_entity(target_id);
    if (target == nullptr) {
      if (slot != nullptr) {
        slot->valid = false;
      }
      continue;
    }

    auto* target_transform =
        target->get_component<Engine::Core::TransformComponent>();
    auto* attacker_transform =
        attacker->get_component<Engine::Core::TransformComponent>();
    if (target_transform == nullptr || attacker_transform == nullptr) {
      continue;
    }

    auto& occupancy = target_slots[target_id];
    std::uint8_t num_occupied =
        static_cast<std::uint8_t>(occupancy.occupants.size());

    if (num_occupied >= k_max_slots_per_target) {
      // Overflow: unit should consider a different target.
      ++m_diagnostics.overflow_redirects;
      continue;
    }

    // Assign next available slot index.
    std::uint8_t slot_idx = num_occupied;

    // Compute anchor offset around target.
    float const angle = (2.0F * static_cast<float>(std::numbers::pi) *
                         static_cast<float>(slot_idx)) /
                        static_cast<float>(k_max_slots_per_target);

    float const target_radius =
        CommandService::get_unit_radius(*world, target_id);
    float const offset_dist = target_radius + k_slot_radius_offset;

    float const anchor_x = std::cos(angle) * offset_dist;
    float const anchor_z = std::sin(angle) * offset_dist;

    // Assign or create slot component.
    if (slot == nullptr) {
      slot = attacker->add_component<Engine::Core::EngagementSlotComponent>();
    }
    if (slot != nullptr) {
      slot->target_id = target_id;
      slot->slot_index = slot_idx;
      slot->max_slots = k_max_slots_per_target;
      slot->anchor_offset_x = anchor_x;
      slot->anchor_offset_z = anchor_z;
      slot->valid = true;
      slot->lease_remaining = k_slot_lease_duration;

      occupancy.occupants.push_back(attacker->get_id());
      ++m_diagnostics.slots_allocated;
    }
  }
}

} // namespace Game::Systems
