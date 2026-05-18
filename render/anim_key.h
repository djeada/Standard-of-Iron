#pragma once

#include <cstdint>

#include "creature/pose_intent_enum.h"
#include "gl/humanoid/humanoid_types.h"

namespace Render::GL {

inline constexpr std::uint8_t k_anim_frame_count = 16;
inline constexpr std::uint8_t k_template_variant_count = 8;

struct AnimKey {
  Render::Creature::PoseIntent state{Render::Creature::PoseIntent::Idle};
  CombatAnimPhase combat_phase{CombatAnimPhase::Idle};
  std::uint8_t frame{0};
  Engine::Core::CombatAttackFamily attack_family{
      Engine::Core::CombatAttackFamily::None};
  std::uint8_t attack_variant{0};
  bool finisher_attack{false};

  bool operator==(const AnimKey& other) const {
    return state == other.state && combat_phase == other.combat_phase &&
           frame == other.frame && attack_family == other.attack_family &&
           attack_variant == other.attack_variant &&
           finisher_attack == other.finisher_attack;
  }
};

struct AnimKeyHash {
  std::size_t operator()(const AnimKey& key) const noexcept {
    std::size_t h = static_cast<std::size_t>(key.state);
    h ^= static_cast<std::size_t>(key.combat_phase) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= static_cast<std::size_t>(key.frame) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= static_cast<std::size_t>(key.attack_family) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= static_cast<std::size_t>(key.attack_variant) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= static_cast<std::size_t>(key.finisher_attack) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};

} // namespace Render::GL
