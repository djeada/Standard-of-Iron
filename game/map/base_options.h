#pragma once

#include <QString>
#include <QVector3D>

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "map_definition.h"

namespace Game::Map {

struct BaseOption {
  QString key;
  int default_player_id = 0;
  int max_population = 60;
  QVector3D position;
  std::size_t structure_index = 0;
};

[[nodiscard]] auto structure_key(const StructureEntry& entry,
                                 std::size_t index) -> QString;

[[nodiscard]] auto
collect_base_options(const MapDefinition& def) -> std::vector<BaseOption>;

[[nodiscard]] auto
default_base_assignments(const MapDefinition& def) -> std::unordered_map<int, QString>;

} // namespace Game::Map
