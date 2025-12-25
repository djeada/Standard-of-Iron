#pragma once

#include "map_definition.h"
#include <QString>

namespace Game::Map {

class MapLoader {
public:
  static auto loadFromJsonFile(const QString &path, MapDefinition &outMap,
                               QString *out_error = nullptr) -> bool;
};

} // namespace Game::Map
