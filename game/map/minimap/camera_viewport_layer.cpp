#include "camera_viewport_layer.h"
#include "minimap_utils.h"

#include <QPainter>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace Game::Map::Minimap {

namespace {
constexpr float k_corner_size_ratio = 0.15F;
constexpr float k_min_corner_size = 4.0F;
constexpr float k_corner_pen_offset = 1.0F;
} 

void CameraViewportLayer::init(int width, int height, float world_width,
                               float world_height) {
  m_width = width;
  m_height = height;
  m_world_width = world_width;
  m_world_height = world_height;

  m_scale_x = static_cast<float>(width - 1) / world_width;
  m_scale_y = static_cast<float>(height - 1) / world_height;
  m_offset_x = world_width * 0.5F;
  m_offset_y = world_height * 0.5F;

  m_image = QImage(width, height, QImage::Format_RGBA8888);
  m_image.fill(Qt::transparent);
}

auto CameraViewportLayer::world_to_pixel(float world_x, float world_z) const
    -> std::pair<float, float> {

  const float rotated_x = world_x * Constants::k_camera_yaw_cos -
                          world_z * Constants::k_camera_yaw_sin;
  const float rotated_z = world_x * Constants::k_camera_yaw_sin +
                          world_z * Constants::k_camera_yaw_cos;

  const float px = (rotated_x + m_offset_x) * m_scale_x;
  const float py = (rotated_z + m_offset_y) * m_scale_y;

  return {px, py};
}

void CameraViewportLayer::update(float camera_x, float camera_z,
                                 float viewport_width, float viewport_height) {
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

void CameraViewportLayer::draw_viewport_rect(QPainter &painter, float px,
                                             float py, float pixel_width,
                                             float pixel_height) {

  const float half_w = pixel_width * 0.5F;
  const float half_h = pixel_height * 0.5F;

  QRectF rect(static_cast<qreal>(px - half_w), static_cast<qreal>(py - half_h),
              static_cast<qreal>(pixel_width),
              static_cast<qreal>(pixel_height));

  QColor border_color(m_border_r, m_border_g, m_border_b, m_border_a);
  QPen pen(border_color);
  pen.setWidthF(static_cast<qreal>(m_border_width));
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect);

  const float corner_size =
      std::min(pixel_width, pixel_height) * k_corner_size_ratio;
  const float actual_corner = std::max(corner_size, k_min_corner_size);

  QColor corner_color(m_border_r, m_border_g, m_border_b, 255);
  QPen corner_pen(corner_color);
  corner_pen.setWidthF(static_cast<qreal>(m_border_width) +
                       k_corner_pen_offset);
  painter.setPen(corner_pen);

  painter.drawLine(QPointF(rect.left(), rect.top()),
                   QPointF(rect.left() + actual_corner, rect.top()));
  painter.drawLine(QPointF(rect.left(), rect.top()),
                   QPointF(rect.left(), rect.top() + actual_corner));

  painter.drawLine(QPointF(rect.right(), rect.top()),
                   QPointF(rect.right() - actual_corner, rect.top()));
  painter.drawLine(QPointF(rect.right(), rect.top()),
                   QPointF(rect.right(), rect.top() + actual_corner));

  painter.drawLine(QPointF(rect.left(), rect.bottom()),
                   QPointF(rect.left() + actual_corner, rect.bottom()));
  painter.drawLine(QPointF(rect.left(), rect.bottom()),
                   QPointF(rect.left(), rect.bottom() - actual_corner));

  painter.drawLine(QPointF(rect.right(), rect.bottom()),
                   QPointF(rect.right() - actual_corner, rect.bottom()));
  painter.drawLine(QPointF(rect.right(), rect.bottom()),
                   QPointF(rect.right(), rect.bottom() - actual_corner));
}

} 
