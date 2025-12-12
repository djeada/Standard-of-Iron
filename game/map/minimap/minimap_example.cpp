/**
 * @file minimap_example.cpp
 * @brief Example demonstrating how to use the minimap generation system
 *
 * This example shows how to integrate the minimap generator with the map
 * loading system. In a real application, this would typically be done during
 * map initialization in the game bootstrap or level loader.
 */

#include "map/map_loader.h"
#include "map/minimap/minimap_generator.h"
#include "map/minimap/minimap_texture_manager.h"
#include <QCoreApplication>
#include <QDebug>

using namespace Game::Map;
using namespace Game::Map::Minimap;

auto main(int argc, char *argv[]) -> int {
  QCoreApplication app(argc, argv);

  // Step 1: Load a map from JSON
  qDebug() << "=== Minimap Generation Example ===";
  qDebug() << "";
  qDebug() << "Step 1: Loading map from JSON...";

  const QString mapPath = "assets/maps/map_rivers.json";
  MapDefinition mapDef;
  QString error;

  if (!MapLoader::loadFromJsonFile(mapPath, mapDef, &error)) {
    qCritical() << "Failed to load map:" << error;
    return 1;
  }

  qDebug() << "  ✓ Loaded map:" << mapDef.name;
  qDebug() << "  ✓ Grid size:" << mapDef.grid.width << "x" << mapDef.grid.height;
  qDebug() << "  ✓ Terrain features:" << mapDef.terrain.size();
  qDebug() << "  ✓ Rivers:" << mapDef.rivers.size();
  qDebug() << "  ✓ Roads:" << mapDef.roads.size();
  qDebug() << "  ✓ Spawns:" << mapDef.spawns.size();
  qDebug() << "";

  // Step 2: Generate minimap using the generator directly
  qDebug() << "Step 2: Generating minimap texture...";

  MinimapGenerator generator;
  QImage minimapImage = generator.generate(mapDef);

  if (minimapImage.isNull()) {
    qCritical() << "Failed to generate minimap image";
    return 1;
  }

  qDebug() << "  ✓ Generated minimap texture";
  qDebug() << "  ✓ Size:" << minimapImage.width() << "x" << minimapImage.height();
  qDebug() << "  ✓ Format:" << minimapImage.format();
  qDebug() << "";

  // Step 3: Optionally save the minimap for inspection
  const QString outputPath = "/tmp/minimap_example.png";
  if (minimapImage.save(outputPath)) {
    qDebug() << "  ✓ Saved minimap to:" << outputPath;
  } else {
    qWarning() << "  ⚠ Could not save minimap to:" << outputPath;
  }
  qDebug() << "";

  // Step 4: Demonstrate the texture manager (higher-level API)
  qDebug() << "Step 3: Using MinimapTextureManager (recommended approach)...";

  MinimapTextureManager manager;
  if (!manager.generateForMap(mapDef)) {
    qCritical() << "Failed to generate minimap via manager";
    return 1;
  }

  qDebug() << "  ✓ Minimap texture manager initialized";
  qDebug() << "  ✓ Texture ready for OpenGL upload";
  qDebug() << "";

  // Step 5: Show integration points
  qDebug() << "=== Integration Guide ===";
  qDebug() << "";
  qDebug() << "In your game initialization code:";
  qDebug() << "";
  qDebug() << "  1. Load your map JSON:";
  qDebug() << "     MapDefinition mapDef;";
  qDebug() << "     MapLoader::loadFromJsonFile(path, mapDef);";
  qDebug() << "";
  qDebug() << "  2. Generate the minimap:";
  qDebug() << "     MinimapTextureManager minimap;";
  qDebug() << "     minimap.generateForMap(mapDef);";
  qDebug() << "";
  qDebug() << "  3. Use the texture in your renderer:";
  qDebug() << "     auto* texture = minimap.getTexture();";
  qDebug() << "     // Bind and render in your UI";
  qDebug() << "";
  qDebug() << "=== Complete ===";

  return 0;
}
