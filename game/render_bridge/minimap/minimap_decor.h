#pragma once

#include <QImage>

#include "../../map/map_definition.h"
#include "minimap_projection.h"

namespace Game::Map::Minimap {

void paint_parchment_background(QImage& image);
void paint_terrain_base(QImage& image, const MapDefinition& map_def);
void paint_terrain_features(QImage& image,
                            const MapDefinition& map_def,
                            const MinimapProjection& projection);
void paint_historical_styling(QImage& image);

} // namespace Game::Map::Minimap
