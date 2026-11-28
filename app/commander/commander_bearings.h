#pragma once

#include <QString>

#include <cstdint>
#include <vector>

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Map {
class VisibilityService;
}

namespace Game::Systems {
class OwnerRegistry;
}

namespace App::Core {

struct CommanderBearing {
  enum class Kind : std::uint8_t {
    EnemyCommander,

    Enemy,

    Army,
    RallyFlag,
    Convoy
  };

  Kind kind{Kind::Enemy};

  float right_degrees{0.0F};
  float distance{0.0F};
  QString label;
};

struct CommanderBearingsInput {
  const Engine::Core::World* world = nullptr;
  Engine::Core::EntityID commander_id = 0;
  int local_owner_id = 0;
  float view_yaw_degrees = 0.0F;
  const Game::Systems::OwnerRegistry* owners = nullptr;

  const Game::Map::VisibilityService* visibility = nullptr;
};

[[nodiscard]] auto collect_commander_bearings(const CommanderBearingsInput& input)
    -> std::vector<CommanderBearing>;

[[nodiscard]] auto commander_bearing_kind_name(CommanderBearing::Kind kind) -> const
    char*;

} // namespace App::Core
