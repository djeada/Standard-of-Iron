#pragma once

#include "../../systems/owner_registry.h"
#include <QImage>
#include <array>
#include <cstdint>
#include <vector>

class QPainter;

namespace Game::Map::Minimap {

struct UnitMarker {
  float world_x = 0.0F;
  float world_z = 0.0F;
  int owner_id = 0;
  bool is_selected = false;
  bool is_building = false;
  bool is_visible = true;  // For fog-of-war filtering (hide enemies if false)
  bool is_firecamp = false; // Filter out fire camps from minimap
};

struct TeamColors {
  struct ColorSet {
    std::uint8_t r, g, b;
    std::uint8_t border_r, border_g, border_b;
  };

  static constexpr ColorSet NEUTRAL = {100, 95, 85, 50, 48, 43};

  static constexpr std::uint8_t SELECT_R = 255;
  static constexpr std::uint8_t SELECT_G = 215;
  static constexpr std::uint8_t SELECT_B = 0;

  static auto get_color(int owner_id) -> ColorSet {
    auto &registry = Game::Systems::OwnerRegistry::instance();
    std::array<float, 3> color = registry.get_owner_color(owner_id);

    // Convert float [0,1] to uint8_t [0,255]
    auto r = static_cast<std::uint8_t>(color[0] * 255.0F);
    auto g = static_cast<std::uint8_t>(color[1] * 255.0F);
    auto b = static_cast<std::uint8_t>(color[2] * 255.0F);

    // Generate darker border color (50% brightness)
    auto border_r = static_cast<std::uint8_t>(color[0] * 127.5F);
    auto border_g = static_cast<std::uint8_t>(color[1] * 127.5F);
    auto border_b = static_cast<std::uint8_t>(color[2] * 127.5F);

    // Check if this is a neutral or unknown owner
    if (owner_id <= 0) {
      return NEUTRAL;
    }

    return ColorSet{r, g, b, border_r, border_g, border_b};
  }
};

class UnitLayer {
public:
  UnitLayer() = default;

  void init(int width, int height, float world_width, float world_height);

  [[nodiscard]] auto is_initialized() const -> bool {
    return !m_image.isNull();
  }

  void update(const std::vector<UnitMarker> &markers);

  [[nodiscard]] auto get_image() const -> const QImage & { return m_image; }

  void set_unit_radius(float radius) { m_unit_radius = radius; }

  void set_building_size(float size) { m_building_half_size = size; }

  // Set camera yaw for dynamic orientation (replaces hardcoded 225 degrees)
  void set_camera_yaw(float yaw_deg);

private:
  [[nodiscard]] auto
  world_to_pixel(float world_x, float world_z) const -> std::pair<float, float>;

  void draw_unit_marker(QPainter &painter, float px, float py,
                        const TeamColors::ColorSet &colors, bool is_selected);

  void draw_building_marker(QPainter &painter, float px, float py,
                            const TeamColors::ColorSet &colors,
                            bool is_selected);

  QImage m_image;
  int m_width = 0;
  int m_height = 0;
  float m_world_width = 0.0F;
  float m_world_height = 0.0F;
  float m_unit_radius = 3.0F;
  float m_building_half_size = 5.0F;

  float m_scale_x = 1.0F;
  float m_scale_y = 1.0F;
  float m_offset_x = 0.0F;
  float m_offset_y = 0.0F;

  float m_camera_yaw_deg = 225.0F;
  float m_cos_yaw = -0.70710678118F;
  float m_sin_yaw = -0.70710678118F;
};

} // namespace Game::Map::Minimap
