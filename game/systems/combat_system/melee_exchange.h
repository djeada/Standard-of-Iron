#pragma once

#include <cstdint>

#include "../../core/component.h"
#include "../../core/entity.h"

namespace Game::Systems::Combat {

enum class MeleeExchangeOutcome : std::uint8_t {
  Clean = 0,
  Blocked = 1,
  Evaded = 2,
  Heavy = 3,

  Plain = 4,
};

struct MeleeExchangeBeat {
  MeleeExchangeOutcome outcome{MeleeExchangeOutcome::Plain};

  float damage_multiplier{1.0F};

  float interval_weight{1.0F};

  float delay_weight{1.0F};

  Engine::Core::HitReactionKind target_reaction{Engine::Core::HitReactionKind::Flinch};

  bool attacker_recoils{false};
};

inline constexpr std::uint32_t k_melee_exchange_beats = 8U;

[[nodiscard]] auto
melee_exchange_pair_seed(Engine::Core::EntityID attacker_id,
                         Engine::Core::EntityID target_id) noexcept -> std::uint32_t;

[[nodiscard]] auto
resolve_melee_exchange_beat(Engine::Core::EntityID attacker_id,
                            Engine::Core::EntityID target_id,
                            std::uint8_t swing_sequence,
                            bool target_can_defend) noexcept -> MeleeExchangeBeat;

[[nodiscard]] auto melee_exchange_beat_for_outcome(
    MeleeExchangeOutcome outcome) noexcept -> MeleeExchangeBeat;

[[nodiscard]] auto
melee_target_can_defend(const Engine::Core::Entity* attacker,
                        const Engine::Core::Entity* target) noexcept -> bool;

[[nodiscard]] auto melee_exchange_damage(int base_damage,
                                         const MeleeExchangeBeat& beat) noexcept -> int;

} // namespace Game::Systems::Combat
