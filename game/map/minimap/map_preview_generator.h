#pragma once

#include "../map_definition.h"
#include <QColor>
#include <QImage>
#include <QString>
#include <QVariantList>
#include <memory>

namespace Game::Map::Minimap {

class MinimapGenerator;

class MapPreviewGenerator {
public:
  struct PlayerConfig {
    int player_id = 0;
    QColor color;
  };

  MapPreviewGenerator();
  ~MapPreviewGenerator();

  [[nodiscard]] auto generate_preview(const QString &map_path,
                                      const QVariantList &player_configs)
      -> QImage;

private:
  std::unique_ptr<MinimapGenerator> m_minimap_generator;

  void draw_player_bases(QImage &image, const MapDefinition &map_def,
                         const std::vector<PlayerConfig> &player_configs);
  [[nodiscard]] auto parse_player_configs(const QVariantList &configs) const
      -> std::vector<PlayerConfig>;
  [[nodiscard]] auto
  world_to_pixel(float world_x, float world_z, const GridDefinition &grid,
                 float pixels_per_tile) const -> std::pair<float, float>;
};

} // namespace Game::Map::Minimap
