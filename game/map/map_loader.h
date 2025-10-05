#pragma once

#include "map_definition.h"
#include <QString>

namespace Game::Map {

class MapLoader {
public:
  static bool loadFromJsonFile(const QString &path, MapDefinition &outMap,
                               QString *outError = nullptr);
};

} // namespace Game::Map
