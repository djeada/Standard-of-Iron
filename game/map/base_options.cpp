#include "base_options.h"

#include <variant>

#include "../units/spawn_type.h"

namespace Game::Map {

auto structure_key(const StructureEntry& entry, std::size_t index) -> QString {
  const QString authored = entry.id.trimmed();
  if (!authored.isEmpty()) {
    return authored;
  }
  return QStringLiteral("structure_%1").arg(index);
}

auto collect_base_options(const MapDefinition& def) -> std::vector<BaseOption> {
  std::vector<BaseOption> options;
  options.reserve(def.structures.size());

  for (std::size_t index = 0; index < def.structures.size(); ++index) {
    const auto& entry = def.structures[index];
    if (entry.type != Game::Units::SpawnType::Barracks) {
      continue;
    }
    const auto* point = std::get_if<PointStructureGeometry>(&entry.geometry);
    if (point == nullptr) {
      continue;
    }
    if (entry.player_id < 0) {
      continue;
    }

    options.push_back(BaseOption{.key = structure_key(entry, index),
                                 .default_player_id = entry.player_id,
                                 .max_population = entry.max_population,
                                 .position = point->position,
                                 .structure_index = index});
  }

  return options;
}

auto default_base_assignments(const MapDefinition& def)
    -> std::unordered_map<int, QString> {
  std::unordered_map<int, QString> assignments;
  for (const auto& option : collect_base_options(def)) {
    if (option.default_player_id <= 0) {
      continue;
    }
    assignments.emplace(option.default_player_id, option.key);
  }
  return assignments;
}

} // namespace Game::Map
