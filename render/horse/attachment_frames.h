#pragma once

#include "horse_gait.h"

#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

struct HorseAttachmentFrame {
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

struct HorseBodyFrames {
  HorseAttachmentFrame head{};
  HorseAttachmentFrame neck_base{};
  HorseAttachmentFrame withers{};
  HorseAttachmentFrame back_center{};
  HorseAttachmentFrame croup{};
  HorseAttachmentFrame chest{};
  HorseAttachmentFrame barrel{};
  HorseAttachmentFrame rump{};
  HorseAttachmentFrame tail_base{};
  HorseAttachmentFrame muzzle{};
};

struct MountedAttachmentFrame {
  QVector3D saddle_center;
  QVector3D seat_position;
  QVector3D seat_forward;
  QVector3D seat_right;
  QVector3D seat_up;

  QVector3D stirrup_attach_left;
  QVector3D stirrup_attach_right;
  QVector3D stirrup_bottom_left;
  QVector3D stirrup_bottom_right;

  QVector3D rein_bit_left;
  QVector3D rein_bit_right;
  QVector3D bridle_base;
  QVector3D ground_offset;
  auto stirrup_attach(bool is_left) const -> const QVector3D &;
  auto stirrup_bottom(bool is_left) const -> const QVector3D &;
};

struct ReinState {
  float slack = 0.0F;
  float tension = 0.0F;
};

struct HorseMotionSample {
  HorseGait gait{};
  GaitType gait_type{GaitType::IDLE};
  float phase = 0.0F;
  float bob = 0.0F;
  bool is_moving = false;
  float rider_intensity = 0.0F;
  float turn_amount = 0.0F;
  float stop_intent = 0.0F;
  float body_sway = 0.0F;
  float body_pitch = 0.0F;
  float head_nod = 0.0F;
  float head_lateral = 0.0F;
  float spine_flex = 0.0F;
};

} // namespace Render::GL
