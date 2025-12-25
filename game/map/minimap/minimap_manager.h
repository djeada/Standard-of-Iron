#pragma once

#include "fog_of_war_mask.h"
#include "minimap_generator.h"
#include <QImage>
#include <memory>
#include <vector>

namespace Game::Map::Minimap {

struct MinimapManagerConfig {
  MinimapGenerator::Config generator_config;
  FogOfWarConfig fog_config;

  bool fog_enabled = true;

  bool fog_multiply_blend = true;

  MinimapManagerConfig() = default;
};

class MinimapManager {
public:
  MinimapManager(const MapDefinition &map_def,
                 const MinimapManagerConfig &config = MinimapManagerConfig());

  ~MinimapManager();

  MinimapManager(const MinimapManager &) = delete;
  auto operator=(const MinimapManager &) -> MinimapManager & = delete;
  MinimapManager(MinimapManager &&) noexcept;
  auto operator=(MinimapManager &&) noexcept -> MinimapManager &;

  void tick(const std::vector<VisionSource> &vision_sources, int player_id);

  void force_fog_update(const std::vector<VisionSource> &vision_sources,
                        int player_id);

  [[nodiscard]] auto get_base_image() const -> const QImage &;

  [[nodiscard]] auto get_fog_mask() const -> QImage;

  [[nodiscard]] auto get_composite_image() const -> QImage;

  [[nodiscard]] auto get_composite_image(int width, int height) const -> QImage;

  [[nodiscard]] auto is_position_visible(float world_x,
                                         float world_z) const -> bool;

  [[nodiscard]] auto is_position_revealed(float world_x,
                                          float world_z) const -> bool;

  void reset_fog();

  void reveal_all();

  void set_fog_enabled(bool enabled);

  [[nodiscard]] auto is_fog_enabled() const -> bool;

  [[nodiscard]] auto memory_usage() const -> size_t;

  void regenerate_base(const MapDefinition &map_def);

  [[nodiscard]] auto grid() const -> const GridDefinition & { return m_grid; }

private:
  void regenerate_composite() const;

  MinimapManagerConfig m_config;
  GridDefinition m_grid;

  std::unique_ptr<MinimapGenerator> m_generator;
  QImage m_base_image;

  std::unique_ptr<FogOfWarMask> m_fog_mask;

  mutable QImage m_composite_image;
  mutable bool m_composite_dirty = true;
};

} 
