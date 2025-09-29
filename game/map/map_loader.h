#pragma once

#include "map_definition.h"
#include <QString>

namespace Game::Map {

class MapLoader {
public:
    // Load MapDefinition from a JSON file. Returns true on success.
    static bool loadFromJsonFile(const QString& path, MapDefinition& outMap, QString* outError = nullptr);
};

} // namespace Game::Map
