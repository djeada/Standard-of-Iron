#pragma once

#include "../../humanoid/humanoid_renderer_base.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <cmath>
#include <string>

namespace Render::GL {

inline auto safe_attachment_axis(const QVector3D &axis,
                                 const QVector3D &fallback) -> QVector3D {
  return axis.lengthSquared() > 1e-6F ? axis.normalized() : fallback;
}

struct TorsoLocalFrame {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};
  QMatrix4x4 world{};

  [[nodiscard]] auto point(const QVector3D &world_point) const -> QVector3D {
    QVector3D const delta = world_point - origin;
    return {QVector3D::dotProduct(delta, right),
            QVector3D::dotProduct(delta, up),
            QVector3D::dotProduct(delta, forward)};
  }

  [[nodiscard]] auto direction(const QVector3D &world_dir) const -> QVector3D {
    return {QVector3D::dotProduct(world_dir, right),
            QVector3D::dotProduct(world_dir, up),
            QVector3D::dotProduct(world_dir, forward)};
  }
};

inline auto
make_torso_local_frame(const QMatrix4x4 &parent,
                       const AttachmentFrame &torso) -> TorsoLocalFrame {
  TorsoLocalFrame frame;
  frame.origin = torso.origin;
  frame.up = safe_attachment_axis(torso.up, QVector3D(0.0F, 1.0F, 0.0F));
  frame.right = safe_attachment_axis(torso.right, QVector3D(1.0F, 0.0F, 0.0F));
  frame.forward =
      safe_attachment_axis(torso.forward, QVector3D(0.0F, 0.0F, 1.0F));

  QMatrix4x4 local;
  local.setColumn(0, QVector4D(frame.right, 0.0F));
  local.setColumn(1, QVector4D(frame.up, 0.0F));
  local.setColumn(2, QVector4D(frame.forward, 0.0F));
  local.setColumn(3, QVector4D(frame.origin, 1.0F));
  frame.world = parent * local;
  return frame;
}

inline auto local_scale_model(const QVector3D &local_pos,
                              const QVector3D &radii) -> QMatrix4x4 {
  QMatrix4x4 m;
  m.translate(local_pos);
  m.scale(radii.x(), radii.y(), radii.z());
  return m;
}

inline void append_quantized_key(std::string &out, float value) {
  out += std::to_string(std::lround(value * 1000.0F));
  out.push_back('_');
}

inline void append_quantized_key(std::string &out, int value) {
  out += std::to_string(value);
  out.push_back('_');
}

inline void append_quantized_key(std::string &out, const QVector3D &value) {
  append_quantized_key(out, value.x());
  append_quantized_key(out, value.y());
  append_quantized_key(out, value.z());
}

inline void append_quantized_key(std::string &out, const QMatrix4x4 &value) {
  const float *data = value.constData();
  for (int i = 0; i < 16; ++i) {
    append_quantized_key(out, data[i]);
  }
}

} // namespace Render::GL
