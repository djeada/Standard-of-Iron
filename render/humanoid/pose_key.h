#pragma once

// Stage 4 seam — narrow pose-shape cache key.
//
// The legacy template_cache bakes the unit's world transform into its
// 6-tuple key, so a unit that only translated could not reuse a cached
// pose. PoseKey deliberately excludes world transform and owner id: it
// captures *shape only* (animation state, variant, frame, stance), so any
// number of units sharing the same animation/variant/frame hit the same
// cache slot. Apply world transform at submit time.

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
    return variant_id == o.variant_id && anim == o.anim &&
           stance == o.stance && frame == o.frame;
  }
};

} // namespace Render::GL

namespace std {

template <> struct hash<::Render::GL::PoseKey> {
  auto operator()(const ::Render::GL::PoseKey &k) const noexcept
      -> std::size_t {
    std::uint64_t packed = k.variant_id;
    packed = (packed << 8) | static_cast<std::uint64_t>(k.anim);
    packed = (packed << 8) | static_cast<std::uint64_t>(k.stance);
    packed = (packed << 16) | static_cast<std::uint64_t>(k.frame);
    // Splitmix-style finaliser — cheap and well-dispersed for small ints.
    packed ^= packed >> 33;
    packed *= 0xff51afd7ed558ccdULL;
    packed ^= packed >> 33;
    packed *= 0xc4ceb9fe1a85ec53ULL;
    packed ^= packed >> 33;
    return static_cast<std::size_t>(packed);
  }
};

} // namespace std
