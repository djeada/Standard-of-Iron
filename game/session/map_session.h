#pragma once

#include <QJsonObject>

#include <functional>

#include "session_snapshot.h"

namespace Engine::Core {
class World;
}

namespace Game::Map {
struct MapDefinition;
}

namespace Game::Systems {
class VictoryService;
}

namespace Game::Session {

void configure_map_systems(Engine::Core::World& world,
                           const Game::Map::MapDefinition& map_definition,
                           Game::Systems::VictoryService* victory_service);

struct MapSessionRestore {
  Engine::Core::World* world = nullptr;
  const Game::Map::MapDefinition* map = nullptr;
  Game::Systems::VictoryService* victory_service = nullptr;
  std::function<void()> configure_victory_rules;
  QJsonObject snapshot;
};

auto restore_map_session(const MapSessionRestore& restore) -> SnapshotRestoreReport;

} // namespace Game::Session
