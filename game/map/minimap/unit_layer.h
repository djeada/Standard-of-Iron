#pragma once

#include <QImage>
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
};

struct TeamColors {
  struct ColorSet {
    std::uint8_t r, g, b;
    std::uint8_t border_r, border_g, border_b;
  };

  static constexpr ColorSet PLAYER_1 = {70, 100, 160, 35, 50, 80};

  static constexpr ColorSet PLAYER_2 = {180, 60, 50, 90, 30, 25};

  static constexpr ColorSet PLAYER_3 = {60, 130, 70, 30, 65, 35};

  static constexpr ColorSet PLAYER_4 = {190, 160, 60, 95, 80, 30};

  static constexpr ColorSet PLAYER_5 = {120, 60, 140, 60, 30, 70};

  static constexpr ColorSet PLAYER_6 = {60, 140, 140, 30, 70, 70};

  static constexpr ColorSet NEUTRAL = {100, 95, 85, 50, 48, 43};

  static constexpr std::uint8_t SELECT_R = 255;
  static constexpr std::uint8_t SELECT_G = 215;
  static constexpr std::uint8_t SELECT_B = 0;

  static constexpr auto get_color(int owner_id) -> ColorSet {
    switch (owner_id) {
    case 1:
      return PLAYER_1;
    case 2:
      return PLAYER_2;
    case 3:
      return PLAYER_3;
    case 4:
      return PLAYER_4;
    case 5:
      return PLAYER_5;
    case 6:
      return PLAYER_6;
    default:
      return NEUTRAL;
    }
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
};

} 
