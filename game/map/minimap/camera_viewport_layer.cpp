#include "camera_viewport_layer.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

#include "minimap_utils.h"

namespace Game::Map::Minimap {

namespace {
constexpr float k_corner_size_ratio = 0.15F;
constexpr float k_min_corner_size = 4.0F;
constexpr float k_halo_pen_offset = 1.75F;
} // namespace

void CameraViewportLayer::init(int width,
                               int height,
                               float world_width,
                               float world_height) {
  m_width = width;
  m_height = height;
  m_world_width = world_width;
  m_world_height = world_height;

  m_scale_x = static_cast<float>(width - 1) / world_width;
  m_scale_y = static_cast<float>(height - 1) / world_height;
  m_offset_x = world_width * 0.5F;
  m_offset_y = world_height * 0.5F;

  m_image = QImage(width, height, QImage::Format_ARGB32);
  m_image.fill(Qt::transparent);
}

auto CameraViewportLayer::world_to_pixel(float world_x, float world_z) const
    -> std::pair<float, float> {

  const auto& orient = MinimapOrientation::instance();
  const float rotated_x = world_x * orient.cos_yaw() - world_z * orient.sin_yaw();
  const float rotated_z = world_x * orient.sin_yaw() + world_z * orient.cos_yaw();

  const float px = (rotated_x + m_offset_x) * m_scale_x;
  const float py = (rotated_z + m_offset_y) * m_scale_y;

  return {px, py};
}

void CameraViewportLayer::update(float camera_x,
                                 float camera_z,
                                 float viewport_width,
                                 float viewport_height) {
  if (m_image.isNull()) {
    return;
  }

  m_image.fill(Qt::transparent);

  if (viewport_width <= 0.0F || viewport_height <= 0.0F) {
    return;
  }

  QPainter painter(&m_image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const auto [px, py] = world_to_pixel(camera_x, camera_z);

  const float pixel_width = viewport_width * m_scale_x;
  const float pixel_height = viewport_height * m_scale_y;

  draw_viewport_rect(painter, px, py, pixel_width, pixel_height);
}

void CameraViewportLayer::draw_viewport_rect(
    QPainter& painter, float px, float py, float pixel_width, float pixel_height) {

  const float half_w = pixel_width * 0.5F;
  const float half_h = pixel_height * 0.5F;

  QRectF rect(static_cast<qreal>(px - half_w),
              static_cast<qreal>(py - half_h),
              static_cast<qreal>(pixel_width),
              static_cast<qreal>(pixel_height));

  const float corner_size = std::min(pixel_width, pixel_height) * k_corner_size_ratio;
  const float actual_corner =
      std::min(std::max(corner_size, k_min_corner_size),
               std::min(pixel_width, pixel_height) * 0.45F);

  QPainterPath brackets;
  brackets.moveTo(rect.left() + actual_corner, rect.top());
  brackets.lineTo(rect.left(), rect.top());
  brackets.lineTo(rect.left(), rect.top() + actual_corner);
  brackets.moveTo(rect.right() - actual_corner, rect.top());
  brackets.lineTo(rect.right(), rect.top());
  brackets.lineTo(rect.right(), rect.top() + actual_corner);
  brackets.moveTo(rect.left() + actual_corner, rect.bottom());
  brackets.lineTo(rect.left(), rect.bottom());
  brackets.lineTo(rect.left(), rect.bottom() - actual_corner);
  brackets.moveTo(rect.right() - actual_corner, rect.bottom());
  brackets.lineTo(rect.right(), rect.bottom());
  brackets.lineTo(rect.right(), rect.bottom() - actual_corner);

  QPen halo(QColor(25, 20, 15, 135));
  halo.setWidthF(static_cast<qreal>(m_border_width + k_halo_pen_offset));
  halo.setJoinStyle(Qt::MiterJoin);
  painter.setPen(halo);
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(brackets);

  QPen border(QColor(m_border_r, m_border_g, m_border_b, m_border_a));
  border.setWidthF(static_cast<qreal>(m_border_width));
  border.setJoinStyle(Qt::MiterJoin);
  painter.setPen(border);
  painter.drawPath(brackets);
}

} // namespace Game::Map::Minimap
