#pragma once

#include <QImage>
#include <QVector2D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Game::Map::Minimap {

enum class VisibilityState : uint8_t { Unseen = 0, Revealed = 1, Visible = 2 };

struct FogOfWarConfig {

  int update_interval = 15;

  int resolution_divisor = 2;

  int blur_radius = 2;

  uint8_t alpha_unseen = 220;
  uint8_t alpha_revealed = 120;
  uint8_t alpha_visible = 0;

  uint8_t fog_color_r = 30;
  uint8_t fog_color_g = 25;
  uint8_t fog_color_b = 20;

  FogOfWarConfig() = default;
};

struct VisionSource {
  float world_x = 0.0F;
  float world_z = 0.0F;
  float vision_radius = 10.0F;
  int player_id = 0;
};

class FogOfWarMask {
public:
  FogOfWarMask(int map_width, int map_height, float tile_size,
               const FogOfWarConfig &config = FogOfWarConfig());

  ~FogOfWarMask();

  FogOfWarMask(const FogOfWarMask &) = delete;
  auto operator=(const FogOfWarMask &) -> FogOfWarMask & = delete;
  FogOfWarMask(FogOfWarMask &&) noexcept;
  auto operator=(FogOfWarMask &&) noexcept -> FogOfWarMask &;

  void update_vision(const std::vector<VisionSource> &sources, int player_id);

  auto tick(const std::vector<VisionSource> &sources, int player_id) -> bool;

  [[nodiscard]] auto generate_mask(int target_width,
                                   int target_height) const -> QImage;

  [[nodiscard]] auto get_visibility(int fog_x,
                                    int fog_y) const -> VisibilityState;

  [[nodiscard]] auto is_revealed(float world_x, float world_z) const -> bool;

  [[nodiscard]] auto is_visible(float world_x, float world_z) const -> bool;

  void reset();

  void reveal_all();

  [[nodiscard]] auto fog_width() const -> int { return m_fog_width; }
  [[nodiscard]] auto fog_height() const -> int { return m_fog_height; }

  [[nodiscard]] auto memory_usage() const -> size_t;

  [[nodiscard]] auto is_dirty() const -> bool { return m_dirty; }

  void clear_dirty() { m_dirty = false; }

private:
  void set_cell(int fog_x, int fog_y, VisibilityState state);
  [[nodiscard]] auto get_cell(int fog_x, int fog_y) const -> VisibilityState;

  [[nodiscard]] auto world_to_fog(float world_x,
                                  float world_z) const -> std::pair<int, int>;

  void reveal_circle(int center_x, int center_y, float radius_cells);
  void clear_current_visibility();

  void apply_gaussian_blur(std::vector<uint8_t> &alpha_buffer, int width,
                           int height) const;

  FogOfWarConfig m_config;

  int m_map_width;
  int m_map_height;
  float m_tile_size;

  int m_fog_width;
  int m_fog_height;
  float m_fog_cell_size;

  std::vector<uint8_t> m_visibility_data;

  int m_frame_counter = 0;

  bool m_dirty = true;

  mutable QImage m_cached_mask;
  mutable int m_cached_width = 0;
  mutable int m_cached_height = 0;
};

} // namespace Game::Map::Minimap
