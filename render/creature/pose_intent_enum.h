#pragma once

#include <cstdint>

namespace Render::Creature {

enum class PoseIntent : std::uint8_t {

  Idle = 0,
  Walk,
  Run,
  Hold,

  AttackMelee,
  AttackSpear,
  AttackRanged,
  Cast,
  HitReaction,

  Healing,
  Construct,

  RidingIdle,
  RidingCharge,
  RidingReining,
  RidingBowShot,

  Dying,
  Dead,
  Count
};

[[nodiscard]] constexpr auto pose_intent_count() noexcept -> std::size_t {
  return static_cast<std::size_t>(PoseIntent::Count);
}

} // namespace Render::Creature
