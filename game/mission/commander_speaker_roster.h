#pragma once

#include <QString>

#include <vector>

#include "game/map/mission_definition.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {
class OwnerRegistry;
}

namespace Game::Mission {

struct CommanderSpeaker {
  int owner_id = 0;
  QString troop_type;
  CommanderRelationship relationship = CommanderRelationship::Enemy;
};

[[nodiscard]] auto
build_commander_speaker_roster(Engine::Core::World& world,
                               const Game::Systems::OwnerRegistry& owners,
                               int local_owner_id) -> std::vector<CommanderSpeaker>;

} // namespace Game::Mission
