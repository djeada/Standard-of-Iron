#pragma once

#include "../map_definition.h"
#include <QImage>
#include <cstdint>
#include <memory>
#include <utility>

class QPainter;

namespace Game::Map::Minimap {

class MinimapGenerator {
public:
  struct Config {
    float pixels_per_tile = 2.0F;

    Config() = default;
  };

  MinimapGenerator();
  explicit MinimapGenerator(const Config &config);

  [[nodiscard]] auto generate(const MapDefinition &map_def) -> QImage;

private:
  Config m_config;

  void render_parchment_background(QImage &image);
  void render_terrain_base(QImage &image, const MapDefinition &map_def);
  void render_terrain_features(QImage &image, const MapDefinition &map_def);
  void render_rivers(QImage &image, const MapDefinition &map_def);
  void render_roads(QImage &image, const MapDefinition &map_def);
  void render_bridges(QImage &image, const MapDefinition &map_def);
  void render_structures(QImage &image, const MapDefinition &map_def);
  void apply_historical_styling(QImage &image);

  static void draw_mountain_symbol(QPainter &painter, float cx, float cy,
                                   float width, float height);
  static void draw_hill_symbol(QPainter &painter, float cx, float cy,
                               float width, float height);
  static void draw_river_segment(QPainter &painter, float x1, float y1,
                                 float x2, float y2, float width);
  static void draw_road_segment(QPainter &painter, float x1, float y1, float x2,
                                float y2, float width);
  static void draw_fortress_icon(QPainter &painter, float cx, float cy,
                                 const QColor &fill, const QColor &border);

  static void draw_map_border(QPainter &painter, int width, int height);
  static void apply_vignette(QPainter &painter, int width, int height);
  static void draw_compass_rose(QPainter &painter, int width, int height);

  [[nodiscard]] auto
  world_to_pixel(float world_x, float world_z,
                 const GridDefinition &grid) const -> std::pair<float, float>;

  [[nodiscard]] auto
  world_to_pixel_size(float world_size,
                      const GridDefinition &grid) const -> float;

  [[nodiscard]] static auto
  biome_to_base_color(const BiomeSettings &biome) -> QColor;
  [[nodiscard]] static auto terrain_feature_color(TerrainType type) -> QColor;
};

} 
