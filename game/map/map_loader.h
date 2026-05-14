#pragma once

#include <QString>

#include "map_definition.h"

namespace Game::Map {

class MapLoader {
public:
  static auto load_from_json_file(const QString& path,
                                  MapDefinition& out_map,
                                  QString* out_error = nullptr) -> bool;
};

} // namespace Game::Map
