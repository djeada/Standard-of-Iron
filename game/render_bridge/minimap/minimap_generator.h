#pragma once

#include <QImage>

#include <cstdint>
#include <memory>
#include <utility>

#include "../../map/map_definition.h"
#include "minimap_projection.h"

class QPainter;

namespace Game::Map::Minimap {

class MinimapGenerator {
public:
  enum class StructureBake : std::uint8_t {
    All,
    LandmarksOnly
  };

  struct Config {
    float pixels_per_tile = 2.0F;
    int max_image_dimension = 512;
    StructureBake structure_bake = StructureBake::All;

    Config() = default;
  };

  MinimapGenerator();
  explicit MinimapGenerator(const Config& config);

  [[nodiscard]] auto generate(const MapDefinition& map_def) -> QImage;

private:
  Config m_config;

  [[nodiscard]] auto pixels_per_tile_for(const GridDefinition& grid) const -> float;

  void render_roads(QImage& image,
                    const MapDefinition& map_def,
                    const MinimapProjection& projection);
  void render_bridges(QImage& image,
                      const MapDefinition& map_def,
                      const MinimapProjection& projection);
  void render_undead_zones(QImage& image,
                           const MapDefinition& map_def,
                           const MinimapProjection& projection);
  void render_world_props(QImage& image,
                          const MapDefinition& map_def,
                          const MinimapProjection& projection);
  void render_structures(QImage& image,
                         const MapDefinition& map_def,
                         const MinimapProjection& projection);

  static void draw_road_segment(
      QPainter& painter, float x1, float y1, float x2, float y2, float width);
  static void draw_fortress_icon(QPainter& painter,
                                 float cx,
                                 float cy,
                                 float size,
                                 const QColor& fill,
                                 const QColor& border);

  [[nodiscard]] static auto terrain_feature_color(TerrainType type) -> QColor;
};

} // namespace Game::Map::Minimap
