#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "../pose_manifest.h"

namespace Animation::PoseFk {

struct Mat3 {
  std::array<float, 9> m{1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
};

[[nodiscard]] inline auto identity() noexcept -> Mat3 {
  return Mat3{};
}

[[nodiscard]] inline auto rot_x(float radians) noexcept -> Mat3 {
  float const c = std::cos(radians);
  float const s = std::sin(radians);
  return Mat3{{1.0F, 0.0F, 0.0F, 0.0F, c, -s, 0.0F, s, c}};
}

[[nodiscard]] inline auto rot_y(float radians) noexcept -> Mat3 {
  float const c = std::cos(radians);
  float const s = std::sin(radians);
  return Mat3{{c, 0.0F, s, 0.0F, 1.0F, 0.0F, -s, 0.0F, c}};
}

[[nodiscard]] inline auto rot_z(float radians) noexcept -> Mat3 {
  float const c = std::cos(radians);
  float const s = std::sin(radians);
  return Mat3{{c, -s, 0.0F, s, c, 0.0F, 0.0F, 0.0F, 1.0F}};
}

[[nodiscard]] inline auto multiply(const Mat3& a, const Mat3& b) noexcept -> Mat3 {
  Mat3 out{};
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      float sum = 0.0F;
      for (int k = 0; k < 3; ++k) {
        sum += a.m[(row * 3) + k] * b.m[(k * 3) + col];
      }
      out.m[(row * 3) + col] = sum;
    }
  }
  return out;
}

[[nodiscard]] inline auto transform(const Mat3& a, PoseVec3 v) noexcept -> PoseVec3 {
  return {a.m[0] * v.x + a.m[1] * v.y + a.m[2] * v.z,
          a.m[3] * v.x + a.m[4] * v.y + a.m[5] * v.z,
          a.m[6] * v.x + a.m[7] * v.y + a.m[8] * v.z};
}

[[nodiscard]] inline auto add(PoseVec3 a, PoseVec3 b) noexcept -> PoseVec3 {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] inline auto sub(PoseVec3 a, PoseVec3 b) noexcept -> PoseVec3 {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] inline auto scaled(PoseVec3 a, float k) noexcept -> PoseVec3 {
  return {a.x * k, a.y * k, a.z * k};
}

[[nodiscard]] inline auto radians(float degrees) noexcept -> float {
  return degrees * (std::numbers::pi_v<float> / 180.0F);
}

[[nodiscard]] inline auto smoothstep(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

[[nodiscard]] inline auto ramp(float phase, float start, float span) noexcept -> float {
  return smoothstep((phase - start) / std::max(1.0e-4F, span));
}

struct LimbAim {
  float pitch{0.0F};
  float splay{0.0F};
  float yaw{0.0F};
  float bend{0.0F};
};

struct LimbSegments {
  PoseVec3 upper{};
  PoseVec3 lower{};
};

[[nodiscard]] inline auto limb_segments(const LimbAim& aim,
                                        float side_sign,
                                        float bend_sign,
                                        const Mat3& pre) noexcept -> LimbSegments {
  constexpr PoseVec3 k_down{0.0F, -1.0F, 0.0F};
  Mat3 const base = multiply(
      pre, multiply(rot_y(radians(aim.yaw)), rot_z(radians(aim.splay) * side_sign)));
  LimbSegments out{};
  out.upper = transform(multiply(base, rot_x(-radians(aim.pitch))), k_down);
  out.lower = transform(
      multiply(base, rot_x(-radians(aim.pitch + (bend_sign * aim.bend)))), k_down);
  return out;
}

[[nodiscard]] inline auto
blend_limb(const LimbAim& a, const LimbAim& b, float t) noexcept -> LimbAim {
  return {a.pitch + ((b.pitch - a.pitch) * t),
          a.splay + ((b.splay - a.splay) * t),
          a.yaw + ((b.yaw - a.yaw) * t),
          a.bend + ((b.bend - a.bend) * t)};
}

struct HumanoidRigMetrics {
  float pelvis_y{0.975F};
  float neck_rise{0.540F};
  float head_rise{0.140F};
  float shoulder_rise{0.450F};
  float shoulder_half_width{0.252F};
  float hip_half_width{0.100F};
  float hip_drop{0.020F};
  float upper_arm_len{0.320F};
  float fore_arm_len{0.270F};
  float upper_leg_len{0.500F};
  float lower_leg_len{0.470F};
};

} // namespace Animation::PoseFk
