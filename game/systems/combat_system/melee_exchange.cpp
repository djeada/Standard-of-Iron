#include "melee_exchange.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "../combat_rules.h"
#include "combat_random.h"

namespace Game::Systems::Combat {

namespace {

struct BeatAuthoring {
  MeleeExchangeOutcome outcome;
  float interval_weight;
  float delay_weight;
};

constexpr std::array<BeatAuthoring, k_melee_exchange_beats> k_exchange_pattern{{
    {MeleeExchangeOutcome::Clean, 0.80F, 0.6F},
    {MeleeExchangeOutcome::Blocked, 0.75F, 0.4F},
    {MeleeExchangeOutcome::Clean, 1.00F, 1.0F},
    {MeleeExchangeOutcome::Clean, 0.70F, 0.3F},
    {MeleeExchangeOutcome::Evaded, 0.70F, 0.5F},
    {MeleeExchangeOutcome::Heavy, 1.75F, 2.4F},
    {MeleeExchangeOutcome::Blocked, 0.80F, 0.8F},
    {MeleeExchangeOutcome::Clean, 1.50F, 2.0F},
}};

constexpr float k_clean_damage_multiplier = 1.375F;
constexpr float k_heavy_damage_multiplier = 1.70F;
constexpr float k_blocked_damage_multiplier = 0.40F;

static_assert(k_exchange_pattern.size() == 8U);

constexpr auto pattern_interval_sum() -> float {
  float sum = 0.0F;
  for (auto const& beat : k_exchange_pattern) {
    sum += beat.interval_weight;
  }
  return sum;
}

constexpr auto pattern_delay_sum() -> float {
  float sum = 0.0F;
  for (auto const& beat : k_exchange_pattern) {
    sum += beat.delay_weight;
  }
  return sum;
}

constexpr auto pattern_damage_sum() -> float {
  float sum = 0.0F;
  for (auto const& beat : k_exchange_pattern) {
    switch (beat.outcome) {
    case MeleeExchangeOutcome::Clean:
      sum += k_clean_damage_multiplier;
      break;
    case MeleeExchangeOutcome::Heavy:
      sum += k_heavy_damage_multiplier;
      break;
    case MeleeExchangeOutcome::Blocked:
      sum += k_blocked_damage_multiplier;
      break;
    case MeleeExchangeOutcome::Evaded:
    case MeleeExchangeOutcome::Plain:
      break;
    }
  }
  return sum;
}

static_assert(pattern_interval_sum() > 7.999F && pattern_interval_sum() < 8.001F,
              "exchange cadence must average to the unit's cooldown");
static_assert(pattern_delay_sum() > 7.999F && pattern_delay_sum() < 8.001F,
              "exchange delays must average to the old attack delay");
static_assert(pattern_damage_sum() > 7.999F && pattern_damage_sum() < 8.001F,
              "exchange outcomes must be damage-neutral over one cycle");

} // namespace

auto melee_exchange_pair_seed(Engine::Core::EntityID attacker_id,
                              Engine::Core::EntityID target_id) noexcept
    -> std::uint32_t {
  return (static_cast<std::uint32_t>(attacker_id) * 2246822519U) ^
         (static_cast<std::uint32_t>(target_id) * 3266489917U) ^ 0x27D4EB2FU;
}

auto melee_exchange_beat_for_outcome(MeleeExchangeOutcome outcome) noexcept
    -> MeleeExchangeBeat {
  MeleeExchangeBeat beat;
  beat.outcome = outcome;
  switch (outcome) {
  case MeleeExchangeOutcome::Clean:
    beat.damage_multiplier = k_clean_damage_multiplier;
    beat.target_reaction = Engine::Core::HitReactionKind::Flinch;
    break;
  case MeleeExchangeOutcome::Heavy:
    beat.damage_multiplier = k_heavy_damage_multiplier;
    beat.target_reaction = Engine::Core::HitReactionKind::Stagger;
    break;
  case MeleeExchangeOutcome::Blocked:
    beat.damage_multiplier = k_blocked_damage_multiplier;
    beat.target_reaction = Engine::Core::HitReactionKind::Block;
    beat.attacker_recoils = true;
    break;
  case MeleeExchangeOutcome::Evaded:
    beat.damage_multiplier = 0.0F;
    beat.target_reaction = Engine::Core::HitReactionKind::Evade;
    break;
  case MeleeExchangeOutcome::Plain:
    beat.damage_multiplier = 1.0F;
    beat.target_reaction = Engine::Core::HitReactionKind::Flinch;
    break;
  }
  return beat;
}

auto resolve_melee_exchange_beat(Engine::Core::EntityID attacker_id,
                                 Engine::Core::EntityID target_id,
                                 std::uint8_t swing_sequence,
                                 bool target_can_defend) noexcept -> MeleeExchangeBeat {
  if (!target_can_defend) {
    return MeleeExchangeBeat{};
  }

  constexpr std::array<std::uint32_t, 5> k_opening_beats{{0U, 2U, 3U, 5U, 7U}};
  std::uint32_t const seed = melee_exchange_pair_seed(attacker_id, target_id);
  std::uint32_t const rotation =
      k_opening_beats[static_cast<std::uint32_t>(hash_to_unit(seed) * 4.999F)];
  std::uint32_t const index =
      (swing_sequence + k_melee_exchange_beats - 1U + rotation) %
      k_melee_exchange_beats;
  auto const& authored = k_exchange_pattern[index];
  MeleeExchangeBeat beat = melee_exchange_beat_for_outcome(authored.outcome);
  beat.interval_weight = authored.interval_weight;
  beat.delay_weight = authored.delay_weight;
  return beat;
}

auto melee_target_can_defend(const Engine::Core::Entity* attacker,
                             const Engine::Core::Entity* target) noexcept -> bool {
  if (attacker == nullptr || target == nullptr) {
    return false;
  }
  if (target->has_component<Engine::Core::BuildingComponent>()) {
    return false;
  }
  if (Game::Systems::CombatRules::uses_rpg_combat_rules(target)) {
    return false;
  }
  if (target->has_component<Engine::Core::ElephantComponent>() ||
      attacker->has_component<Engine::Core::ElephantComponent>()) {
    return false;
  }
  auto const* target_unit = target->get_component<Engine::Core::UnitComponent>();
  if (target_unit == nullptr || target_unit->health <= 0) {
    return false;
  }
  auto const* target_attack = target->get_component<Engine::Core::AttackComponent>();
  if (target_attack == nullptr) {
    return false;
  }
  if (target->has_component<Engine::Core::StaggerComponent>()) {
    return false;
  }
  return true;
}

auto melee_exchange_damage(int base_damage,
                           const MeleeExchangeBeat& beat) noexcept -> int {
  if (base_damage <= 0 || beat.damage_multiplier <= 0.0F) {
    return 0;
  }
  return std::max(1,
                  static_cast<int>(std::lround(static_cast<float>(base_damage) *
                                               beat.damage_multiplier)));
}

} // namespace Game::Systems::Combat
