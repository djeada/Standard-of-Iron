#pragma once

#include <QImage>
#include <QRectF>
#include <cstdint>

class QPainter;

namespace Game::Map::Minimap {

class CameraViewportLayer {
public:
  CameraViewportLayer() = default;

  void init(int width, int height, float world_width, float world_height);

  [[nodiscard]] auto is_initialized() const -> bool {
    return !m_image.isNull();
  }

  void update(float camera_x, float camera_z, float viewport_width,
              float viewport_height);

  [[nodiscard]] auto get_image() const -> const QImage & { return m_image; }

  void set_border_width(float width) { m_border_width = width; }
  void set_border_color(std::uint8_t r, std::uint8_t g, std::uint8_t b,
                        std::uint8_t a = 200) {
    m_border_r = r;
    m_border_g = g;
    m_border_b = b;
    m_border_a = a;
  }

  // Set camera yaw for dynamic orientation (replaces hardcoded 225 degrees)
  void set_camera_yaw(float yaw_deg);

private:
  [[nodiscard]] auto
  world_to_pixel(float world_x, float world_z) const -> std::pair<float, float>;

  void draw_viewport_rect(QPainter &painter, float px, float py,
                          float pixel_width, float pixel_height);

  QImage m_image;
  int m_width = 0;
  int m_height = 0;
  float m_world_width = 0.0F;
  float m_world_height = 0.0F;

  float m_scale_x = 1.0F;
  float m_scale_y = 1.0F;
  float m_offset_x = 0.0F;
  float m_offset_y = 0.0F;

  float m_border_width = 2.0F;
  std::uint8_t m_border_r = 255;
  std::uint8_t m_border_g = 255;
  std::uint8_t m_border_b = 255;
  std::uint8_t m_border_a = 200;

  float m_camera_yaw_deg = 225.0F;
  float m_cos_yaw = -0.70710678118F;
  float m_sin_yaw = -0.70710678118F;
};

} // namespace Game::Map::Minimap
