#include "commander_spear_manifest.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "clip_manifest.h"

namespace Animation {

namespace {

using Keys = std::array<CommanderSpearPoseKey, 6>;

constexpr PoseVec3 k_guard_right_hand{0.26F, 1.06F, 0.16F};
constexpr PoseVec3 k_guard_left_hand{-0.08F, 1.12F, 0.52F};
constexpr PoseVec3 k_guard_spear_dir{0.05F, 0.55F, 0.85F};
constexpr float k_offhand_along_shaft = 0.44F;

[[nodiscard]] auto guard_key(float phase) noexcept -> CommanderSpearPoseKey {
  return {.phase = phase,
          .right_hand = k_guard_right_hand,
          .left_hand = k_guard_left_hand,
          .spear_dir = k_guard_spear_dir};
}

[[nodiscard]] auto add(PoseVec3 a, PoseVec3 b) noexcept -> PoseVec3 {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] auto scale(PoseVec3 v, float s) noexcept -> PoseVec3 {
  return {v.x * s, v.y * s, v.z * s};
}

[[nodiscard]] auto normalize(PoseVec3 v) noexcept -> PoseVec3 {
  float const len = std::sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
  return len > 1.0e-6F ? scale(v, 1.0F / len) : PoseVec3{0.0F, 0.0F, 1.0F};
}

const Keys k_thrust{{
    guard_key(0.00F),
    {.phase = 0.14F,
     .right_hand = {0.32F, 1.02F, -0.26F},
     .left_hand = {-0.04F, 1.10F, 0.22F},
     .spear_dir = {0.04F, 0.30F, 0.95F},
     .pelvis_delta = {0.02F, -0.02F, -0.10F},
     .shoulder_r_delta = {0.06F, 0.00F, -0.16F},
     .shoulder_l_delta = {-0.04F, 0.00F, 0.02F},
     .neck_delta = {0.00F, 0.00F, -0.06F},
     .head_delta = {0.00F, 0.00F, -0.04F},
     .foot_r_delta = {0.00F, 0.00F, -0.06F},
     .knee_r_delta = {0.00F, 0.00F, -0.04F}},
    {.phase = 0.32F,
     .right_hand = {0.28F, 0.98F, -0.04F},
     .left_hand = {-0.02F, 1.08F, 0.44F},
     .spear_dir = {0.03F, 0.16F, 0.99F},
     .pelvis_delta = {0.02F, -0.04F, 0.02F},
     .shoulder_r_delta = {0.04F, -0.02F, -0.04F},
     .foot_r_delta = {0.00F, 0.06F, 0.12F},
     .knee_r_delta = {0.00F, 0.05F, 0.12F}},
    {.phase = 0.48F,
     .right_hand = {0.16F, 1.08F, 0.84F},
     .left_hand = {0.02F, 1.10F, 1.22F},
     .spear_dir = {0.02F, 0.04F, 1.00F},
     .pelvis_delta = {0.00F, -0.14F, 0.44F},
     .shoulder_r_delta = {0.00F, -0.10F, 0.56F},
     .shoulder_l_delta = {0.00F, -0.04F, 0.20F},
     .neck_delta = {0.00F, -0.08F, 0.26F},
     .head_delta = {0.00F, -0.06F, 0.18F},
     .foot_r_delta = {0.00F, 0.00F, 0.54F},
     .knee_r_delta = {0.00F, 0.02F, 0.34F},
     .foot_l_delta = {0.00F, 0.00F, -0.04F},
     .knee_l_delta = {0.00F, -0.03F, 0.04F}},
    {.phase = 0.72F,
     .right_hand = {0.22F, 1.04F, 0.36F},
     .left_hand = {0.00F, 1.10F, 0.84F},
     .spear_dir = {0.03F, 0.14F, 0.99F},
     .pelvis_delta = {0.00F, -0.10F, 0.34F},
     .shoulder_r_delta = {0.00F, -0.06F, 0.36F},
     .shoulder_l_delta = {0.00F, -0.02F, 0.12F},
     .neck_delta = {0.00F, -0.05F, 0.18F},
     .head_delta = {0.00F, -0.04F, 0.12F},
     .foot_r_delta = {0.00F, 0.00F, 0.54F},
     .knee_r_delta = {0.00F, 0.02F, 0.34F},
     .knee_l_delta = {0.00F, -0.02F, 0.03F}},
    guard_key(1.00F),
}};

const Keys k_sweep{{
    guard_key(0.00F),
    {.phase = 0.16F,
     .right_hand = {0.44F, 1.10F, -0.16F},
     .left_hand = {0.18F, 1.16F, 0.30F},
     .spear_dir = {-0.52F, 0.18F, 0.83F},
     .pelvis_delta = {0.03F, -0.02F, -0.08F},
     .shoulder_r_delta = {0.10F, 0.02F, -0.16F},
     .shoulder_l_delta = {-0.06F, 0.00F, 0.08F},
     .neck_delta = {0.02F, 0.00F, -0.05F},
     .head_delta = {0.02F, 0.00F, -0.04F},
     .foot_r_delta = {0.00F, 0.00F, -0.06F},
     .knee_r_delta = {0.00F, 0.00F, -0.04F}},
    {.phase = 0.38F,
     .right_hand = {0.48F, 1.12F, 0.02F},
     .left_hand = {0.22F, 1.16F, 0.46F},
     .spear_dir = {-0.62F, 0.12F, 0.78F},
     .pelvis_delta = {0.03F, -0.04F, 0.00F},
     .shoulder_r_delta = {0.14F, 0.03F, -0.18F},
     .shoulder_l_delta = {-0.08F, -0.01F, 0.10F},
     .foot_r_delta = {0.00F, 0.06F, 0.10F},
     .knee_r_delta = {0.00F, 0.05F, 0.10F}},
    {.phase = 0.58F,
     .right_hand = {-0.20F, 1.00F, 0.62F},
     .left_hand = {-0.40F, 1.04F, 0.92F},
     .spear_dir = {0.74F, -0.06F, 0.67F},
     .pelvis_delta = {0.00F, -0.12F, 0.32F},
     .shoulder_r_delta = {-0.18F, -0.14F, 0.42F},
     .shoulder_l_delta = {-0.12F, 0.02F, 0.06F},
     .neck_delta = {-0.02F, -0.08F, 0.22F},
     .head_delta = {-0.02F, -0.06F, 0.16F},
     .foot_r_delta = {0.00F, 0.00F, 0.42F},
     .knee_r_delta = {0.00F, 0.02F, 0.26F},
     .knee_l_delta = {0.00F, -0.02F, 0.04F}},
    {.phase = 0.78F,
     .right_hand = {-0.34F, 0.96F, 0.46F},
     .left_hand = {-0.52F, 1.00F, 0.72F},
     .spear_dir = {0.86F, -0.14F, 0.49F},
     .pelvis_delta = {-0.02F, -0.10F, 0.26F},
     .shoulder_r_delta = {-0.24F, -0.10F, 0.28F},
     .shoulder_l_delta = {-0.08F, 0.00F, 0.02F},
     .neck_delta = {-0.03F, -0.06F, 0.14F},
     .head_delta = {-0.03F, -0.04F, 0.10F},
     .foot_r_delta = {0.00F, 0.00F, 0.42F},
     .knee_r_delta = {0.00F, 0.02F, 0.26F},
     .knee_l_delta = {0.00F, -0.02F, 0.03F}},
    guard_key(1.00F),
}};

const Keys k_launcher{{
    guard_key(0.00F),
    {.phase = 0.18F,
     .right_hand = {0.30F, 0.82F, -0.24F},
     .left_hand = {-0.02F, 0.92F, 0.22F},
     .spear_dir = {0.04F, 0.36F, 0.93F},
     .pelvis_delta = {0.02F, -0.14F, -0.08F},
     .shoulder_r_delta = {0.06F, -0.10F, -0.14F},
     .shoulder_l_delta = {-0.04F, -0.08F, 0.00F},
     .neck_delta = {0.00F, -0.08F, -0.06F},
     .head_delta = {0.00F, -0.06F, -0.04F},
     .foot_r_delta = {0.00F, 0.00F, -0.06F},
     .knee_r_delta = {0.00F, -0.04F, 0.00F},
     .knee_l_delta = {0.00F, -0.04F, 0.04F}},
    {.phase = 0.40F,
     .right_hand = {0.26F, 0.78F, -0.02F},
     .left_hand = {0.00F, 0.90F, 0.44F},
     .spear_dir = {0.03F, 0.46F, 0.89F},
     .pelvis_delta = {0.02F, -0.18F, 0.02F},
     .shoulder_r_delta = {0.04F, -0.14F, -0.04F},
     .shoulder_l_delta = {-0.02F, -0.10F, 0.04F},
     .neck_delta = {0.00F, -0.10F, -0.02F},
     .head_delta = {0.00F, -0.08F, 0.00F},
     .foot_r_delta = {0.00F, 0.06F, 0.12F},
     .knee_r_delta = {0.00F, 0.02F, 0.12F},
     .knee_l_delta = {0.00F, -0.05F, 0.05F}},
    {.phase = 0.58F,
     .right_hand = {0.14F, 1.20F, 0.70F},
     .left_hand = {0.02F, 1.48F, 1.02F},
     .spear_dir = {0.02F, 0.62F, 0.78F},
     .pelvis_delta = {0.00F, -0.02F, 0.36F},
     .shoulder_r_delta = {0.00F, 0.06F, 0.44F},
     .shoulder_l_delta = {0.00F, 0.08F, 0.20F},
     .neck_delta = {0.00F, 0.06F, 0.20F},
     .head_delta = {0.00F, 0.06F, 0.14F},
     .foot_r_delta = {0.00F, 0.00F, 0.46F},
     .knee_r_delta = {0.00F, 0.02F, 0.30F},
     .knee_l_delta = {0.00F, -0.02F, 0.04F}},
    {.phase = 0.80F,
     .right_hand = {0.20F, 1.08F, 0.36F},
     .left_hand = {0.00F, 1.26F, 0.78F},
     .spear_dir = {0.03F, 0.48F, 0.88F},
     .pelvis_delta = {0.00F, -0.06F, 0.28F},
     .shoulder_r_delta = {0.00F, 0.00F, 0.30F},
     .shoulder_l_delta = {0.00F, 0.02F, 0.12F},
     .neck_delta = {0.00F, 0.00F, 0.14F},
     .head_delta = {0.00F, 0.00F, 0.10F},
     .foot_r_delta = {0.00F, 0.00F, 0.46F},
     .knee_r_delta = {0.00F, 0.02F, 0.30F},
     .knee_l_delta = {0.00F, -0.02F, 0.03F}},
    guard_key(1.00F),
}};

const Keys k_finisher{{
    guard_key(0.00F),
    {.phase = 0.18F,
     .right_hand = {0.34F, 1.52F, -0.42F},
     .left_hand = {-0.04F, 1.34F, 0.02F},
     .spear_dir = {0.04F, -0.12F, 0.99F},
     .pelvis_delta = {0.02F, -0.02F, -0.14F},
     .shoulder_r_delta = {0.08F, 0.12F, -0.20F},
     .shoulder_l_delta = {-0.04F, 0.08F, -0.10F},
     .neck_delta = {0.00F, 0.02F, -0.10F},
     .head_delta = {0.00F, 0.02F, -0.08F},
     .foot_r_delta = {0.00F, 0.00F, -0.08F},
     .knee_r_delta = {0.00F, 0.00F, -0.05F}},
    {.phase = 0.46F,
     .right_hand = {0.30F, 1.62F, -0.20F},
     .left_hand = {-0.02F, 1.40F, 0.24F},
     .spear_dir = {0.03F, -0.22F, 0.98F},
     .pelvis_delta = {0.02F, -0.05F, -0.04F},
     .shoulder_r_delta = {0.10F, 0.18F, -0.16F},
     .shoulder_l_delta = {-0.04F, 0.12F, -0.08F},
     .neck_delta = {0.00F, 0.06F, -0.08F},
     .head_delta = {0.00F, 0.06F, -0.06F},
     .foot_r_delta = {0.00F, 0.10F, 0.14F},
     .knee_r_delta = {0.00F, 0.08F, 0.14F}},
    {.phase = 0.66F,
     .right_hand = {0.14F, 0.98F, 0.78F},
     .left_hand = {0.02F, 0.72F, 1.20F},
     .spear_dir = {0.02F, -0.44F, 0.90F},
     .pelvis_delta = {0.00F, -0.24F, 0.46F},
     .shoulder_r_delta = {0.02F, -0.38F, 0.56F},
     .shoulder_l_delta = {-0.06F, -0.20F, 0.36F},
     .neck_delta = {0.00F, -0.26F, 0.40F},
     .head_delta = {0.00F, -0.20F, 0.30F},
     .foot_r_delta = {0.00F, 0.00F, 0.60F},
     .knee_r_delta = {0.00F, 0.02F, 0.38F},
     .knee_l_delta = {0.00F, -0.04F, 0.08F}},
    {.phase = 0.86F,
     .right_hand = {0.18F, 0.94F, 0.48F},
     .left_hand = {0.02F, 0.74F, 0.92F},
     .spear_dir = {0.03F, -0.34F, 0.94F},
     .pelvis_delta = {-0.02F, -0.18F, 0.36F},
     .shoulder_r_delta = {-0.04F, -0.30F, 0.38F},
     .shoulder_l_delta = {-0.04F, -0.16F, 0.24F},
     .neck_delta = {0.00F, -0.20F, 0.28F},
     .head_delta = {0.00F, -0.14F, 0.20F},
     .foot_r_delta = {0.00F, 0.00F, 0.60F},
     .knee_r_delta = {0.00F, 0.02F, 0.38F},
     .knee_l_delta = {0.00F, -0.03F, 0.06F}},
    guard_key(1.00F),
}};

[[nodiscard]] auto keys_for(CommanderSpearMove move) noexcept -> const Keys& {
  switch (move) {
  case CommanderSpearMove::Sweep:
    return k_sweep;
  case CommanderSpearMove::Launcher:
    return k_launcher;
  case CommanderSpearMove::Finisher:
    return k_finisher;
  case CommanderSpearMove::Thrust:
  case CommanderSpearMove::None:
    break;
  }
  return k_thrust;
}

using Channel = PoseVec3 CommanderSpearPoseKey::*;

[[nodiscard]] auto hermite(const Keys& keys,
                           std::size_t segment,
                           float t,
                           Channel channel) noexcept -> PoseVec3 {
  auto const& from = keys[segment - 1U];
  auto const& to = keys[segment];
  float const span = std::max(0.001F, to.phase - from.phase);
  auto const tangent = [&keys, channel](std::size_t index) -> PoseVec3 {
    if (index == 0U || index + 1U >= keys.size()) {
      return {};
    }
    float const window =
        std::max(0.001F, keys[index + 1U].phase - keys[index - 1U].phase);
    return scale(
        add(keys[index + 1U].*channel, scale(keys[index - 1U].*channel, -1.0F)),
        1.0F / window);
  };
  float const t2 = t * t;
  float const t3 = t2 * t;
  float const h00 = (2.0F * t3) - (3.0F * t2) + 1.0F;
  float const h10 = t3 - (2.0F * t2) + t;
  float const h01 = (-2.0F * t3) + (3.0F * t2);
  float const h11 = t3 - t2;
  return add(add(scale(from.*channel, h00), scale(tangent(segment - 1U), span * h10)),
             add(scale(to.*channel, h01), scale(tangent(segment), span * h11)));
}

} // namespace

auto resolve_commander_spear_pose(CommanderSpearMove move,
                                  float phase) noexcept -> CommanderSpearPoseKey {
  auto const& keys = keys_for(move);
  float const clamped = std::clamp(phase, 0.0F, 1.0F);
  std::size_t segment = 1U;
  while (segment + 1U < keys.size() && clamped > keys[segment].phase) {
    ++segment;
  }
  auto const& from = keys[segment - 1U];
  auto const& to = keys[segment];
  float const span = std::max(0.001F, to.phase - from.phase);
  float const t = std::clamp((clamped - from.phase) / span, 0.0F, 1.0F);

  auto const sample = [&](Channel channel) {
    return hermite(keys, segment, t, channel);
  };
  float const dir_t = t * t * (3.0F - (2.0F * t));
  PoseVec3 const right_hand = sample(&CommanderSpearPoseKey::right_hand);
  PoseVec3 const spear_dir =
      normalize(add(scale(normalize(from.spear_dir), 1.0F - dir_t),
                    scale(normalize(to.spear_dir), dir_t)));

  PoseVec3 const left_hand = add(right_hand, scale(spear_dir, k_offhand_along_shaft));
  return {
      .phase = clamped,
      .right_hand = right_hand,
      .left_hand = left_hand,
      .spear_dir = spear_dir,
      .pelvis_delta = sample(&CommanderSpearPoseKey::pelvis_delta),
      .shoulder_r_delta = sample(&CommanderSpearPoseKey::shoulder_r_delta),
      .shoulder_l_delta = sample(&CommanderSpearPoseKey::shoulder_l_delta),
      .neck_delta = sample(&CommanderSpearPoseKey::neck_delta),
      .head_delta = sample(&CommanderSpearPoseKey::head_delta),
      .foot_r_delta = sample(&CommanderSpearPoseKey::foot_r_delta),
      .knee_r_delta = sample(&CommanderSpearPoseKey::knee_r_delta),
      .foot_l_delta = sample(&CommanderSpearPoseKey::foot_l_delta),
      .knee_l_delta = sample(&CommanderSpearPoseKey::knee_l_delta),
  };
}

auto commander_spear_move_for_clip(std::uint16_t clip_id) noexcept
    -> CommanderSpearMove {
  switch (clip_id) {
  case k_humanoid_rpg_spear_thrust_clip:
    return CommanderSpearMove::Thrust;
  case k_humanoid_rpg_spear_sweep_clip:
    return CommanderSpearMove::Sweep;
  case k_humanoid_rpg_spear_launcher_clip:
    return CommanderSpearMove::Launcher;
  case k_humanoid_rpg_spear_finisher_clip:
    return CommanderSpearMove::Finisher;
  default:
    return CommanderSpearMove::None;
  }
}

auto commander_spear_clip_for_move(CommanderSpearMove move) noexcept -> std::uint16_t {
  switch (move) {
  case CommanderSpearMove::Thrust:
    return k_humanoid_rpg_spear_thrust_clip;
  case CommanderSpearMove::Sweep:
    return k_humanoid_rpg_spear_sweep_clip;
  case CommanderSpearMove::Launcher:
    return k_humanoid_rpg_spear_launcher_clip;
  case CommanderSpearMove::Finisher:
    return k_humanoid_rpg_spear_finisher_clip;
  case CommanderSpearMove::None:
    break;
  }
  return k_unmapped_clip;
}

} // namespace Animation
