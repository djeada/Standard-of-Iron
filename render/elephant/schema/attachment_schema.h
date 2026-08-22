#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include "animation/rig/instance_transform.h"

namespace Render::GL {

struct ElephantAttachmentFrame {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};

  auto make_local_transform(const QMatrix4x4& parent,
                            const QVector3D& local_offset,
                            float uniform_scale) const -> QMatrix4x4 {
    return make_basis_attachment_transform(
        parent, origin, right, up, forward, local_offset, uniform_scale);
  }
};

struct ElephantBodyFrames {
  ElephantAttachmentFrame head{};
  ElephantAttachmentFrame neck_base{};
  ElephantAttachmentFrame back_center{};
  ElephantAttachmentFrame howdah{};
  ElephantAttachmentFrame rump{};
  ElephantAttachmentFrame tail_base{};
  ElephantAttachmentFrame trunk_base{};
  ElephantAttachmentFrame trunk_tip{};
  ElephantAttachmentFrame ear_left{};
  ElephantAttachmentFrame ear_right{};
};

struct HowdahAttachmentFrame {
  QVector3D howdah_center;
  QVector3D seat_position;
  QVector3D seat_forward;
  QVector3D seat_right;
  QVector3D seat_up;
  QVector3D ground_offset;
};

} // namespace Render::GL
