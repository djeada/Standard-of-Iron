#pragma once

#include <cstdint>

namespace Render::Creature {

// Canonical animation intent enum — single source of truth for all pose
// selection logic.  Every downstream system converts from this value.
//
// Priority order (highest → lowest):
//   dying → dead → hit_reaction → attacking(+weapon) → healing →
//   construct → hold → run → walk → idle
enum class PoseIntent : std::uint8_t {
  // Locomotion
  Idle = 0,
  Walk,
  Run,
  Hold,
  // Combat
  AttackMelee,   // sword
  AttackSpear,
  AttackRanged,  // bow / sling
  HitReaction,
  // Other actions
  Healing,
  Construct,
  // Mount-specific
  RidingIdle,
  RidingCharge,
  RidingReining,
  RidingBowShot,
  // Death
  Dying,
  Dead,
  Count
};

[[nodiscard]] constexpr auto pose_intent_count() noexcept -> std::size_t {
  return static_cast<std::size_t>(PoseIntent::Count);
}

} // namespace Render::Creature
