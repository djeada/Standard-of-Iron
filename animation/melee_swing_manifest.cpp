#include "melee_swing_manifest.h"

#include <array>
#include <limits>

namespace Animation {

namespace {

[[nodiscard]] auto add(const PoseVec3& a, const PoseVec3& b) noexcept -> PoseVec3 {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] auto scale(const PoseVec3& v, float s) noexcept -> PoseVec3 {
  return {v.x * s, v.y * s, v.z * s};
}

[[nodiscard]] auto length(const PoseVec3& v) noexcept -> float {
  return std::sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
}

[[nodiscard]] auto normalize(const PoseVec3& v,
                             const PoseVec3& fallback) noexcept -> PoseVec3 {
  float const len = length(v);
  if (len < k_melee_intent_min_axis) {
    return fallback;
  }
  return scale(v, 1.0F / len);
}

[[nodiscard]] auto
nlerp(const PoseVec3& from, const PoseVec3& to, float t) noexcept -> PoseVec3 {
  PoseVec3 const mixed{std::lerp(from.x, to.x, t),
                       std::lerp(from.y, to.y, t),
                       std::lerp(from.z, to.z, t)};
  return normalize(mixed, to);
}

[[nodiscard]] auto smoothstep(float t) noexcept -> float {
  float const clamped = std::clamp(t, 0.0F, 1.0F);
  return clamped * clamped * (3.0F - (2.0F * clamped));
}

constexpr float k_nominal_swing_seconds = 1.40F;

constexpr float k_weapon_shoulder_x = 0.19F;

struct SwingKey {
  float time{0.0F};
  PoseVec3 grip{};
  PoseVec3 blade{};
};

using SwingKeys = std::array<SwingKey, 6>;

[[nodiscard]] auto build_swing_keys(const MeleeSwingInputs& inputs,
                                    const MeleeIntent& intent) noexcept -> SwingKeys {
  PoseVec3 const shoulder{k_weapon_shoulder_x, inputs.shoulder_y, 0.0F};
  float const reach = std::max(inputs.arm_reach, 0.15F);
  float const thrust = intent.thrust_amount;

  MeleeRestDirection const rest =
      inputs.has_rest ? inputs.rest
                      : MeleeRestDirection{intent.windup_dir_x, intent.windup_dir_y};

  auto from_shoulder = [&](PoseVec3 direction, float extension) {
    return add(shoulder,
               scale(normalize(direction, {0.0F, 0.0F, 1.0F}), reach * extension));
  };

  PoseVec3 const guard_grip =
      from_shoulder({rest.x * 0.60F, rest.y * 0.45F, 0.75F}, 0.55F);
  PoseVec3 const guard_blade =
      normalize({rest.x * 0.25F, 0.86F, 0.45F}, {0.12F, 0.86F, 0.50F});

  PoseVec3 const chamber_grip = from_shoulder(
      {intent.windup_dir_x, intent.windup_dir_y, -0.35F + (0.20F * thrust)},
      0.80F - (0.25F * thrust));
  PoseVec3 const chamber_blade = normalize(
      {intent.windup_dir_x * 0.50F, intent.windup_dir_y * 0.80F, -0.40F}, guard_blade);

  PoseVec3 const apex_grip = from_shoulder({intent.windup_dir_x * 0.85F,
                                            intent.windup_dir_y * 0.95F,
                                            -0.10F + (0.35F * thrust)},
                                           0.92F - (0.18F * thrust));
  PoseVec3 const apex_blade =
      normalize({intent.windup_dir_x * 0.40F, intent.windup_dir_y * 0.90F, -0.20F},
                chamber_blade);

  PoseVec3 const contact_grip{intent.weapon_target_x,
                              inputs.shoulder_y + intent.weapon_target_y,
                              intent.weapon_target_z};
  PoseVec3 const contact_blade{
      intent.blade_dir_x, intent.blade_dir_y, intent.blade_dir_z};

  PoseVec3 const follow_grip =
      add(contact_grip,
          scale(normalize({intent.strike_dir_x * (1.0F - thrust),
                           intent.strike_dir_y * (1.0F - thrust),
                           0.30F + (0.70F * thrust)},
                          {0.0F, 0.0F, 1.0F}),
                reach * (0.30F + (0.42F * intent.follow_through))));
  PoseVec3 const follow_blade = normalize(
      {intent.strike_dir_x * 0.80F, intent.strike_dir_y * 0.70F, 0.20F}, contact_blade);

  return SwingKeys{SwingKey{0.0F, guard_grip, guard_blade},
                   SwingKey{k_melee_chamber_time, chamber_grip, chamber_blade},
                   SwingKey{k_melee_apex_time, apex_grip, apex_blade},
                   SwingKey{k_melee_contact_time, contact_grip, contact_blade},
                   SwingKey{k_melee_follow_time, follow_grip, follow_blade},
                   SwingKey{1.0F, guard_grip, guard_blade}};
}

[[nodiscard]] auto catmull_rom(const PoseVec3& p0,
                               const PoseVec3& p1,
                               const PoseVec3& p2,
                               const PoseVec3& p3,
                               float u) noexcept -> PoseVec3 {
  float const u2 = u * u;
  float const u3 = u2 * u;
  auto axis = [&](float a0, float a1, float a2, float a3) {
    return 0.5F * (((2.0F * a1)) + ((-a0 + a2) * u) +
                   (((2.0F * a0) - (5.0F * a1) + (4.0F * a2) - a3) * u2) +
                   ((-a0 + (3.0F * a1) - (3.0F * a2) + a3) * u3));
  };
  return {axis(p0.x, p1.x, p2.x, p3.x),
          axis(p0.y, p1.y, p2.y, p3.y),
          axis(p0.z, p1.z, p2.z, p3.z)};
}

[[nodiscard]] auto catmull_rom_tangent(const PoseVec3& p0,
                                       const PoseVec3& p1,
                                       const PoseVec3& p2,
                                       const PoseVec3& p3,
                                       float u) noexcept -> PoseVec3 {
  float const u2 = u * u;
  auto axis = [&](float a0, float a1, float a2, float a3) {
    return 0.5F *
           ((-a0 + a2) + ((2.0F * ((2.0F * a0) - (5.0F * a1) + (4.0F * a2) - a3)) * u) +
            ((3.0F * (-a0 + (3.0F * a1) - (3.0F * a2) + a3)) * u2));
  };
  return {axis(p0.x, p1.x, p2.x, p3.x),
          axis(p0.y, p1.y, p2.y, p3.y),
          axis(p0.z, p1.z, p2.z, p3.z)};
}

} // namespace

void normalize_melee_intent(MeleeIntent& intent) noexcept {
  auto normalize_pair = [](float& x, float& y, float fallback_x, float fallback_y) {
    float const len = std::sqrt((x * x) + (y * y));
    if (len < k_melee_intent_min_axis) {
      x = fallback_x;
      y = fallback_y;
      return;
    }
    x /= len;
    y /= len;
  };
  normalize_pair(intent.windup_dir_x, intent.windup_dir_y, 0.80F, 0.60F);
  normalize_pair(intent.strike_dir_x,
                 intent.strike_dir_y,
                 -intent.windup_dir_x,
                 -intent.windup_dir_y);

  PoseVec3 const blade =
      normalize({intent.blade_dir_x, intent.blade_dir_y, intent.blade_dir_z},
                {0.12F, 0.86F, 0.50F});
  intent.blade_dir_x = blade.x;
  intent.blade_dir_y = blade.y;
  intent.blade_dir_z = blade.z;

  intent.thrust_amount = std::clamp(intent.thrust_amount, 0.0F, 1.0F);
  intent.charge = std::clamp(intent.charge, 0.0F, 1.0F);
  intent.swing_speed = std::clamp(intent.swing_speed, 0.35F, 2.20F);
  intent.follow_through = std::clamp(intent.follow_through, 0.0F, 1.0F);
  intent.elevation = std::clamp(intent.elevation, -1.20F, 1.20F);
}

auto normalized_melee_intent(MeleeIntent intent) noexcept -> MeleeIntent {
  normalize_melee_intent(intent);
  return intent;
}

void complete_melee_intent(MeleeIntent& intent, float reach) noexcept {
  normalize_melee_intent(intent);

  if (intent.thrust_amount > 0.0F) {
    constexpr float k_hip_guard_x = 0.55F;
    constexpr float k_hip_guard_y = -0.84F;
    intent.windup_dir_x =
        std::lerp(intent.windup_dir_x, k_hip_guard_x, intent.thrust_amount);
    intent.windup_dir_y =
        std::lerp(intent.windup_dir_y, k_hip_guard_y, intent.thrust_amount);
    normalize_melee_intent(intent);
  }

  float const usable_reach = std::max(reach, 0.20F);
  float const lateral = usable_reach * 0.55F * (1.0F - intent.thrust_amount);
  intent.weapon_target_x = intent.strike_dir_x * lateral;
  intent.weapon_target_y = intent.elevation;
  intent.weapon_target_z = usable_reach * (0.72F + (0.38F * intent.thrust_amount));

  float const cut_weight = 0.75F * (1.0F - intent.thrust_amount);
  intent.blade_dir_x = intent.strike_dir_x * cut_weight;
  intent.blade_dir_y = intent.strike_dir_y * cut_weight;
  intent.blade_dir_z = 0.55F + (0.45F * intent.thrust_amount);

  normalize_melee_intent(intent);
}

auto melee_intent_from_strike_angle(float strike_angle,
                                    float thrust_amount,
                                    float reach) noexcept -> MeleeIntent {
  MeleeIntent intent{};
  intent.strike_dir_x = std::cos(strike_angle);
  intent.strike_dir_y = std::sin(strike_angle);
  intent.windup_dir_x = -intent.strike_dir_x;
  intent.windup_dir_y = -intent.strike_dir_y;
  intent.thrust_amount = thrust_amount;
  complete_melee_intent(intent, reach);
  return intent;
}

auto melee_intent_for_attack_variant(std::uint8_t variant,
                                     float reach) noexcept -> MeleeIntent {
  switch (variant % 3U) {
  case 1U:
    return melee_intent_from_strike_angle(k_melee_right_cut_angle, 0.0F, reach);
  case 2U:
    return melee_intent_from_strike_angle(k_melee_overhead_angle, 0.0F, reach);
  default:
    break;
  }
  return melee_intent_from_strike_angle(k_melee_left_cut_angle, 0.0F, reach);
}

auto melee_intent_rotated(const MeleeIntent& intent,
                          float radians,
                          float reach) noexcept -> MeleeIntent {
  MeleeIntent rotated = intent;
  float const angle = intent.strike_angle() + radians;
  rotated.strike_dir_x = std::cos(angle);
  rotated.strike_dir_y = std::sin(angle);
  rotated.windup_dir_x = -rotated.strike_dir_x;
  rotated.windup_dir_y = -rotated.strike_dir_y;
  complete_melee_intent(rotated, reach);
  return rotated;
}

auto nearest_attack_variant(const MeleeIntent& intent) noexcept -> std::uint8_t {
  std::uint8_t best = 0U;
  float best_delta = std::numeric_limits<float>::infinity();
  for (std::uint8_t variant = 0U; variant < 3U; ++variant) {
    float const delta =
        melee_intent_strike_delta(melee_intent_for_attack_variant(variant), intent);
    if (delta < best_delta) {
      best_delta = delta;
      best = variant;
    }
  }
  return best;
}

auto melee_intent_about_anchor(const MeleeIntent& live,
                               const MeleeIntent& anchor,
                               float reach) noexcept -> MeleeIntent {

  if (live.thrust_amount >= 0.55F) {
    return live;
  }

  MeleeIntent const own = melee_intent_for_attack_variant(nearest_attack_variant(live));
  constexpr float k_two_pi = 6.283185307F;
  float offset = live.strike_angle() - own.strike_angle();
  while (offset > k_two_pi * 0.5F) {
    offset -= k_two_pi;
  }
  while (offset < -k_two_pi * 0.5F) {
    offset += k_two_pi;
  }

  MeleeIntent carried = live;
  float const angle = anchor.strike_angle() + offset;
  carried.strike_dir_x = std::cos(angle);
  carried.strike_dir_y = std::sin(angle);
  carried.windup_dir_x = -carried.strike_dir_x;
  carried.windup_dir_y = -carried.strike_dir_y;
  complete_melee_intent(carried, reach);
  return carried;
}

auto melee_intent_strike_delta(const MeleeIntent& from,
                               const MeleeIntent& to) noexcept -> float {
  float const dot = std::clamp((from.strike_dir_x * to.strike_dir_x) +
                                   (from.strike_dir_y * to.strike_dir_y),
                               -1.0F,
                               1.0F);
  return std::acos(dot);
}

auto melee_intent_resting_direction(const MeleeIntent& intent) noexcept
    -> MeleeRestDirection {
  float const carry = 0.55F + (0.35F * intent.follow_through);
  MeleeRestDirection rest{intent.strike_dir_x * carry, intent.strike_dir_y * carry};
  float const len = std::sqrt((rest.x * rest.x) + (rest.y * rest.y));
  if (len < k_melee_intent_min_axis) {
    return {};
  }
  rest.x /= len;
  rest.y /= len;
  return rest;
}

auto blend_melee_intent(const MeleeIntent& from,
                        const MeleeIntent& to,
                        float t,
                        float reach) noexcept -> MeleeIntent {
  float const weight = std::clamp(t, 0.0F, 1.0F);
  MeleeIntent result = from;

  constexpr float k_two_pi = 6.283185307F;
  float delta = to.strike_angle() - from.strike_angle();
  while (delta > k_two_pi * 0.5F) {
    delta -= k_two_pi;
  }
  while (delta < -k_two_pi * 0.5F) {
    delta += k_two_pi;
  }
  float const angle = from.strike_angle() + (delta * weight);
  result.strike_dir_x = std::cos(angle);
  result.strike_dir_y = std::sin(angle);
  result.windup_dir_x = -result.strike_dir_x;
  result.windup_dir_y = -result.strike_dir_y;

  result.thrust_amount = std::lerp(from.thrust_amount, to.thrust_amount, weight);
  result.elevation = std::lerp(from.elevation, to.elevation, weight);
  result.charge = std::lerp(from.charge, to.charge, weight);
  result.swing_speed = std::lerp(from.swing_speed, to.swing_speed, weight);
  result.follow_through = std::lerp(from.follow_through, to.follow_through, weight);
  complete_melee_intent(result, reach);
  return result;
}

auto resolve_melee_swing(const MeleeSwingInputs& inputs) noexcept -> MeleeSwingSample {
  MeleeIntent const intent = normalized_melee_intent(inputs.intent);
  SwingKeys const keys = build_swing_keys(inputs, intent);
  float const phase = std::clamp(inputs.phase, 0.0F, 1.0F);

  std::size_t segment = 0;
  while (segment + 2 < keys.size() && phase > keys[segment + 1].time) {
    ++segment;
  }
  float const span = std::max(keys[segment + 1].time - keys[segment].time, 1.0e-4F);
  float const u = std::clamp((phase - keys[segment].time) / span, 0.0F, 1.0F);

  auto key_at = [&keys](std::size_t index) -> const SwingKey& {
    return keys[std::clamp<std::size_t>(index, 0, keys.size() - 1)];
  };
  const SwingKey& k0 = key_at(segment == 0 ? 0 : segment - 1);
  const SwingKey& k1 = keys[segment];
  const SwingKey& k2 = keys[segment + 1];
  const SwingKey& k3 = key_at(segment + 2);

  MeleeSwingSample sample{};
  sample.grip = catmull_rom(k0.grip, k1.grip, k2.grip, k3.grip, u);
  sample.blade_direction = nlerp(k1.blade, k2.blade, smoothstep(u));

  PoseVec3 const tangent = catmull_rom_tangent(k0.grip, k1.grip, k2.grip, k3.grip, u);
  float const phase_rate = intent.swing_speed / (k_nominal_swing_seconds * span);
  sample.speed = length(scale(tangent, phase_rate));

  if (phase <= k_melee_contact_time) {
    sample.commitment = smoothstep(phase / k_melee_contact_time);
  } else {
    sample.commitment = 1.0F - smoothstep((phase - k_melee_contact_time) /
                                          (1.0F - k_melee_contact_time));
  }

  return sample;
}

auto resolve_melee_body_solve(const MeleeBodySolveInputs& inputs) noexcept
    -> MeleeBodySolveSample {
  MeleeSwingSample const swing = resolve_melee_swing(inputs.swing);
  MeleeIntent const intent = normalized_melee_intent(inputs.swing.intent);

  MeleeBodySolveSample solved{};
  solved.grip = swing.grip;
  solved.blade_direction = swing.blade_direction;

  PoseVec3 const shoulder{k_weapon_shoulder_x, inputs.swing.shoulder_y, 0.0F};
  float const reach = std::max(inputs.swing.arm_reach, 0.15F);

  constexpr float k_twist_per_metre = 1.05F;
  constexpr float k_max_twist = 0.62F;
  float const lateral_offset = swing.grip.x - shoulder.x;
  solved.spine_twist =
      std::clamp(-lateral_offset * k_twist_per_metre, -k_max_twist, k_max_twist);

  solved.pelvis_twist = solved.spine_twist * (-0.30F + (0.75F * swing.commitment));

  float const commitment_signed = (swing.commitment * 2.0F) - 1.0F;
  solved.weight_shift = commitment_signed * (0.60F + (0.40F * intent.thrust_amount)) *
                        (0.70F + (0.30F * intent.charge));

  constexpr float k_nominal_grip_speed = 3.0F;
  float const drive = std::clamp(swing.speed / k_nominal_grip_speed, 0.60F, 1.60F);

  constexpr float k_lean_metres = 0.085F;
  solved.forward_lean = k_lean_metres * commitment_signed * drive *
                        (0.75F + (0.55F * intent.thrust_amount));
  solved.lateral_lean = 0.055F * intent.strike_dir_x * swing.commitment * drive;

  float const grip_distance = length({swing.grip.x - shoulder.x,
                                      swing.grip.y - shoulder.y,
                                      swing.grip.z - shoulder.z});
  constexpr float k_comfortable_fraction = 0.82F;
  float const overreach = grip_distance - (reach * k_comfortable_fraction);
  solved.shoulder_drive =
      std::clamp(overreach / std::max(reach * 0.30F, 0.01F), 0.0F, 1.0F) * 0.12F;

  solved.front_foot_advance =
      std::max(0.0F, solved.weight_shift) * (0.075F + (0.075F * intent.thrust_amount));
  solved.back_foot_brace = std::max(0.0F, -solved.weight_shift) * 0.055F;

  PoseVec3 const off_shoulder{-k_weapon_shoulder_x, inputs.swing.shoulder_y, 0.0F};
  if (std::abs(inputs.offhand_along_weapon) > 1.0e-4F) {

    solved.offhand =
        add(swing.grip, scale(swing.blade_direction, inputs.offhand_along_weapon));
  } else {

    PoseVec3 const counter = normalize({-intent.strike_dir_x * 0.85F,
                                        (-intent.strike_dir_y * 0.45F) + 0.15F,
                                        0.40F - (0.25F * swing.commitment)},
                                       {-1.0F, 0.0F, 0.35F});
    solved.offhand = add(off_shoulder, scale(counter, reach * 0.58F));
  }

  return solved;
}

} // namespace Animation
