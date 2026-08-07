#include "death_pose_manifest.h"

#include <algorithm>
#include <array>
#include <span>

namespace Animation {

namespace {

using PoseFk::LimbAim;
using PoseFk::Mat3;

struct DeathKey {
  float t{0.0F};
  float root_x{0.0F};
  float root_y{0.975F};
  float root_z{0.0F};
  float body_pitch{0.0F};
  float body_roll{0.0F};
  float body_yaw{0.0F};
  float spine_pitch{0.0F};
  float spine_roll{0.0F};
  float spine_yaw{0.0F};
  float head_pitch{0.0F};
  float head_roll{0.0F};
  float foot_pitch_l{0.0F};
  float foot_pitch_r{0.0F};
  LimbAim arm_l{4.0F, 7.0F, 0.0F, 14.0F};
  LimbAim arm_r{4.0F, 7.0F, 0.0F, 14.0F};
  LimbAim leg_l{0.0F, 3.0F, 0.0F, 4.0F};
  LimbAim leg_r{0.0F, 3.0F, 0.0F, 4.0F};
};

constexpr std::array<DeathKey, 8> k_back_sprawl_keys{{
    {.t = 0.00F},
    {.t = 0.07F,
     .root_y = 0.955F,
     .root_z = -0.03F,
     .body_pitch = -5.0F,
     .spine_pitch = -15.0F,
     .head_pitch = -20.0F,
     .arm_l = {-34.0F, 40.0F, 0.0F, 22.0F},
     .arm_r = {-30.0F, 36.0F, 0.0F, 26.0F},
     .leg_l = {2.0F, 6.0F, 0.0F, 14.0F},
     .leg_r = {2.0F, 5.0F, 0.0F, 14.0F}},
    {.t = 0.20F,
     .root_y = 0.905F,
     .root_z = -0.13F,
     .body_pitch = -13.0F,
     .spine_pitch = -11.0F,
     .head_pitch = -16.0F,
     .arm_l = {-22.0F, 50.0F, 0.0F, 30.0F},
     .arm_r = {-18.0F, 46.0F, 0.0F, 34.0F},
     .leg_l = {20.0F, 7.0F, 0.0F, 51.0F},
     .leg_r = {2.0F, 5.0F, 0.0F, 51.0F}},
    {.t = 0.42F,
     .root_y = 0.600F,
     .root_z = -0.24F,
     .body_pitch = -20.0F,
     .spine_pitch = -2.0F,
     .head_pitch = 2.0F,
     .arm_l = {-26.0F, 56.0F, 0.0F, 30.0F},
     .arm_r = {-22.0F, 52.0F, 0.0F, 34.0F},
     .leg_l = {39.0F, 9.0F, 0.0F, 112.0F},
     .leg_r = {35.0F, 6.0F, 0.0F, 106.0F}},
    {.t = 0.62F,
     .root_y = 0.300F,
     .root_z = -0.34F,
     .body_pitch = -44.0F,
     .spine_pitch = 8.0F,
     .head_pitch = 14.0F,
     .arm_l = {-18.0F, 58.0F, 0.0F, 26.0F},
     .arm_r = {-14.0F, 50.0F, 0.0F, 34.0F},
     .leg_l = {70.0F, 11.0F, 0.0F, 119.0F},
     .leg_r = {69.0F, 7.0F, 0.0F, 125.0F}},
    {.t = 0.78F,
     .root_y = 0.175F,
     .root_z = -0.44F,
     .body_pitch = -82.0F,
     .spine_pitch = 4.0F,
     .head_pitch = 8.0F,
     .foot_pitch_l = -18.0F,
     .foot_pitch_r = -14.0F,
     .arm_l = {-8.0F, 54.0F, 0.0F, 20.0F},
     .arm_r = {-6.0F, 44.0F, 0.0F, 32.0F},
     .leg_l = {23.0F, 12.0F, 0.0F, 52.0F},
     .leg_r = {29.0F, 8.0F, 0.0F, 62.0F}},
    {.t = 0.88F,
     .root_y = 0.178F,
     .root_z = -0.50F,
     .body_pitch = -96.0F,
     .spine_pitch = -4.0F,
     .head_pitch = 2.0F,
     .head_roll = 4.0F,
     .foot_pitch_l = -26.0F,
     .foot_pitch_r = -22.0F,
     .arm_l = {-16.0F, 56.0F, 0.0F, 12.0F},
     .arm_r = {-14.0F, 38.0F, 0.0F, 30.0F},
     .leg_l = {-5.0F, 12.0F, 0.0F, 24.0F},
     .leg_r = {5.0F, 8.0F, 0.0F, 47.0F}},
    {.t = 1.00F,
     .root_y = 0.158F,
     .root_z = -0.52F,
     .body_pitch = -92.0F,
     .body_roll = 5.0F,
     .spine_roll = 4.0F,
     .head_pitch = 2.0F,
     .head_roll = 10.0F,
     .foot_pitch_l = -30.0F,
     .foot_pitch_r = -24.0F,
     .arm_l = {-9.0F, 52.0F, 0.0F, 14.0F},
     .arm_r = {-7.0F, 26.0F, 0.0F, 42.0F},
     .leg_l = {-8.0F, 9.0F, 0.0F, 3.0F},
     .leg_r = {10.0F, 4.0F, 0.0F, 41.0F}},
}};

constexpr std::array<DeathKey, 8> k_face_plant_keys{{
    {.t = 0.00F},
    {.t = 0.08F,
     .root_y = 0.955F,
     .root_z = 0.04F,
     .body_pitch = 6.0F,
     .spine_pitch = 14.0F,
     .head_pitch = 16.0F,
     .arm_l = {26.0F, 14.0F, 0.0F, 30.0F},
     .arm_r = {22.0F, 12.0F, 0.0F, 34.0F},
     .leg_l = {2.0F, 4.0F, 0.0F, 8.0F},
     .leg_r = {2.0F, 4.0F, 0.0F, 8.0F}},
    {.t = 0.22F,
     .root_y = 0.860F,
     .root_z = 0.10F,
     .body_pitch = 20.0F,
     .spine_pitch = 18.0F,
     .head_pitch = 12.0F,
     .arm_l = {46.0F, 18.0F, 0.0F, 40.0F},
     .arm_r = {42.0F, 16.0F, 0.0F, 44.0F},
     .leg_l = {20.0F, 5.0F, 0.0F, 46.0F},
     .leg_r = {16.0F, 5.0F, 0.0F, 42.0F}},
    {.t = 0.42F,
     .root_y = 0.580F,
     .root_z = 0.10F,
     .body_pitch = 34.0F,
     .spine_pitch = 12.0F,
     .head_pitch = 10.0F,
     .foot_pitch_l = 20.0F,
     .foot_pitch_r = 18.0F,
     .arm_l = {60.0F, 22.0F, 0.0F, 32.0F},
     .arm_r = {56.0F, 20.0F, 0.0F, 36.0F},
     .leg_l = {35.0F, 7.0F, 0.0F, 97.0F},
     .leg_r = {42.0F, 6.0F, 0.0F, 106.0F}},
    {.t = 0.62F,
     .root_y = 0.470F,
     .root_z = 0.24F,
     .body_pitch = 62.0F,
     .spine_pitch = 6.0F,
     .head_pitch = 4.0F,
     .foot_pitch_l = 22.0F,
     .foot_pitch_r = 20.0F,
     .arm_l = {30.0F, 26.0F, 0.0F, 24.0F},
     .arm_r = {26.0F, 24.0F, 0.0F, 28.0F},
     .leg_l = {24.0F, 8.0F, 0.0F, 64.0F},
     .leg_r = {24.0F, 7.0F, 0.0F, 70.0F}},
    {.t = 0.80F,
     .root_y = 0.170F,
     .root_z = 0.40F,
     .body_pitch = 88.0F,
     .spine_pitch = -2.0F,
     .head_pitch = -6.0F,
     .foot_pitch_l = 16.0F,
     .foot_pitch_r = 12.0F,
     .arm_l = {14.0F, 46.0F, 0.0F, 6.0F},
     .arm_r = {12.0F, 30.0F, 0.0F, 6.0F},
     .leg_l = {6.0F, 8.0F, 0.0F, 2.0F},
     .leg_r = {4.0F, 5.0F, 0.0F, 2.0F}},
    {.t = 0.90F,
     .root_y = 0.155F,
     .root_z = 0.44F,
     .body_pitch = 95.0F,
     .spine_pitch = -5.0F,
     .head_pitch = -12.0F,
     .head_roll = 10.0F,
     .foot_pitch_l = 22.0F,
     .foot_pitch_r = 18.0F,
     .arm_l = {12.0F, 38.0F, 0.0F, 4.0F},
     .arm_r = {10.0F, 22.0F, 0.0F, 6.0F},
     .leg_l = {11.0F, 8.0F, 0.0F, 2.0F},
     .leg_r = {9.0F, 5.0F, 0.0F, 2.0F}},
    {.t = 1.00F,
     .root_y = 0.150F,
     .root_z = 0.43F,
     .body_pitch = 90.0F,
     .body_roll = -4.0F,
     .spine_roll = -4.0F,
     .head_pitch = -4.0F,
     .head_roll = 14.0F,
     .foot_pitch_l = 24.0F,
     .foot_pitch_r = 20.0F,
     .arm_l = {11.0F, 26.0F, 0.0F, 6.0F},
     .arm_r = {9.0F, 34.0F, 0.0F, 10.0F},
     .leg_l = {6.0F, 8.0F, 0.0F, 3.0F},
     .leg_r = {4.0F, 4.0F, 0.0F, 4.0F}},
}};

constexpr std::array<DeathKey, 8> k_side_crumple_keys{{
    {.t = 0.00F},
    {.t = 0.08F,
     .root_x = -0.03F,
     .root_y = 0.955F,
     .body_roll = 8.0F,
     .spine_roll = 10.0F,
     .head_pitch = -8.0F,
     .head_roll = 14.0F,
     .arm_l = {-10.0F, 34.0F, 0.0F, 26.0F},
     .arm_r = {-24.0F, 30.0F, 0.0F, 30.0F},
     .leg_l = {2.0F, 6.0F, 0.0F, 18.0F},
     .leg_r = {2.0F, 5.0F, 0.0F, 14.0F}},
    {.t = 0.22F,
     .root_x = -0.11F,
     .root_y = 0.905F,
     .root_z = -0.06F,
     .body_pitch = -8.0F,
     .body_roll = 18.0F,
     .body_yaw = 10.0F,
     .spine_roll = 12.0F,
     .head_roll = 12.0F,
     .arm_l = {-6.0F, 44.0F, 0.0F, 34.0F},
     .arm_r = {-16.0F, 34.0F, 0.0F, 40.0F},
     .leg_l = {26.0F, 9.0F, 0.0F, 36.0F},
     .leg_r = {4.0F, 5.0F, 0.0F, 34.0F}},
    {.t = 0.42F,
     .root_x = -0.22F,
     .root_y = 0.580F,
     .root_z = -0.16F,
     .body_pitch = -18.0F,
     .body_roll = 30.0F,
     .body_yaw = 16.0F,
     .spine_roll = 8.0F,
     .head_roll = 10.0F,
     .arm_l = {6.0F, 40.0F, 0.0F, 44.0F},
     .arm_r = {-6.0F, 30.0F, 0.0F, 48.0F},
     .leg_l = {52.0F, 10.0F, 0.0F, 76.0F},
     .leg_r = {34.0F, 6.0F, 0.0F, 70.0F}},
    {.t = 0.62F,
     .root_x = -0.30F,
     .root_y = 0.320F,
     .root_z = -0.24F,
     .body_pitch = -40.0F,
     .body_roll = 40.0F,
     .body_yaw = 18.0F,
     .spine_roll = 6.0F,
     .head_roll = 6.0F,
     .arm_l = {2.0F, 30.0F, 0.0F, 46.0F},
     .arm_r = {-4.0F, 42.0F, 0.0F, 40.0F},
     .leg_l = {66.0F, 11.0F, 0.0F, 122.0F},
     .leg_r = {62.0F, 7.0F, 0.0F, 128.0F}},
    {.t = 0.80F,
     .root_x = -0.35F,
     .root_y = 0.190F,
     .root_z = -0.32F,
     .body_pitch = -74.0F,
     .body_roll = 44.0F,
     .body_yaw = 18.0F,
     .head_pitch = 6.0F,
     .head_roll = 6.0F,
     .foot_pitch_l = -12.0F,
     .foot_pitch_r = -10.0F,
     .arm_l = {-4.0F, 22.0F, 0.0F, 42.0F},
     .arm_r = {-6.0F, 46.0F, 0.0F, 30.0F},
     .leg_l = {26.0F, 11.0F, 0.0F, 56.0F},
     .leg_r = {20.0F, 7.0F, 0.0F, 32.0F}},
    {.t = 0.90F,
     .root_x = -0.37F,
     .root_y = 0.178F,
     .root_z = -0.36F,
     .body_pitch = -86.0F,
     .body_roll = 46.0F,
     .body_yaw = 18.0F,
     .head_pitch = 10.0F,
     .foot_pitch_l = -16.0F,
     .foot_pitch_r = -14.0F,
     .arm_l = {-10.0F, 16.0F, 0.0F, 38.0F},
     .arm_r = {-6.0F, 48.0F, 0.0F, 24.0F},
     .leg_l = {12.0F, 11.0F, 0.0F, 44.0F},
     .leg_r = {24.0F, 7.0F, 0.0F, 58.0F}},
    {.t = 1.00F,
     .root_x = -0.37F,
     .root_y = 0.168F,
     .root_z = -0.37F,
     .body_pitch = -80.0F,
     .body_roll = 46.0F,
     .body_yaw = 18.0F,
     .spine_roll = 6.0F,
     .head_pitch = 12.0F,
     .head_roll = -6.0F,
     .foot_pitch_l = -18.0F,
     .foot_pitch_r = -15.0F,
     .arm_l = {-8.0F, 14.0F, 0.0F, 36.0F},
     .arm_r = {-9.0F, 44.0F, 0.0F, 22.0F},
     .leg_l = {14.0F, 11.0F, 0.0F, 48.0F},
     .leg_r = {26.0F, 7.0F, 0.0F, 62.0F}},
}};

constexpr std::array<DeathKey, 7> k_mounted_unseat_keys{{
    {.t = 0.00F},
    {.t = 0.10F,
     .root_x = 0.10F,
     .root_y = 0.965F,
     .body_roll = -12.0F,
     .spine_roll = -14.0F,
     .head_roll = -18.0F,
     .arm_l = {-20.0F, 26.0F, 0.0F, 34.0F},
     .arm_r = {-34.0F, 40.0F, 0.0F, 24.0F},
     .leg_l = {-6.0F, 8.0F, 0.0F, 10.0F},
     .leg_r = {-6.0F, 8.0F, 0.0F, 10.0F}},
    {.t = 0.28F,
     .root_x = 0.32F,
     .root_y = 0.890F,
     .root_z = -0.08F,
     .body_pitch = -10.0F,
     .body_roll = -22.0F,
     .body_yaw = -14.0F,
     .spine_roll = -12.0F,
     .head_roll = -14.0F,
     .arm_l = {-10.0F, 34.0F, 0.0F, 42.0F},
     .arm_r = {-20.0F, 48.0F, 0.0F, 30.0F},
     .leg_l = {22.0F, 12.0F, 0.0F, 54.0F},
     .leg_r = {6.0F, 6.0F, 0.0F, 52.0F}},
    {.t = 0.50F,
     .root_x = 0.54F,
     .root_y = 0.580F,
     .root_z = -0.18F,
     .body_pitch = -22.0F,
     .body_roll = -34.0F,
     .body_yaw = -20.0F,
     .arm_l = {0.0F, 38.0F, 0.0F, 46.0F},
     .arm_r = {-8.0F, 50.0F, 0.0F, 38.0F},
     .leg_l = {26.0F, 14.0F, 0.0F, 74.0F},
     .leg_r = {48.0F, 8.0F, 0.0F, 106.0F}},
    {.t = 0.72F,
     .root_x = 0.68F,
     .root_y = 0.250F,
     .root_z = -0.26F,
     .body_pitch = -54.0F,
     .body_roll = -40.0F,
     .body_yaw = -22.0F,
     .head_pitch = 8.0F,
     .foot_pitch_l = -14.0F,
     .foot_pitch_r = -12.0F,
     .arm_l = {-4.0F, 30.0F, 0.0F, 42.0F},
     .arm_r = {-8.0F, 46.0F, 0.0F, 32.0F},
     .leg_l = {37.0F, 12.0F, 0.0F, 50.0F},
     .leg_r = {42.0F, 7.0F, 0.0F, 44.0F}},
    {.t = 0.88F,
     .root_x = 0.75F,
     .root_y = 0.186F,
     .root_z = -0.30F,
     .body_pitch = -88.0F,
     .body_roll = -42.0F,
     .body_yaw = -20.0F,
     .head_pitch = 12.0F,
     .head_roll = -8.0F,
     .foot_pitch_l = -20.0F,
     .foot_pitch_r = -18.0F,
     .arm_l = {-6.0F, 20.0F, 0.0F, 32.0F},
     .arm_r = {-14.0F, 42.0F, 0.0F, 26.0F},
     .leg_l = {14.0F, 12.0F, 0.0F, 30.0F},
     .leg_r = {-10.0F, 6.0F, 0.0F, 3.0F}},
    {.t = 1.00F,
     .root_x = 0.76F,
     .root_y = 0.172F,
     .root_z = -0.30F,
     .body_pitch = -82.0F,
     .body_roll = -40.0F,
     .body_yaw = -18.0F,
     .spine_roll = -6.0F,
     .head_pitch = 10.0F,
     .head_roll = -12.0F,
     .foot_pitch_l = -22.0F,
     .foot_pitch_r = -18.0F,
     .arm_l = {-9.0F, 18.0F, 0.0F, 34.0F},
     .arm_r = {-8.0F, 40.0F, 0.0F, 24.0F},
     .leg_l = {15.0F, 11.0F, 0.0F, 25.0F},
     .leg_r = {-4.0F, 6.0F, 0.0F, 1.0F}},
}};

[[nodiscard]] auto
keys_for(HumanoidDeathCollapse collapse) noexcept -> std::span<const DeathKey> {
  switch (collapse) {
  case HumanoidDeathCollapse::BackSprawl:
    return k_back_sprawl_keys;
  case HumanoidDeathCollapse::FacePlant:
    return k_face_plant_keys;
  case HumanoidDeathCollapse::SideCrumple:
    return k_side_crumple_keys;
  case HumanoidDeathCollapse::MountedUnseat:
    return k_mounted_unseat_keys;
  case HumanoidDeathCollapse::None:
  case HumanoidDeathCollapse::Count:
    break;
  }
  return {};
}

[[nodiscard]] auto mix(float a, float b, float t) noexcept -> float {
  return a + ((b - a) * t);
}

[[nodiscard]] auto
blend_key(const DeathKey& a, const DeathKey& b, float t) noexcept -> DeathKey {
  DeathKey out{};
  out.t = mix(a.t, b.t, t);
  out.root_x = mix(a.root_x, b.root_x, t);
  out.root_y = mix(a.root_y, b.root_y, t);
  out.root_z = mix(a.root_z, b.root_z, t);
  out.body_pitch = mix(a.body_pitch, b.body_pitch, t);
  out.body_roll = mix(a.body_roll, b.body_roll, t);
  out.body_yaw = mix(a.body_yaw, b.body_yaw, t);
  out.spine_pitch = mix(a.spine_pitch, b.spine_pitch, t);
  out.spine_roll = mix(a.spine_roll, b.spine_roll, t);
  out.spine_yaw = mix(a.spine_yaw, b.spine_yaw, t);
  out.head_pitch = mix(a.head_pitch, b.head_pitch, t);
  out.head_roll = mix(a.head_roll, b.head_roll, t);
  out.foot_pitch_l = mix(a.foot_pitch_l, b.foot_pitch_l, t);
  out.foot_pitch_r = mix(a.foot_pitch_r, b.foot_pitch_r, t);
  out.arm_l = PoseFk::blend_limb(a.arm_l, b.arm_l, t);
  out.arm_r = PoseFk::blend_limb(a.arm_r, b.arm_r, t);
  out.leg_l = PoseFk::blend_limb(a.leg_l, b.leg_l, t);
  out.leg_r = PoseFk::blend_limb(a.leg_r, b.leg_r, t);
  return out;
}

[[nodiscard]] auto
segment_ease(const DeathKey& a, const DeathKey& b, float t) noexcept -> float {
  float const drop = a.root_y - b.root_y;
  if (drop > 0.08F) {
    return t * t;
  }
  return PoseFk::smoothstep(t);
}

[[nodiscard]] auto sample_key(std::span<const DeathKey> keys,
                              float phase) noexcept -> DeathKey {
  phase = std::clamp(phase, 0.0F, 1.0F);
  if (keys.empty()) {
    return {};
  }
  if (phase <= keys.front().t) {
    return keys.front();
  }
  for (std::size_t i = 0; i + 1 < keys.size(); ++i) {
    const DeathKey& a = keys[i];
    const DeathKey& b = keys[i + 1];
    if (phase <= b.t) {
      float const span = std::max(1.0e-5F, b.t - a.t);
      return blend_key(a, b, segment_ease(a, b, (phase - a.t) / span));
    }
  }
  return keys.back();
}

} // namespace

auto humanoid_death_collapse_name(HumanoidDeathCollapse collapse) noexcept
    -> std::string_view {
  switch (collapse) {
  case HumanoidDeathCollapse::BackSprawl:
    return "back_sprawl";
  case HumanoidDeathCollapse::FacePlant:
    return "face_plant";
  case HumanoidDeathCollapse::SideCrumple:
    return "side_crumple";
  case HumanoidDeathCollapse::MountedUnseat:
    return "mounted_unseat";
  case HumanoidDeathCollapse::None:
  case HumanoidDeathCollapse::Count:
    break;
  }
  return "none";
}

auto resolve_humanoid_death_pose(const HumanoidDeathPoseInputs& inputs) noexcept
    -> HumanoidDeathPoseSample {
  auto const keys = keys_for(inputs.collapse);
  if (keys.empty()) {
    return {};
  }
  DeathKey const key = sample_key(keys, inputs.phase);
  PoseFk::HumanoidRigMetrics const& rig = inputs.rig;

  using PoseFk::add;
  using PoseFk::multiply;
  using PoseFk::radians;
  using PoseFk::rot_x;
  using PoseFk::rot_y;
  using PoseFk::rot_z;
  using PoseFk::scaled;
  using PoseFk::sub;
  using PoseFk::transform;

  Mat3 const spine =
      multiply(rot_x(radians(key.spine_pitch)),
               multiply(rot_z(radians(key.spine_roll)), rot_y(radians(key.spine_yaw))));

  PoseVec3 const pelvis{0.0F, rig.pelvis_y, 0.0F};
  PoseVec3 const neck = add(pelvis, transform(spine, {0.0F, rig.neck_rise, 0.0F}));
  PoseVec3 const shoulder_center =
      add(pelvis, transform(spine, {0.0F, rig.shoulder_rise, 0.0F}));
  PoseVec3 const shoulder_l =
      add(shoulder_center, transform(spine, {-rig.shoulder_half_width, 0.0F, 0.0F}));
  PoseVec3 const shoulder_r =
      add(shoulder_center, transform(spine, {rig.shoulder_half_width, 0.0F, 0.0F}));
  Mat3 const skull = multiply(
      spine, multiply(rot_x(radians(key.head_pitch)), rot_z(radians(key.head_roll))));
  PoseVec3 const head = add(neck, transform(skull, {0.0F, rig.head_rise, 0.0F}));

  auto const arm_l = PoseFk::limb_segments(key.arm_l, -1.0F, 1.0F, spine);
  auto const arm_r = PoseFk::limb_segments(key.arm_r, 1.0F, 1.0F, spine);
  PoseVec3 const elbow_l = add(shoulder_l, scaled(arm_l.upper, rig.upper_arm_len));
  PoseVec3 const hand_l = add(elbow_l, scaled(arm_l.lower, rig.fore_arm_len));
  PoseVec3 const elbow_r = add(shoulder_r, scaled(arm_r.upper, rig.upper_arm_len));
  PoseVec3 const hand_r = add(elbow_r, scaled(arm_r.lower, rig.fore_arm_len));

  Mat3 const upright = PoseFk::identity();
  PoseVec3 const hip_l = add(pelvis, {-rig.hip_half_width, -rig.hip_drop, 0.0F});
  PoseVec3 const hip_r = add(pelvis, {rig.hip_half_width, -rig.hip_drop, 0.0F});
  auto const leg_l = PoseFk::limb_segments(key.leg_l, -1.0F, -1.0F, upright);
  auto const leg_r = PoseFk::limb_segments(key.leg_r, 1.0F, -1.0F, upright);
  PoseVec3 const knee_l = add(hip_l, scaled(leg_l.upper, rig.upper_leg_len));
  PoseVec3 const foot_l = add(knee_l, scaled(leg_l.lower, rig.lower_leg_len));
  PoseVec3 const knee_r = add(hip_r, scaled(leg_r.upper, rig.upper_leg_len));
  PoseVec3 const foot_r = add(knee_r, scaled(leg_r.lower, rig.lower_leg_len));

  Mat3 const body =
      multiply(rot_x(radians(key.body_pitch)),
               multiply(rot_z(radians(key.body_roll)), rot_y(radians(key.body_yaw))));
  PoseVec3 const root{key.root_x, key.root_y, key.root_z};
  float const height = std::max(0.1F, inputs.height_scale);

  auto place = [&](PoseVec3 p) {
    return scaled(add(transform(body, sub(p, pelvis)), root), height);
  };

  HumanoidDeathPoseSample sample{};
  sample.active = true;
  sample.pelvis = place(pelvis);
  sample.neck_base = place(neck);
  sample.head = place(head);
  sample.shoulder_l = place(shoulder_l);
  sample.shoulder_r = place(shoulder_r);
  sample.elbow_l = place(elbow_l);
  sample.elbow_r = place(elbow_r);
  sample.hand_l = place(hand_l);
  sample.hand_r = place(hand_r);
  sample.knee_l = place(knee_l);
  sample.knee_r = place(knee_r);
  sample.foot_l = place(foot_l);
  sample.foot_r = place(foot_r);
  sample.foot_pitch_l = radians(key.foot_pitch_l);
  sample.foot_pitch_r = radians(key.foot_pitch_r);
  return sample;
}

} // namespace Animation
