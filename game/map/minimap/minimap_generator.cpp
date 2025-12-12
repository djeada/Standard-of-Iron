#include "minimap_generator.h"
#include <QColor>
#include <QPainter>
#include <QPen>
#include <cmath>

namespace Game::Map::Minimap {

MinimapGenerator::MinimapGenerator() : m_config() {}

MinimapGenerator::MinimapGenerator(const Config &config) : m_config(config) {}

auto MinimapGenerator::generate(const MapDefinition &mapDef) -> QImage {
  // Calculate actual resolution based on map size
  const int width = static_cast<int>(mapDef.grid.width * m_config.pixels_per_tile);
  const int height = static_cast<int>(mapDef.grid.height * m_config.pixels_per_tile);

  // Create image with appropriate format
  QImage image(width, height, QImage::Format_RGBA8888);
  image.fill(Qt::transparent);

  // Render layers in order (back to front)
  renderTerrainBase(image, mapDef);
  renderRoads(image, mapDef);
  renderRivers(image, mapDef);
  renderTerrainFeatures(image, mapDef);
  renderStructures(image, mapDef);

  return image;
}

void MinimapGenerator::renderTerrainBase(QImage &image,
                                         const MapDefinition &mapDef) {
  // Fill with base terrain color from biome
  const QColor baseColor = biomeToBaseColor(mapDef.biome);
  image.fill(baseColor);
}

void MinimapGenerator::renderTerrainFeatures(QImage &image,
                                             const MapDefinition &mapDef) {
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto &feature : mapDef.terrain) {
    const QColor color = terrainFeatureColor(feature.type);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);

    const auto [px, pz] = worldToPixel(feature.center_x, feature.center_z, mapDef.grid);

    // Calculate pixel dimensions
    const float pixel_width = feature.width * m_config.pixels_per_tile;
    const float pixel_depth = feature.depth * m_config.pixels_per_tile;

    // Draw as ellipse
    painter.drawEllipse(QPointF(px, pz), pixel_width / 2.0F, pixel_depth / 2.0F);
  }
}

void MinimapGenerator::renderRivers(QImage &image,
                                    const MapDefinition &mapDef) {
  if (mapDef.rivers.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  // River color - light blue
  const QColor riverColor(100, 150, 255, 200);

  for (const auto &river : mapDef.rivers) {
    const auto [x1, z1] = worldToPixel(river.start.x(), river.start.z(), mapDef.grid);
    const auto [x2, z2] = worldToPixel(river.end.x(), river.end.z(), mapDef.grid);

    const float pixel_width = river.width * m_config.pixels_per_tile * 0.5F;

    QPen pen(riverColor);
    pen.setWidthF(pixel_width);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    painter.drawLine(QPointF(x1, z1), QPointF(x2, z2));
  }
}

void MinimapGenerator::renderRoads(QImage &image,
                                   const MapDefinition &mapDef) {
  if (mapDef.roads.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  // Road color - brownish
  const QColor roadColor(139, 119, 101, 180);

  for (const auto &road : mapDef.roads) {
    const auto [x1, z1] = worldToPixel(road.start.x(), road.start.z(), mapDef.grid);
    const auto [x2, z2] = worldToPixel(road.end.x(), road.end.z(), mapDef.grid);

    const float pixel_width = road.width * m_config.pixels_per_tile * 0.4F;

    QPen pen(roadColor);
    pen.setWidthF(pixel_width);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    painter.drawLine(QPointF(x1, z1), QPointF(x2, z2));
  }
}

void MinimapGenerator::renderStructures(QImage &image,
                                        const MapDefinition &mapDef) {
  if (mapDef.spawns.empty()) {
    return;
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const auto &spawn : mapDef.spawns) {
    // Only render structure spawns (barracks, etc.)
    if (!Game::Units::isBuildingSpawn(spawn.type)) {
      continue;
    }

    const auto [px, pz] = worldToPixel(spawn.x, spawn.z, mapDef.grid);

    // Draw structure as a small square
    const float size = 3.0F * m_config.pixels_per_tile;
    
    // Use team color or neutral
    QColor structureColor(200, 200, 200, 255);
    if (spawn.player_id > 0) {
      // Use different colors for different players
      if (spawn.player_id == 1) {
        structureColor = QColor(100, 150, 255, 255); // Blue
      } else if (spawn.player_id == 2) {
        structureColor = QColor(255, 100, 100, 255); // Red
      }
    }

    painter.setBrush(structureColor);
    painter.setPen(QPen(Qt::black, 0.5F));

    const QRectF rect(px - size / 2.0F, pz - size / 2.0F, size, size);
    painter.drawRect(rect);
  }
}

auto MinimapGenerator::worldToPixel(float world_x, float world_z,
                                    const GridDefinition &grid) const
    -> std::pair<int, int> {
  // Convert world coordinates to pixel coordinates
  const int px = static_cast<int>(world_x * m_config.pixels_per_tile);
  const int pz = static_cast<int>(world_z * m_config.pixels_per_tile);
  return {px, pz};
}

auto MinimapGenerator::biomeToBaseColor(const BiomeSettings &biome) -> QColor {
  // Use grass_primary as the base terrain color
  const auto &grass = biome.grass_primary;
  return QColor::fromRgbF(grass.x(), grass.y(), grass.z());
}

auto MinimapGenerator::terrainFeatureColor(TerrainType type) -> QColor {
  switch (type) {
  case TerrainType::Mountain:
    // Dark gray/brown for mountains
    return QColor(120, 110, 100, 200);
  case TerrainType::Hill:
    // Lighter brown for hills
    return QColor(150, 140, 120, 150);
  case TerrainType::River:
    // Blue for rivers (though rivers are handled separately)
    return QColor(100, 150, 255, 200);
  case TerrainType::Flat:
  default:
    // Slightly darker grass for flat terrain
    return QColor(80, 120, 75, 100);
  }
}

} // namespace Game::Map::Minimap
