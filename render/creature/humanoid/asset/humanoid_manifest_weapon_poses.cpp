#include "render/creature/humanoid/asset/humanoid_manifest_internal.h"

namespace Render::Humanoid {

namespace humanoid_manifest_detail {

auto blend_vec(const QVector3D& from, const QVector3D& to, float t) -> QVector3D {
  return from * (1.0F - t) + to * t;
}

auto normalized_or(const QVector3D& value, const QVector3D& fallback) -> QVector3D {
  QVector3D out = value;
  if (out.lengthSquared() <= 1.0e-6F) {
    out = fallback;
  }
  if (out.lengthSquared() <= 1.0e-6F) {
    return {0.0F, 1.0F, 0.0F};
  }
  out.normalize();
  return out;
}

auto attachment_frame_matrix(const Render::GL::AttachmentFrame& frame) -> QMatrix4x4 {
  QMatrix4x4 out;
  out.setColumn(
      0, QVector4D(normalized_or(frame.right, QVector3D(1.0F, 0.0F, 0.0F)), 0.0F));
  out.setColumn(1,
                QVector4D(normalized_or(frame.up, QVector3D(0.0F, 1.0F, 0.0F)), 0.0F));
  out.setColumn(
      2, QVector4D(normalized_or(frame.forward, QVector3D(0.0F, 0.0F, 1.0F)), 0.0F));
  out.setColumn(3, QVector4D(frame.origin, 1.0F));
  return out;
}

auto sword_local_pose(const QVector3D& blade_axis_local) -> QMatrix4x4 {
  QVector3D const blade_dir =
      normalized_or(blade_axis_local, QVector3D(0.0F, 1.0F, 0.0F));

  QVector3D guard_right(0.0F, 0.0F, 1.0F);
  guard_right -= blade_dir * QVector3D::dotProduct(guard_right, blade_dir);
  guard_right = normalized_or(guard_right, QVector3D(1.0F, 0.0F, 0.0F));

  QVector3D const z_axis = normalized_or(
      QVector3D::crossProduct(guard_right, blade_dir), QVector3D(0.0F, 0.0F, 1.0F));

  QMatrix4x4 pose;
  pose.setColumn(0, QVector4D(guard_right, 0.0F));
  pose.setColumn(1, QVector4D(blade_dir, 0.0F));
  pose.setColumn(2, QVector4D(z_axis, 0.0F));
  pose.setColumn(3, QVector4D(blade_dir * 0.05F, 1.0F));
  return pose;
}

auto baked_sword_blade_socket_matrix(const Render::GL::AttachmentFrame& grip,
                                     bool tip) -> QMatrix4x4 {
  Render::GL::SwordRenderConfig const config{};
  QVector3D const blade_axis_local(0.02F, 0.97F, 0.0F);
  QMatrix4x4 socket =
      attachment_frame_matrix(grip) * sword_local_pose(blade_axis_local);
  if (tip) {
    QVector3D const tip_origin =
        (socket * QVector4D(0.0F, config.sword_length, 0.0F, 1.0F)).toVector3D();
    socket.setColumn(3, QVector4D(tip_origin, 1.0F));
  }
  return socket;
}

auto spear_endpoint_distance(HumanoidSocketSpec::Kind kind,
                             const Render::GL::SpearRenderConfig& config) -> float {
  switch (kind) {
  case HumanoidSocketSpec::Kind::SpearShaftBase:
    return -0.28F;
  case HumanoidSocketSpec::Kind::SpearShaftTip:
    return config.spear_length;
  case HumanoidSocketSpec::Kind::SpearHeadTip:
    return config.spear_length + config.spearhead_length;
  case HumanoidSocketSpec::Kind::TopologySocket:
  case HumanoidSocketSpec::Kind::GripFrame:
  case HumanoidSocketSpec::Kind::SwordBladeBase:
  case HumanoidSocketSpec::Kind::SwordBladeTip:
    break;
  }
  return 0.0F;
}

auto baked_spear_socket_matrix(const Render::GL::AttachmentFrame& grip,
                               const Render::GL::AnimationInputs& inputs,
                               float attack_phase,
                               HumanoidSocketSpec::Kind kind) -> QMatrix4x4 {
  Render::GL::SpearRenderConfig const config{};
  QVector3D const spear_dir = Render::GL::resolve_spear_direction(inputs, attack_phase);

  QVector3D right =
      grip.right - spear_dir * QVector3D::dotProduct(grip.right, spear_dir);
  right = normalized_or(right, QVector3D(1.0F, 0.0F, 0.0F));
  QVector3D const forward = normalized_or(QVector3D::crossProduct(right, spear_dir),
                                          QVector3D(0.0F, 0.0F, 1.0F));

  QMatrix4x4 socket;
  socket.setColumn(0, QVector4D(right, 0.0F));
  socket.setColumn(1, QVector4D(spear_dir, 0.0F));
  socket.setColumn(2, QVector4D(forward, 0.0F));
  socket.setColumn(
      3,
      QVector4D(grip.origin + spear_dir * spear_endpoint_distance(kind, config), 1.0F));
  return socket;
}

auto transition_phase(std::uint32_t frame_index, std::uint32_t frame_count) -> float {
  if (frame_count <= 1U) {
    return 1.0F;
  }
  return std::clamp(static_cast<float>(frame_index) /
                        static_cast<float>(frame_count - 1U),
                    0.0F,
                    1.0F);
}

auto hold_gait_descriptor() -> Render::GL::HumanoidGaitDescriptor {
  Render::GL::HumanoidGaitDescriptor gait{};
  gait.state = Render::GL::HumanoidMotionState::Hold;
  gait.cycle_time = 1.8F;
  gait.cycle_phase = 0.0F;
  gait.speed = 0.0F;
  gait.normalized_speed = 0.0F;
  return gait;
}

auto rpg_sword_guard_key(float phase) -> AuthoredSwordPoseKey {
  return {.phase = phase,
          .right_hand = k_rpg_sword_guard_right_hand,
          .left_hand = k_rpg_sword_guard_left_hand,
          .blade_dir = k_rpg_sword_guard_blade_dir};
}

auto rpg_sword_pose_keys(std::uint8_t variant) -> const AuthoredSwordPoseKeys& {

  static const AuthoredSwordPoseKeys slash_left{{
      rpg_sword_guard_key(0.00F),
      {.phase = 0.16F,
       .right_hand = {0.50F, 1.46F, -0.40F},
       .left_hand = {-0.28F, 1.10F, 0.14F},
       .blade_dir = {0.46F, 0.74F, -0.49F},
       .pelvis_delta = {0.03F, -0.02F, -0.08F},
       .shoulder_r_delta = {0.10F, 0.03F, -0.16F},
       .shoulder_l_delta = {-0.06F, -0.01F, 0.08F},
       .neck_delta = {0.02F, 0.00F, -0.05F},
       .head_delta = {0.02F, 0.00F, -0.04F},
       .foot_r_delta = {0.00F, 0.00F, -0.06F},
       .knee_r_delta = {0.00F, 0.00F, -0.04F}},

      {.phase = 0.38F,
       .right_hand = {0.46F, 1.54F, -0.14F},
       .left_hand = {-0.24F, 1.08F, 0.22F},
       .blade_dir = {0.32F, 0.90F, -0.30F},
       .pelvis_delta = {0.03F, -0.04F, 0.00F},
       .shoulder_r_delta = {0.14F, 0.04F, -0.18F},
       .shoulder_l_delta = {-0.08F, -0.02F, 0.10F},
       .neck_delta = {0.02F, -0.01F, -0.03F},
       .head_delta = {0.02F, -0.01F, -0.02F},
       .foot_r_delta = {0.00F, 0.06F, 0.10F},
       .knee_r_delta = {0.00F, 0.05F, 0.10F}},
      {.phase = 0.54F,
       .right_hand = {-0.30F, 0.80F, 1.10F},
       .left_hand = {0.06F, 0.96F, 0.86F},
       .blade_dir = {-0.56F, -0.32F, 0.76F},
       .pelvis_delta = {0.02F, -0.13F, 0.34F},
       .shoulder_r_delta = {-0.16F, -0.18F, 0.44F},
       .shoulder_l_delta = {-0.14F, 0.02F, 0.10F},
       .neck_delta = {-0.02F, -0.10F, 0.24F},
       .head_delta = {-0.02F, -0.08F, 0.18F},
       .foot_r_delta = {0.00F, 0.00F, 0.44F},
       .knee_r_delta = {0.00F, 0.02F, 0.28F},
       .foot_l_delta = {0.00F, 0.00F, 0.02F},
       .knee_l_delta = {0.00F, -0.02F, 0.06F}},
      {.phase = 0.76F,
       .right_hand = {-0.44F, 0.68F, 0.92F},
       .left_hand = {-0.10F, 0.92F, 0.70F},
       .blade_dir = {-0.84F, -0.44F, 0.32F},
       .pelvis_delta = {-0.02F, -0.10F, 0.28F},
       .shoulder_r_delta = {-0.22F, -0.14F, 0.30F},
       .shoulder_l_delta = {-0.08F, 0.00F, 0.02F},
       .neck_delta = {-0.03F, -0.07F, 0.16F},
       .head_delta = {-0.03F, -0.05F, 0.12F},
       .foot_r_delta = {0.00F, 0.00F, 0.44F},
       .knee_r_delta = {0.00F, 0.02F, 0.28F},
       .knee_l_delta = {0.00F, -0.02F, 0.04F}},
      rpg_sword_guard_key(1.00F),
  }};
  static const AuthoredSwordPoseKeys slash_right{{
      rpg_sword_guard_key(0.00F),
      {.phase = 0.16F,
       .right_hand = {-0.32F, 1.44F, -0.36F},
       .left_hand = {-0.30F, 1.10F, 0.12F},
       .blade_dir = {-0.46F, 0.74F, -0.49F},
       .pelvis_delta = {-0.03F, -0.02F, -0.08F},
       .shoulder_r_delta = {-0.08F, 0.03F, 0.02F},
       .shoulder_l_delta = {0.10F, -0.02F, -0.16F},
       .neck_delta = {-0.02F, 0.00F, -0.05F},
       .head_delta = {-0.02F, 0.00F, -0.04F},
       .foot_l_delta = {0.00F, 0.00F, -0.06F},
       .knee_l_delta = {0.00F, 0.00F, -0.04F}},
      {.phase = 0.38F,
       .right_hand = {-0.26F, 1.52F, -0.12F},
       .left_hand = {-0.26F, 1.08F, 0.20F},
       .blade_dir = {-0.32F, 0.90F, -0.30F},
       .pelvis_delta = {-0.03F, -0.04F, 0.00F},
       .shoulder_r_delta = {-0.10F, 0.04F, 0.04F},
       .shoulder_l_delta = {0.14F, -0.02F, -0.18F},
       .neck_delta = {-0.02F, -0.01F, -0.03F},
       .head_delta = {-0.02F, -0.01F, -0.02F},
       .foot_l_delta = {0.00F, 0.06F, 0.10F},
       .knee_l_delta = {0.00F, 0.05F, 0.10F}},
      {.phase = 0.54F,
       .right_hand = {0.66F, 0.82F, 1.08F},
       .left_hand = {0.12F, 0.96F, 0.86F},
       .blade_dir = {0.56F, -0.32F, 0.76F},
       .pelvis_delta = {-0.02F, -0.13F, 0.34F},
       .shoulder_r_delta = {0.20F, -0.16F, 0.42F},
       .shoulder_l_delta = {0.14F, 0.02F, 0.12F},
       .neck_delta = {0.02F, -0.10F, 0.24F},
       .head_delta = {0.02F, -0.08F, 0.18F},
       .foot_r_delta = {0.00F, 0.00F, 0.02F},
       .knee_r_delta = {0.00F, -0.02F, 0.06F},
       .foot_l_delta = {0.00F, 0.00F, 0.44F},
       .knee_l_delta = {0.00F, 0.02F, 0.28F}},
      {.phase = 0.76F,
       .right_hand = {0.72F, 0.72F, 0.90F},
       .left_hand = {0.04F, 0.92F, 0.70F},
       .blade_dir = {0.84F, -0.44F, 0.32F},
       .pelvis_delta = {0.02F, -0.10F, 0.28F},
       .shoulder_r_delta = {0.26F, -0.12F, 0.28F},
       .shoulder_l_delta = {0.08F, 0.00F, 0.04F},
       .neck_delta = {0.03F, -0.07F, 0.16F},
       .head_delta = {0.03F, -0.05F, 0.12F},
       .knee_r_delta = {0.00F, -0.02F, 0.04F},
       .foot_l_delta = {0.00F, 0.00F, 0.44F},
       .knee_l_delta = {0.00F, 0.02F, 0.28F}},
      rpg_sword_guard_key(1.00F),
  }};
  static const AuthoredSwordPoseKeys overhead{{
      rpg_sword_guard_key(0.00F),
      {.phase = 0.18F,
       .right_hand = {0.20F, 1.62F, -0.44F},
       .left_hand = {-0.20F, 1.18F, 0.14F},
       .blade_dir = {0.10F, 0.97F, -0.22F},
       .pelvis_delta = {0.00F, -0.01F, -0.10F},
       .shoulder_r_delta = {0.06F, 0.10F, -0.14F},
       .shoulder_l_delta = {-0.04F, 0.08F, -0.08F},
       .neck_delta = {0.00F, 0.02F, -0.08F},
       .head_delta = {0.00F, 0.02F, -0.06F},
       .foot_r_delta = {0.00F, 0.00F, -0.06F},
       .knee_r_delta = {0.00F, 0.00F, -0.04F}},
      {.phase = 0.42F,
       .right_hand = {0.10F, 1.80F, -0.16F},
       .left_hand = {-0.10F, 1.24F, 0.26F},
       .blade_dir = {0.04F, 0.99F, -0.10F},
       .pelvis_delta = {0.00F, -0.03F, -0.02F},
       .shoulder_r_delta = {0.06F, 0.16F, -0.10F},
       .shoulder_l_delta = {-0.04F, 0.12F, -0.06F},
       .neck_delta = {0.00F, 0.04F, -0.06F},
       .head_delta = {0.00F, 0.04F, -0.04F},
       .foot_r_delta = {0.00F, 0.08F, 0.12F},
       .knee_r_delta = {0.00F, 0.06F, 0.12F}},
      {.phase = 0.60F,
       .right_hand = {0.02F, 0.70F, 1.46F},
       .left_hand = {-0.02F, 0.92F, 0.94F},
       .blade_dir = {-0.04F, -0.50F, 0.87F},
       .pelvis_delta = {0.00F, -0.18F, 0.38F},
       .shoulder_r_delta = {0.04F, -0.30F, 0.46F},
       .shoulder_l_delta = {-0.04F, -0.14F, 0.30F},
       .neck_delta = {0.00F, -0.18F, 0.32F},
       .head_delta = {0.00F, -0.14F, 0.24F},
       .foot_r_delta = {0.00F, 0.00F, 0.50F},
       .knee_r_delta = {0.00F, 0.02F, 0.32F},
       .knee_l_delta = {0.00F, -0.03F, 0.06F}},
      {.phase = 0.82F,
       .right_hand = {-0.02F, 0.56F, 0.84F},
       .left_hand = {-0.06F, 0.86F, 0.62F},
       .blade_dir = {-0.10F, -0.78F, 0.62F},
       .pelvis_delta = {0.00F, -0.14F, 0.30F},
       .shoulder_r_delta = {0.02F, -0.24F, 0.30F},
       .shoulder_l_delta = {-0.02F, -0.12F, 0.20F},
       .neck_delta = {0.00F, -0.14F, 0.22F},
       .head_delta = {0.00F, -0.10F, 0.16F},
       .foot_r_delta = {0.00F, 0.00F, 0.50F},
       .knee_r_delta = {0.00F, 0.02F, 0.32F},
       .knee_l_delta = {0.00F, -0.02F, 0.04F}},
      rpg_sword_guard_key(1.00F),
  }};
  static const AuthoredSwordPoseKeys thrust{{
      rpg_sword_guard_key(0.00F),
      {.phase = 0.14F,
       .right_hand = {0.32F, 1.16F, -0.42F},
       .left_hand = {-0.20F, 1.08F, 0.12F},
       .blade_dir = {0.22F, 0.66F, 0.72F},
       .pelvis_delta = {0.02F, -0.02F, -0.10F},
       .shoulder_r_delta = {0.06F, 0.00F, -0.16F},
       .shoulder_l_delta = {-0.04F, 0.00F, 0.02F},
       .neck_delta = {0.00F, 0.00F, -0.06F},
       .head_delta = {0.00F, 0.00F, -0.04F},
       .foot_r_delta = {0.00F, 0.00F, -0.06F},
       .knee_r_delta = {0.00F, 0.00F, -0.04F}},
      {.phase = 0.34F,
       .right_hand = {0.20F, 1.10F, 0.14F},
       .left_hand = {-0.06F, 1.06F, 0.30F},
       .blade_dir = {0.10F, 0.32F, 0.94F},
       .pelvis_delta = {0.02F, -0.04F, 0.02F},
       .shoulder_r_delta = {0.04F, -0.02F, -0.04F},
       .foot_r_delta = {0.00F, 0.06F, 0.12F},
       .knee_r_delta = {0.00F, 0.05F, 0.12F}},
      {.phase = 0.48F,
       .right_hand = {0.06F, 1.12F, 1.84F},
       .left_hand = {0.00F, 1.00F, 0.96F},
       .blade_dir = {0.03F, 0.08F, 1.00F},
       .pelvis_delta = {0.00F, -0.14F, 0.44F},
       .shoulder_r_delta = {0.00F, -0.10F, 0.56F},
       .shoulder_l_delta = {0.00F, -0.04F, 0.18F},
       .neck_delta = {0.00F, -0.08F, 0.26F},
       .head_delta = {0.00F, -0.06F, 0.18F},
       .foot_r_delta = {0.00F, 0.00F, 0.54F},
       .knee_r_delta = {0.00F, 0.02F, 0.34F},
       .foot_l_delta = {0.00F, 0.00F, -0.04F},
       .knee_l_delta = {0.00F, -0.03F, 0.04F}},
      {.phase = 0.72F,
       .right_hand = {0.08F, 1.00F, 1.16F},
       .left_hand = {-0.04F, 0.98F, 0.66F},
       .blade_dir = {0.06F, 0.28F, 0.96F},
       .pelvis_delta = {0.00F, -0.10F, 0.34F},
       .shoulder_r_delta = {0.00F, -0.06F, 0.36F},
       .shoulder_l_delta = {0.00F, -0.02F, 0.12F},
       .neck_delta = {0.00F, -0.05F, 0.18F},
       .head_delta = {0.00F, -0.04F, 0.12F},
       .foot_r_delta = {0.00F, 0.00F, 0.54F},
       .knee_r_delta = {0.00F, 0.02F, 0.34F},
       .knee_l_delta = {0.00F, -0.02F, 0.03F}},
      rpg_sword_guard_key(1.00F),
  }};
  static const AuthoredSwordPoseKeys finisher{{
      rpg_sword_guard_key(0.00F),
      {.phase = 0.18F,
       .right_hand = {0.30F, 1.70F, -0.56F},
       .left_hand = {-0.22F, 1.22F, 0.10F},
       .blade_dir = {0.14F, 0.96F, -0.24F},
       .pelvis_delta = {0.02F, -0.02F, -0.14F},
       .shoulder_r_delta = {0.08F, 0.12F, -0.20F},
       .shoulder_l_delta = {-0.04F, 0.08F, -0.10F},
       .neck_delta = {0.00F, 0.02F, -0.10F},
       .head_delta = {0.00F, 0.02F, -0.08F},
       .foot_r_delta = {0.00F, 0.00F, -0.08F},
       .knee_r_delta = {0.00F, 0.00F, -0.05F}},
      {.phase = 0.50F,
       .right_hand = {0.14F, 1.92F, -0.28F},
       .left_hand = {-0.08F, 1.30F, 0.24F},
       .blade_dir = {0.06F, 0.99F, -0.12F},
       .pelvis_delta = {0.02F, -0.05F, -0.04F},
       .shoulder_r_delta = {0.10F, 0.20F, -0.16F},
       .shoulder_l_delta = {-0.04F, 0.14F, -0.08F},
       .neck_delta = {0.00F, 0.06F, -0.08F},
       .head_delta = {0.00F, 0.06F, -0.06F},
       .foot_r_delta = {0.00F, 0.10F, 0.14F},
       .knee_r_delta = {0.00F, 0.08F, 0.14F}},
      {.phase = 0.66F,
       .right_hand = {-0.10F, 0.48F, 1.66F},
       .left_hand = {-0.02F, 0.80F, 1.04F},
       .blade_dir = {-0.06F, -0.62F, 0.78F},
       .pelvis_delta = {0.00F, -0.24F, 0.46F},
       .shoulder_r_delta = {0.02F, -0.40F, 0.56F},
       .shoulder_l_delta = {-0.06F, -0.20F, 0.36F},
       .neck_delta = {0.00F, -0.26F, 0.40F},
       .head_delta = {0.00F, -0.20F, 0.30F},
       .foot_r_delta = {0.00F, 0.00F, 0.60F},
       .knee_r_delta = {0.00F, 0.02F, 0.38F},
       .knee_l_delta = {0.00F, -0.04F, 0.08F}},
      {.phase = 0.86F,
       .right_hand = {-0.26F, 0.46F, 0.92F},
       .left_hand = {-0.08F, 0.78F, 0.66F},
       .blade_dir = {-0.14F, -0.86F, 0.48F},
       .pelvis_delta = {-0.02F, -0.18F, 0.36F},
       .shoulder_r_delta = {-0.06F, -0.32F, 0.38F},
       .shoulder_l_delta = {-0.04F, -0.16F, 0.24F},
       .neck_delta = {0.00F, -0.20F, 0.28F},
       .head_delta = {0.00F, -0.14F, 0.20F},
       .foot_r_delta = {0.00F, 0.00F, 0.60F},
       .knee_r_delta = {0.00F, 0.02F, 0.38F},
       .knee_l_delta = {0.00F, -0.03F, 0.06F}},
      rpg_sword_guard_key(1.00F),
  }};

  switch (variant) {
  case 1U:
    return slash_right;
  case 3U:
    return overhead;
  case 4U:
    return thrust;
  case 5U:
    return finisher;
  case 0U:
  default:
    return slash_left;
  }
}

auto slerp_dir(const QVector3D& from, const QVector3D& to, float t) -> QVector3D {
  if (from.lengthSquared() < 1.0e-8F || to.lengthSquared() < 1.0e-8F) {
    return blend_vec(from, to, t);
  }
  QVector3D const a = from.normalized();
  QVector3D const b = to.normalized();
  float const dot = std::clamp(QVector3D::dotProduct(a, b), -1.0F, 1.0F);
  if (dot > 0.9995F) {
    QVector3D const straight = blend_vec(a, b, t);
    return straight.lengthSquared() > 1.0e-8F ? straight.normalized() : a;
  }
  QVector3D ortho = b - (a * dot);
  if (ortho.lengthSquared() < 1.0e-8F) {

    ortho = QVector3D::crossProduct(a, QVector3D(0.0F, 1.0F, 0.0F));
    if (ortho.lengthSquared() < 1.0e-8F) {
      ortho = QVector3D::crossProduct(a, QVector3D(1.0F, 0.0F, 0.0F));
    }
  }
  ortho.normalize();
  float const theta = std::acos(dot) * std::clamp(t, 0.0F, 1.0F);
  return (a * std::cos(theta)) + (ortho * std::sin(theta));
}

auto sample_authored_sword_pose_key(const AuthoredSwordPoseKeys& keys,
                                    float phase) -> AuthoredSwordPoseKey {
  float const clamped = std::clamp(phase, 0.0F, 1.0F);
  std::size_t segment = 1U;
  while (segment + 1U < keys.size() && clamped > keys[segment].phase) {
    ++segment;
  }
  auto const& from = keys[segment - 1U];
  auto const& to = keys[segment];
  float const span = std::max(0.001F, to.phase - from.phase);
  float const t = std::clamp((clamped - from.phase) / span, 0.0F, 1.0F);

  using Channel = QVector3D AuthoredSwordPoseKey::*;
  auto tangent = [&keys](std::size_t index, Channel channel) -> QVector3D {
    if (index == 0U || index + 1U >= keys.size()) {
      return {};
    }
    float const window =
        std::max(0.001F, keys[index + 1U].phase - keys[index - 1U].phase);
    return (keys[index + 1U].*channel - keys[index - 1U].*channel) / window;
  };

  float const t2 = t * t;
  float const t3 = t2 * t;
  float const h00 = (2.0F * t3) - (3.0F * t2) + 1.0F;
  float const h10 = t3 - (2.0F * t2) + t;
  float const h01 = (-2.0F * t3) + (3.0F * t2);
  float const h11 = t3 - t2;
  auto hermite = [&](Channel channel) -> QVector3D {
    if (channel == &AuthoredSwordPoseKey::foot_r_delta ||
        channel == &AuthoredSwordPoseKey::foot_l_delta) {

      return {Animation::sample_pose_channel(
                  keys, clamped, [&](auto const& k) { return (k.*channel).x(); }),
              Animation::sample_pose_channel(
                  keys, clamped, [&](auto const& k) { return (k.*channel).y(); }),
              Animation::sample_pose_channel(
                  keys, clamped, [&](auto const& k) { return (k.*channel).z(); })};
    }
    return (from.*channel * h00) + (tangent(segment - 1U, channel) * span * h10) +
           (to.*channel * h01) + (tangent(segment, channel) * span * h11);
  };

  QVector3D blade = hermite(&AuthoredSwordPoseKey::blade_dir);
  blade = blade.lengthSquared() > 1.0e-6F ? blade.normalized()
                                          : slerp_dir(from.blade_dir, to.blade_dir, t);

  auto step_lift = [&](Channel channel) -> float {
    constexpr float epsilon = 0.0001F;
    auto speed_on_axis = [&](int axis) {
      auto value = [&](auto const& k) {
        return (k.*channel)[axis];
      };
      return (Animation::sample_pose_channel(keys, clamped + epsilon, value) -
              Animation::sample_pose_channel(keys, clamped - epsilon, value)) /
             (2.0F * epsilon);
    };

    constexpr float k_full_lift_speed = 1.20F;
    constexpr float k_max_lift = 0.10F;
    float const speed = std::hypot(speed_on_axis(0), speed_on_axis(2));
    return std::clamp(speed / k_full_lift_speed, 0.0F, 1.0F) * k_max_lift;
  };

  QVector3D foot_r = hermite(&AuthoredSwordPoseKey::foot_r_delta);
  QVector3D foot_l = hermite(&AuthoredSwordPoseKey::foot_l_delta);
  QVector3D knee_r = hermite(&AuthoredSwordPoseKey::knee_r_delta);
  QVector3D knee_l = hermite(&AuthoredSwordPoseKey::knee_l_delta);
  float const lift_r = step_lift(&AuthoredSwordPoseKey::foot_r_delta);
  float const lift_l = step_lift(&AuthoredSwordPoseKey::foot_l_delta);
  foot_r.setY(foot_r.y() + lift_r);
  foot_l.setY(foot_l.y() + lift_l);
  knee_r.setY(knee_r.y() + lift_r * 0.55F);
  knee_l.setY(knee_l.y() + lift_l * 0.55F);

  return {
      .phase = clamped,
      .right_hand = hermite(&AuthoredSwordPoseKey::right_hand),
      .left_hand = hermite(&AuthoredSwordPoseKey::left_hand),
      .blade_dir = blade,
      .pelvis_delta = hermite(&AuthoredSwordPoseKey::pelvis_delta),
      .shoulder_r_delta = hermite(&AuthoredSwordPoseKey::shoulder_r_delta),
      .shoulder_l_delta = hermite(&AuthoredSwordPoseKey::shoulder_l_delta),
      .neck_delta = hermite(&AuthoredSwordPoseKey::neck_delta),
      .head_delta = hermite(&AuthoredSwordPoseKey::head_delta),
      .foot_r_delta = foot_r,
      .knee_r_delta = knee_r,
      .foot_l_delta = foot_l,
      .knee_l_delta = knee_l,
  };
}

void apply_authored_rpg_sword_pose(Render::GL::HumanoidPoseController& ctrl,
                                   std::uint8_t variant,
                                   float phase,
                                   Render::GL::HumanoidPose& pose) {
  auto const sample =
      sample_authored_sword_pose_key(rpg_sword_pose_keys(variant), phase);

  pose.pelvis_pos += sample.pelvis_delta;
  pose.shoulder_r += sample.shoulder_r_delta;
  pose.shoulder_l += sample.shoulder_l_delta;
  pose.neck_base += sample.neck_delta;
  pose.head_pos += sample.head_delta;
  pose.foot_r += sample.foot_r_delta;
  pose.knee_r += sample.knee_r_delta;
  pose.foot_l += sample.foot_l_delta;
  pose.knee_l += sample.knee_l_delta;

  ctrl.place_hand_at(Render::GL::Side::Right, sample.right_hand);
  ctrl.place_hand_at(Render::GL::Side::Left, sample.left_hand);
  pose.grip_axis_r = Render::Humanoid::hand_axis_for_weapon_direction(
      sample.blade_dir, Render::GL::baked_sword_direction(), true);
}

void apply_authored_rpg_spear_pose(Render::GL::HumanoidPoseController& ctrl,
                                   std::uint8_t variant,
                                   float phase,
                                   Render::GL::HumanoidPose& pose) {
  auto const q = [](Animation::PoseVec3 v) {
    return QVector3D(v.x, v.y, v.z);
  };
  auto const sample = Animation::resolve_commander_spear_pose(
      commander_spear_move_for_variant(variant), phase);

  pose.pelvis_pos += q(sample.pelvis_delta);
  pose.shoulder_r += q(sample.shoulder_r_delta);
  pose.shoulder_l += q(sample.shoulder_l_delta);
  pose.neck_base += q(sample.neck_delta);
  pose.head_pos += q(sample.head_delta);
  pose.foot_r += q(sample.foot_r_delta);
  pose.knee_r += q(sample.knee_r_delta);
  pose.foot_l += q(sample.foot_l_delta);
  pose.knee_l += q(sample.knee_l_delta);

  ctrl.place_hand_at(Render::GL::Side::Right, q(sample.right_hand));
  ctrl.place_hand_at(Render::GL::Side::Left, q(sample.left_hand));
  pose.grip_axis_r = Render::Humanoid::hand_axis_for_weapon_direction(
      q(sample.spear_dir).normalized(),
      Render::GL::resolve_spear_direction(Render::GL::AnimationInputs{}),
      true);
}

} // namespace humanoid_manifest_detail

} // namespace Render::Humanoid
