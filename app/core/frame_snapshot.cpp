#include "app/core/frame_snapshot.h"

#include <QVector4D>
#include <QtGlobal>

#include <cmath>

namespace App::Core {
namespace {

constexpr float k_clip_w_epsilon = 1e-6F;
constexpr double k_ndc_offset = 1.0;
constexpr double k_ndc_half = 0.5;

[[nodiscard]] auto finite(const QVector3D& value) -> bool {
  return qIsFinite(value.x()) && qIsFinite(value.y()) && qIsFinite(value.z());
}

} // namespace

auto CameraProjection::project(const QVector3D& world,
                               QPointF& out_screen) const -> bool {
  if (viewport_width <= 0 || viewport_height <= 0) {
    return false;
  }
  if (!finite(world)) {
    return false;
  }

  const QVector4D clip = view_projection * QVector4D(world, 1.0F);
  if (std::abs(clip.w()) < k_clip_w_epsilon) {
    return false;
  }

  const QVector3D ndc = (clip / clip.w()).toVector3D();
  if (!qIsFinite(ndc.x()) || !qIsFinite(ndc.y()) || !qIsFinite(ndc.z())) {
    return false;
  }
  if (ndc.z() < -1.0F || ndc.z() > 1.0F) {
    return false;
  }

  const qreal sx =
      (ndc.x() * k_ndc_half + k_ndc_half) * static_cast<qreal>(viewport_width);
  const qreal sy = (k_ndc_offset - (ndc.y() * k_ndc_half + k_ndc_half)) *
                   static_cast<qreal>(viewport_height);
  out_screen = QPointF(sx, sy);
  return qIsFinite(sx) && qIsFinite(sy);
}

} // namespace App::Core
