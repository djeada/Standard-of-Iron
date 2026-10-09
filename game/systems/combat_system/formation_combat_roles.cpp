#include "formation_combat_roles.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "../../core/world.h"
#include "../../units/combat_role.h"
#include "combat_random.h"
#include "formation_local_frame.h"

namespace Game::Systems::Combat {
namespace {

constexpr float k_target_switch_hysteresis = 0.60F;
constexpr float k_target_hold_slack = 1.5F;

} // namespace

auto opponent_alive(Engine::Core::World& world,
                    Engine::Core::EntityID opponent_id) -> bool {
  auto* opponent = world.get_entity(opponent_id);
  auto const* unit = opponent != nullptr
                         ? opponent->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
  return opponent != nullptr && unit != nullptr && unit->health > 0 &&
         !opponent->has_component<Engine::Core::PendingRemovalComponent>();
}

auto combat_role_for(std::uint32_t formation_seed,
                     std::uint16_t stable_slot,
                     bool engaged,
                     float combat_seconds) -> Engine::Core::FormationSoldierCombatRole {
  if (!engaged) {
    return Engine::Core::FormationSoldierCombatRole::Ready;
  }
  using Role = Engine::Core::FormationSoldierCombatRole;
  std::uint32_t const seed = mix_hash32(
      formation_seed ^ (static_cast<std::uint32_t>(stable_slot) * 0x9e3779b9U));

  constexpr std::array<Role, 8> k_exchange{Role::LeadStrike,
                                           Role::SupportStrike,
                                           Role::Guard,
                                           Role::StepIn,
                                           Role::LeadStrike,
                                           Role::StepOut,
                                           Role::SupportStrike,
                                           Role::Guard};
  float const beat_seconds = 1.8F + static_cast<float>(seed % 41U) * 0.01F;
  float const offset = static_cast<float>((seed >> 8U) % 100U) * 0.01F;
  auto const beat = static_cast<std::uint32_t>(
      std::floor(std::max(0.0F, combat_seconds) / beat_seconds + offset));
  return k_exchange[((seed >> 16U) + beat) % k_exchange.size()];
}

auto brawls_as_a_crowd(const Engine::Core::World& world,
                       Engine::Core::EntityID entity_id) -> bool {
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
  return unit != nullptr && Game::Units::combat_role(unit->spawn_type) ==
                                Game::Units::CombatRole::Noncombatant;
}

auto crowd_brawl_role(std::uint16_t stable_slot)
    -> Engine::Core::FormationSoldierCombatRole {
  return (stable_slot % 2U) == 0U
             ? Engine::Core::FormationSoldierCombatRole::LeadStrike
             : Engine::Core::FormationSoldierCombatRole::SupportStrike;
}

auto action_for_role(Engine::Core::FormationSoldierCombatRole role)
    -> Engine::Core::FormationSoldierAction {
  using Action = Engine::Core::FormationSoldierAction;
  using Role = Engine::Core::FormationSoldierCombatRole;
  switch (role) {
  case Role::LeadStrike:
  case Role::SupportStrike:
    return Action::MeleeEngaged;
  case Role::Guard:
    return Action::MeleeGuard;
  case Role::StepIn:
  case Role::StepOut:
    return Action::MeleeReposition;
  case Role::Ready:
    return Action::MeleeReady;
  case Role::None:
    return Action::FollowUnit;
  }
  return Action::FollowUnit;
}

auto assignment_for_slot(const Engine::Core::FormationContactComponent* contact,
                         const Engine::Core::FormationSoldierPresentation* previous,
                         std::uint16_t stable_slot) -> SoldierAssignment {
  if (contact == nullptr) {
    return {};
  }

  SoldierAssignment best;
  float best_distance = std::numeric_limits<float>::infinity();
  for (auto const& front : contact->fronts) {
    if (!front.in_contact) {
      continue;
    }
    auto const pair = std::lower_bound(front.engagement_pairs.begin(),
                                       front.engagement_pairs.end(),
                                       stable_slot,
                                       [](auto const& candidate, std::uint16_t slot) {
                                         return candidate.attacker_slot < slot;
                                       });
    if (pair == front.engagement_pairs.end() || pair->attacker_slot != stable_slot) {
      continue;
    }
    if (previous != nullptr && previous->opponent_id == front.opponent_id) {
      return {&front, &*pair};
    }
    if (pair->root_distance < best_distance) {
      best = {&front, &*pair};
      best_distance = pair->root_distance;
    }
  }
  return best;
}

auto retained_target_slot(const FormationCombat::FormationLayout& opponent_layout,
                          const FormationCombat::SoldierSlot* attacker_slot,
                          const Engine::Core::FormationSoldierPresentation* previous,
                          const SoldierAssignment& assignment,
                          float spacing) -> RetainedTarget {
  RetainedTarget result{assignment.pair->target_slot, assignment.pair->root_distance};
  if (previous == nullptr || attacker_slot == nullptr ||
      previous->opponent_id != assignment.front->opponent_id ||
      previous->target_slot == result.slot) {
    return result;
  }

  auto const* held = find_live_slot(opponent_layout, previous->target_slot);
  if (held == nullptr) {
    return result;
  }
  float const held_distance = std::hypot(held->world_x - attacker_slot->world_x,
                                         held->world_z - attacker_slot->world_z);
  bool const within_hysteresis =
      held_distance <= result.root_distance + spacing * k_target_switch_hysteresis;
  bool const still_holding =
      previous->target_held_seconds < k_target_hold_seconds &&
      held_distance <= result.root_distance + spacing * k_target_hold_slack;
  if (within_hysteresis || still_holding) {
    return {previous->target_slot, held_distance};
  }
  return result;
}

} // namespace Game::Systems::Combat
