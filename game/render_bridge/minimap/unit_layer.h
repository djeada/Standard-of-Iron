#pragma once

#include <QImage>
#include <QRect>

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

class QPainter;

namespace Game::Map::Minimap {

enum class MarkerClass : std::uint8_t {
  Troop = 0,
  MinorStructure = 1,
  Tower = 2,
  Landmark = 3,
  Stronghold = 4,
};

inline constexpr std::uint8_t k_capture_steps = 12;

struct UnitMarker {
  float world_x = 0.0F;
  float world_z = 0.0F;
  int owner_id = 0;
  bool is_selected = false;
  MarkerClass marker_class = MarkerClass::Troop;
  std::uint8_t capture_step = 0;
  int capture_owner_id = 0;
  bool contested = false;
};

using VisibilityCheckFn = std::function<bool(float world_x, float world_z)>;

using PlayerColorFn = std::function<bool(
    int owner_id, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b)>;

struct TeamColors {
  struct ColorSet {
    std::uint8_t r, g, b;
    std::uint8_t border_r, border_g, border_b;
  };

  static constexpr ColorSet PLAYER_1 = {46, 74, 140, 17, 26, 54};

  static constexpr ColorSet PLAYER_2 = {156, 46, 34, 56, 17, 12};

  static constexpr ColorSet PLAYER_3 = {58, 112, 70, 20, 42, 26};

  static constexpr ColorSet PLAYER_4 = {176, 138, 46, 70, 53, 16};

  static constexpr ColorSet PLAYER_5 = {112, 52, 116, 41, 18, 43};

  static constexpr ColorSet PLAYER_6 = {46, 122, 124, 16, 45, 46};

  static constexpr ColorSet NEUTRAL = {170, 154, 124, 52, 43, 32};

  static constexpr std::uint8_t SELECT_R = 226;
  static constexpr std::uint8_t SELECT_G = 190;
  static constexpr std::uint8_t SELECT_B = 108;

  static constexpr std::uint8_t CONTESTED_R = 206;
  static constexpr std::uint8_t CONTESTED_G = 138;
  static constexpr std::uint8_t CONTESTED_B = 46;

  static constexpr std::uint8_t INK_R = 44;
  static constexpr std::uint8_t INK_G = 34;
  static constexpr std::uint8_t INK_B = 24;

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

  void init(int width,
            int height,
            float world_width,
            float world_height,
            float tile_size = 1.0F);

  [[nodiscard]] auto is_initialized() const -> bool { return !m_image.isNull(); }

  void update(const std::vector<UnitMarker>& markers);

  void update(const std::vector<UnitMarker>& markers,
              int local_owner_id,
              const VisibilityCheckFn& visibility_check,
              const PlayerColorFn& player_color_fn = nullptr);

  [[nodiscard]] auto get_image() const -> const QImage& { return m_image; }

  [[nodiscard]] auto content_rect() const -> const QRect& { return m_content_rect; }

  void set_unit_radius(float radius) { m_unit_radius = radius; }

  void set_building_size(float size) { m_building_half_size = size; }

  [[nodiscard]] auto world_to_pixel(float world_x,
                                    float world_z) const -> std::pair<float, float>;

private:
  struct PlacedMarker {
    float px = 0.0F;
    float py = 0.0F;
    int owner_id = 0;
    const UnitMarker* marker = nullptr;
  };

  [[nodiscard]] auto
  get_color_for_owner(int owner_id,
                      const PlayerColorFn& player_color_fn) -> TeamColors::ColorSet;

  void draw_troops(QPainter& painter, const PlayerColorFn& player_color_fn);
  void draw_minor_structures(QPainter& painter, const PlayerColorFn& player_color_fn);
  void draw_structures(QPainter& painter, const PlayerColorFn& player_color_fn);
  void draw_strongholds(QPainter& painter, const PlayerColorFn& player_color_fn);
  void draw_selected(QPainter& painter, const PlayerColorFn& player_color_fn);

  void draw_stronghold_shape(QPainter& painter,
                             float px,
                             float py,
                             const TeamColors::ColorSet& colors,
                             bool neutral);
  void draw_keep_outline(QPainter& painter, float px, float py, float half) const;
  void draw_temple_shape(QPainter& painter, float px, float py) const;
  void draw_capture_ring(QPainter& painter,
                         const PlacedMarker& placed,
                         const PlayerColorFn& player_color_fn);
  void draw_tower_shape(QPainter& painter, float px, float py) const;
  void draw_selection_halo(QPainter& painter, const PlacedMarker& placed);

  [[nodiscard]] auto stronghold_half_size() const -> float;

  QImage m_image;
  int m_width = 0;
  int m_height = 0;
  float m_world_width = 0.0F;
  float m_world_height = 0.0F;
  float m_inv_tile_size = 1.0F;
  float m_unit_radius = 3.0F;
  float m_building_half_size = 4.25F;

  float m_scale_x = 1.0F;
  float m_scale_y = 1.0F;
  float m_offset_x = 0.0F;
  float m_offset_y = 0.0F;

  QRect m_content_rect;

  std::vector<PlacedMarker> m_minor_structures;
  std::vector<PlacedMarker> m_structures;
  std::vector<PlacedMarker> m_strongholds;
  std::vector<PlacedMarker> m_troops;
  std::vector<PlacedMarker> m_selected;
};

} // namespace Game::Map::Minimap
