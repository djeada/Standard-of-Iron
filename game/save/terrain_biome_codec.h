#pragma once

#include <QJsonObject>

#include "../map/biome_settings.h"

namespace Engine::Core::TerrainCodec {

void write_biome(const Game::Map::BiomeSettings& biome, QJsonObject& terrain_obj);
void read_biome(const QJsonObject& json, Game::Map::BiomeSettings& biome);

} // namespace Engine::Core::TerrainCodec
