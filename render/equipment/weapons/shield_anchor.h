#pragma once

#include <QMatrix4x4>
#include <QVector4D>

#include "../../gl/humanoid/humanoid_types.h"
#include "../../humanoid/skeleton.h"

namespace Render::GL {

[[nodiscard]] inline auto
attachment_frame_transform(const AttachmentFrame& frame) -> QMatrix4x4 {
  QMatrix4x4 transform;
  transform.setColumn(0, QVector4D(frame.right, 0.0F));
  transform.setColumn(1, QVector4D(frame.up, 0.0F));
  transform.setColumn(2, QVector4D(frame.forward, 0.0F));
  transform.setColumn(3, QVector4D(frame.origin, 1.0F));
  return transform;
}

[[nodiscard]] inline auto
resolve_left_shield_frame(const BodyFrames& frames) -> AttachmentFrame {
  if (frames.shield_l.radius > 0.0F) {
    return frames.shield_l;
  }
  if (frames.grip_l.radius > 0.0F) {
    return frames.grip_l;
  }
  if (frames.hand_l.radius <= 0.0F) {
    return Render::Humanoid::socket_attachment_frame(
        frames.hand_l, Render::Humanoid::HumanoidSocket::GripL);
  }
  return Render::Humanoid::socket_attachment_frame(
      frames.hand_l, Render::Humanoid::HumanoidSocket::GripL);
}

[[nodiscard]] inline auto bind_left_shield_pose_calibration() -> QMatrix4x4 {
  auto const& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  QMatrix4x4 const bind_shield = attachment_frame_transform(bind_frames.shield_l);
  QMatrix4x4 const bind_legacy = attachment_frame_transform(bind_frames.grip_l);
  bool invertible = false;
  QMatrix4x4 const bind_shield_inverse = bind_shield.inverted(&invertible);
  if (!invertible) {
    return QMatrix4x4{};
  }
  return bind_shield_inverse * bind_legacy;
}

} // namespace Render::GL
