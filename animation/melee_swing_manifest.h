#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "pose_manifest.h"

namespace Animation {

struct MeleeIntent {

  float windup_dir_x{0.80F};
  float windup_dir_y{0.60F};

  float strike_dir_x{-0.80F};
  float strike_dir_y{-0.60F};

  float thrust_amount{0.0F};

  float elevation{0.0F};

  float charge{0.0F};

  float swing_speed{1.0F};

  float follow_through{0.5F};

  float weapon_target_x{0.0F};
  float weapon_target_y{0.0F};
  float weapon_target_z{1.10F};

  float blade_dir_x{0.12F};
  float blade_dir_y{0.86F};
  float blade_dir_z{0.50F};

  [[nodiscard]] auto strike_angle() const noexcept -> float {
    return std::atan2(strike_dir_y, strike_dir_x);
  }
};

inline constexpr float k_melee_intent_min_axis = 1.0e-4F;

inline constexpr float k_melee_left_cut_angle = -2.378F;
inline constexpr float k_melee_right_cut_angle = -0.764F;
inline constexpr float k_melee_overhead_angle = -1.731F;
inline constexpr float k_melee_thrust_angle = -0.200F;

inline constexpr float k_melee_default_reach = 1.5F;

void normalize_melee_intent(MeleeIntent& intent) noexcept;

[[nodiscard]] auto normalized_melee_intent(MeleeIntent intent) noexcept -> MeleeIntent;

void complete_melee_intent(MeleeIntent& intent,
                           float reach = k_melee_default_reach) noexcept;

[[nodiscard]] auto melee_intent_from_strike_angle(
    float strike_angle,
    float thrust_amount = 0.0F,
    float reach = k_melee_default_reach) noexcept -> MeleeIntent;

[[nodiscard]] auto melee_intent_for_attack_variant(
    std::uint8_t variant, float reach = k_melee_default_reach) noexcept -> MeleeIntent;

[[nodiscard]] auto
nearest_attack_variant(const MeleeIntent& intent) noexcept -> std::uint8_t;

[[nodiscard]] auto
melee_intent_about_anchor(const MeleeIntent& live,
                          const MeleeIntent& anchor,
                          float reach = k_melee_default_reach) noexcept -> MeleeIntent;

[[nodiscard]] auto
melee_intent_rotated(const MeleeIntent& intent,
                     float radians,
                     float reach = k_melee_default_reach) noexcept -> MeleeIntent;

[[nodiscard]] auto melee_intent_strike_delta(const MeleeIntent& from,
                                             const MeleeIntent& to) noexcept -> float;

struct MeleeRestDirection {
  float x{0.80F};
  float y{0.60F};
};

[[nodiscard]] auto melee_intent_resting_direction(const MeleeIntent& intent) noexcept
    -> MeleeRestDirection;

[[nodiscard]] auto
blend_melee_intent(const MeleeIntent& from,
                   const MeleeIntent& to,
                   float t,
                   float reach = k_melee_default_reach) noexcept -> MeleeIntent;

inline constexpr float k_melee_chamber_time = 0.18F;
inline constexpr float k_melee_apex_time = 0.32F;
inline constexpr float k_melee_contact_time = 0.58F;
inline constexpr float k_melee_follow_time = 0.76F;

struct MeleeSwingInputs {
  MeleeIntent intent{};

  float phase{0.0F};

  float shoulder_y{1.40F};

  float arm_reach{0.62F};

  MeleeRestDirection rest{};
  bool has_rest{false};
};

struct MeleeSwingSample {
  PoseVec3 grip{};
  PoseVec3 blade_direction{0.12F, 0.86F, 0.50F};

  float speed{0.0F};

  float commitment{0.0F};
};

[[nodiscard]] auto
resolve_melee_swing(const MeleeSwingInputs& inputs) noexcept -> MeleeSwingSample;

struct MeleeBodySolveInputs {
  MeleeSwingInputs swing{};

  float offhand_along_weapon{0.0F};
};

struct MeleeBodySolveSample {

  PoseVec3 grip{};
  PoseVec3 offhand{};
  PoseVec3 blade_direction{0.12F, 0.86F, 0.50F};

  float spine_twist{0.0F};
  float pelvis_twist{0.0F};

  float forward_lean{0.0F};
  float lateral_lean{0.0F};

  float shoulder_drive{0.0F};

  float weight_shift{0.0F};
  float front_foot_advance{0.0F};
  float back_foot_brace{0.0F};
};

[[nodiscard]] auto resolve_melee_body_solve(const MeleeBodySolveInputs& inputs) noexcept
    -> MeleeBodySolveSample;

} // namespace Animation
