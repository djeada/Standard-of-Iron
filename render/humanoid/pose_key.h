#pragma once

#include "../creature/pose_intent.h"

#include <cstdint>
#include <functional>

namespace Render::GL {

enum class AnimationState : std::uint8_t {
  Idle = 0,
  Walk = 1,
  Run = 2,
  Attack = 3,
  Death = 4,
  Stance = 5,
};

enum class PoseStance : std::uint8_t {
  Neutral = 0,
  Guard = 1,
  Ranged = 2,
  Charge = 3,
};

struct PoseKey {
  std::uint32_t variant_id = 0;
  AnimationState anim = AnimationState::Idle;
  PoseStance stance = PoseStance::Neutral;
  std::uint16_t frame = 0;

  [[nodiscard]] auto operator==(const PoseKey &o) const noexcept -> bool {
    return variant_id == o.variant_id && anim == o.anim && stance == o.stance &&
           frame == o.frame;
  }
};

} // namespace Render::GL

namespace std {

template <> struct hash<::Render::GL::PoseKey> {
  auto
  operator()(const ::Render::GL::PoseKey &k) const noexcept -> std::size_t {
    std::uint64_t packed = k.variant_id;
    packed = (packed << 8) | static_cast<std::uint64_t>(k.anim);
    packed = (packed << 8) | static_cast<std::uint64_t>(k.stance);
    packed = (packed << 16) | static_cast<std::uint64_t>(k.frame);

    packed ^= packed >> 33;
    packed *= 0xff51afd7ed558ccdULL;
    packed ^= packed >> 33;
    packed *= 0xc4ceb9fe1a85ec53ULL;
    packed ^= packed >> 33;
    return static_cast<std::size_t>(packed);
  }
};

} // namespace std

namespace Render::GL {

[[nodiscard]] inline auto
to_pose_key_anim_state(Render::Creature::PoseIntent intent) noexcept
    -> AnimationState {
  switch (intent) {
  case Render::Creature::PoseIntent::Walk:          return AnimationState::Walk;
  case Render::Creature::PoseIntent::Run:           return AnimationState::Run;
  case Render::Creature::PoseIntent::AttackMelee:
  case Render::Creature::PoseIntent::AttackSpear:
  case Render::Creature::PoseIntent::AttackRanged:
  case Render::Creature::PoseIntent::HitReaction:  return AnimationState::Attack;
  case Render::Creature::PoseIntent::Dying:
  case Render::Creature::PoseIntent::Dead:          return AnimationState::Death;
  case Render::Creature::PoseIntent::Hold:          return AnimationState::Stance;
  default:                                          return AnimationState::Idle;
  }
}

} // namespace Render::GL
