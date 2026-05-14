

#include <QCoreApplication>
#include <QDebug>

#include "map/map_loader.h"
#include "map/minimap/minimap_generator.h"
#include "map/minimap/minimap_texture_manager.h"

using namespace Game::Map;
using namespace Game::Map::Minimap;

auto main(int argc, char* argv[]) -> int {
  QCoreApplication app(argc, argv);

  qDebug() << "=== Minimap Generation Example ===";
  qDebug() << "";
  qDebug() << "Step 1: Loading map from JSON...";

  const QString map_path = "assets/maps/map_rivers.json";
  MapDefinition map_def;
  QString error;

  if (!MapLoader::load_from_json_file(map_path, map_def, &error)) {
    qCritical() << "Failed to load map:" << error;
    return 1;
  }

  qDebug() << "  ✓ Loaded map:" << map_def.name;
  qDebug() << "  ✓ Grid size:" << map_def.grid.width << "x" << map_def.grid.height;
  qDebug() << "  ✓ Terrain features:" << map_def.terrain.size();
  qDebug() << "  ✓ Rivers:" << map_def.rivers.size();
  qDebug() << "  ✓ Roads:" << map_def.roads.size();
  qDebug() << "  ✓ Spawns:" << map_def.spawns.size();
  qDebug() << "";

  qDebug() << "Step 2: Generating minimap texture...";

  MinimapGenerator generator;
  QImage minimap_image = generator.generate(map_def);

  if (minimap_image.isNull()) {
    qCritical() << "Failed to generate minimap image";
    return 1;
  }

  qDebug() << "  ✓ Generated minimap texture";
  qDebug() << "  ✓ Size:" << minimap_image.width() << "x" << minimap_image.height();
  qDebug() << "  ✓ Format:" << minimap_image.format();
  qDebug() << "";

  const QString output_path = "/tmp/minimap_example.png";
  if (minimap_image.save(output_path)) {
    qDebug() << "  ✓ Saved minimap to:" << output_path;
  } else {
    qWarning() << "  ⚠ Could not save minimap to:" << output_path;
  }
  qDebug() << "";

  qDebug() << "Step 4: Using MinimapTextureManager (recommended approach)...";

  MinimapTextureManager manager;
  if (!manager.generate_for_map(map_def)) {
    qCritical() << "Failed to generate minimap via manager";
    return 1;
  }

  qDebug() << "  ✓ Minimap texture manager initialized";
  qDebug() << "  ✓ Texture ready for OpenGL upload";
  qDebug() << "";

  qDebug() << "=== Integration Guide ===";
  qDebug() << "";
  qDebug() << "In your game initialization code:";
  qDebug() << "";
  qDebug() << "  1. Load your map JSON:";
  qDebug() << "     MapDefinition map_def;";
  qDebug() << "     MapLoader::load_from_json_file(path, map_def);";
  qDebug() << "";
  qDebug() << "  2. Generate the minimap:";
  qDebug() << "     MinimapTextureManager minimap;";
  qDebug() << "     minimap.generateForMap(map_def);";
  qDebug() << "";
  qDebug() << "  3. Use the texture in your renderer:";
  qDebug() << "     auto* texture = minimap.getTexture();";
  qDebug() << "     // Bind and render in your UI";
  qDebug() << "";
  qDebug() << "=== Complete ===";

  return 0;
}
