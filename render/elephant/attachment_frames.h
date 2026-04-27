#pragma once

#include "elephant_gait.h"

#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

struct ElephantAttachmentFrame {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};

  auto make_local_transform(const QMatrix4x4 &parent,
                            const QVector3D &local_offset,
                            float uniform_scale) const -> QMatrix4x4 {
    QMatrix4x4 m = parent;
    QVector3D const world_pos = origin + right * local_offset.x() +
                                up * local_offset.y() +
                                forward * local_offset.z();
    m.translate(world_pos);
    QMatrix4x4 basis;
    basis.setColumn(0, QVector4D(right * uniform_scale, 0.0F));
    basis.setColumn(1, QVector4D(up * uniform_scale, 0.0F));
    basis.setColumn(2, QVector4D(forward * uniform_scale, 0.0F));
    basis.setColumn(3, QVector4D(0.0F, 0.0F, 0.0F, 1.0F));
    return m * basis;
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

struct ElephantMotionSample {
  ElephantGait gait{};
  HowdahAttachmentFrame howdah{};
  QVector3D barrel_center{0.0F, 0.0F, 0.0F};
  QVector3D chest_center{0.0F, 0.0F, 0.0F};
  QVector3D rump_center{0.0F, 0.0F, 0.0F};
  QVector3D neck_base{0.0F, 0.0F, 0.0F};
  QVector3D neck_top{0.0F, 0.0F, 0.0F};
  QVector3D head_center{0.0F, 0.0F, 0.0F};
  float phase = 0.0F;
  float bob = 0.0F;
  bool is_moving = false;
  bool is_fighting = false;
  float body_sway = 0.0F;
  float trunk_swing = 0.0F;
  float ear_flap = 0.0F;
};

enum class LegIndex : int {
  FrontLeft = 0,
  FrontRight = 1,
  RearLeft = 2,
  RearRight = 3
};

struct ElephantLegState {
  QVector3D planted_foot{0.0F, 0.0F, 0.0F};
  QVector3D swing_start{0.0F, 0.0F, 0.0F};
  QVector3D swing_target{0.0F, 0.0F, 0.0F};
  float swing_progress = 0.0F;
  bool in_swing = false;
};

struct ElephantGaitState {
  ElephantLegState legs[4]{};
  float cycle_phase = 0.0F;
  float weight_shift_x = 0.0F;
  float weight_shift_z = 0.0F;
  float shoulder_lag = 0.0F;
  float hip_lag = 0.0F;
  bool initialized = false;
};

struct ElephantLegPose {
  QVector3D hip;
  QVector3D knee;
  QVector3D ankle;
  QVector3D foot;
  float upper_radius;
  float lower_radius;
};

} // namespace Render::GL
