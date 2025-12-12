#pragma once

#include "../map_definition.h"
#include <QImage>
#include <cstdint>
#include <memory>

namespace Game::Map::Minimap {

/**
 * @brief Generates static minimap textures from map JSON definitions.
 *
 * This class implements Stage I of the minimap system - generating a static
 * background texture that includes:
 * - Terrain colors based on biome settings
 * - Rivers as blue polylines
 * - Mountains/hills as shaded symbols
 * - Roads as paths
 * - Villages/structures as simplified icons
 *
 * The generated texture is meant to be uploaded to OpenGL once during map
 * load and never recalculated during gameplay.
 */
class MinimapGenerator {
public:
  /**
   * @brief Configuration for minimap generation
   */
  struct Config {
    int resolution_width;     // Width of minimap texture in pixels
    int resolution_height;    // Height of minimap texture in pixels
    float pixels_per_tile;    // How many pixels per grid tile

    Config()
        : resolution_width(256), resolution_height(256), pixels_per_tile(2.0F) {}
  };

  MinimapGenerator();
  explicit MinimapGenerator(const Config &config);

  /**
   * @brief Generates a minimap texture from a map definition
   * @param mapDef The map definition containing terrain, rivers, etc.
   * @return A QImage containing the generated minimap texture
   */
  [[nodiscard]] auto generate(const MapDefinition &mapDef) -> QImage;

private:
  Config m_config;

  // Helper methods for rendering different map elements
  void renderTerrainBase(QImage &image, const MapDefinition &mapDef);
  void renderTerrainFeatures(QImage &image, const MapDefinition &mapDef);
  void renderRivers(QImage &image, const MapDefinition &mapDef);
  void renderRoads(QImage &image, const MapDefinition &mapDef);
  void renderStructures(QImage &image, const MapDefinition &mapDef);

  // Coordinate conversion
  [[nodiscard]] auto worldToPixel(float world_x, float world_z,
                                  const GridDefinition &grid) const
      -> std::pair<int, int>;

  // Color utilities
  [[nodiscard]] static auto biomeToBaseColor(const BiomeSettings &biome)
      -> QColor;
  [[nodiscard]] static auto terrainFeatureColor(TerrainType type) -> QColor;
};

} // namespace Game::Map::Minimap
