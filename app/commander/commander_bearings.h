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

// One mark on the bearing strip across the top of the commander's view.
struct CommanderBearing {
  enum class Kind : std::uint8_t {
    EnemyCommander,
    // The nearest enemy troop that can be seen.
    Enemy,
    // The middle of the player's own troops.
    Army,
    RallyFlag
  };

  Kind kind{Kind::Enemy};
  // Degrees round from the centre of the view, positive to the player's
  // right, in (-180, 180].
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
  // Null means nothing is hidden (no fog of war in this match).
  const Game::Map::VisibilityService* visibility = nullptr;
};

// Where the things a commander on foot has to find are, as bearings off his
// view. From the ground the battle is a wall of backs: the army view's map
// knowledge - where the enemy general is, where the nearest fight is, where
// his own line stands - is what this carries down to him. It reveals nothing
// the map would not: enemies count only on ground the player can see.
[[nodiscard]] auto collect_commander_bearings(const CommanderBearingsInput& input)
    -> std::vector<CommanderBearing>;

[[nodiscard]] auto commander_bearing_kind_name(CommanderBearing::Kind kind)
    -> const char*;

} // namespace App::Core
