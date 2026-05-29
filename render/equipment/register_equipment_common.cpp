#include "register_equipment_internal.h"

namespace Render::GL::EquipmentRegistration {

auto humanoid_head_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
}

auto humanoid_chest_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
}

auto humanoid_pelvis_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Pelvis);
}

auto humanoid_foot_l_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::FootL);
}

auto humanoid_foot_r_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::FootR);
}

auto humanoid_hip_l_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HipL);
}

auto humanoid_shoulder_l_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderL);
}

auto humanoid_shoulder_r_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderR);
}

auto humanoid_forearm_l_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ForearmL);
}

auto humanoid_forearm_r_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ForearmR);
}

auto humanoid_head_bind_matrix() -> const QMatrix4x4& {
  static const QMatrix4x4 matrix =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          Render::Humanoid::HumanoidBone::Head)];
  return matrix;
}

auto humanoid_shoulder_bind_matrix(bool left) -> const QMatrix4x4& {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  static const QMatrix4x4 left_matrix =
      Render::GL::make_shoulder_cover_transform(QMatrix4x4{},
                                                bind_frames.shoulder_l.origin,
                                                -bind_frames.torso.right,
                                                bind_frames.torso.up);
  static const QMatrix4x4 right_matrix =
      Render::GL::make_shoulder_cover_transform(QMatrix4x4{},
                                                bind_frames.shoulder_r.origin,
                                                bind_frames.torso.right,
                                                bind_frames.torso.up);
  return left ? left_matrix : right_matrix;
}

auto humanoid_shin_bind_matrix(bool left) -> const QMatrix4x4& {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  static const auto frame_to_matrix = [](const Render::GL::AttachmentFrame& f) {
    QMatrix4x4 matrix;
    matrix.setColumn(0, QVector4D(f.right, 0.0F));
    matrix.setColumn(1, QVector4D(f.up, 0.0F));
    matrix.setColumn(2, QVector4D(f.forward, 0.0F));
    matrix.setColumn(3, QVector4D(f.origin, 1.0F));
    return matrix;
  };
  static const QMatrix4x4 left_matrix = frame_to_matrix(bind_frames.shin_l);
  static const QMatrix4x4 right_matrix = frame_to_matrix(bind_frames.shin_r);
  return left ? left_matrix : right_matrix;
}

auto horse_root_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Horse::HorseBone::Root);
}

auto horse_head_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Horse::HorseBone::Head);
}

auto horse_neck_top_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Horse::HorseBone::NeckTop);
}

auto horse_root_bind_matrix() -> const QMatrix4x4& {
  static const QMatrix4x4 matrix =
      Render::Horse::horse_bind_palette()[static_cast<std::size_t>(
          Render::Horse::HorseBone::Root)];
  return matrix;
}

auto horse_head_bind_matrix() -> const QMatrix4x4& {
  static const QMatrix4x4 matrix =
      Render::Horse::horse_bind_palette()[static_cast<std::size_t>(
          Render::Horse::HorseBone::Head)];
  return matrix;
}

auto horse_neck_top_bind_matrix() -> const QMatrix4x4& {
  static const QMatrix4x4 matrix =
      Render::Horse::horse_bind_palette()[static_cast<std::size_t>(
          Render::Horse::HorseBone::NeckTop)];
  return matrix;
}

} // namespace Render::GL::EquipmentRegistration
