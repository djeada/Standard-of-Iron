#pragma once

#include <QImage>

#include "../../map/map_definition.h"
#include "minimap_projection.h"

namespace Game::Map::Minimap {

void paint_water_layer(QImage& image,
                       const MapDefinition& map_def,
                       const MinimapProjection& projection);

} // namespace Game::Map::Minimap
