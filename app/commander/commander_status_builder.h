#pragma once

#include <QString>
#include <QVariantMap>

#include <cstdint>

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Core {

struct CommanderStatusInput {
  const Engine::Core::World* world = nullptr;
  Engine::Core::EntityID controlled_commander_id = 0;
  bool dodge_active = false;
  Engine::Core::EntityID locked_target_id = 0;
  bool rally_placing = false;
};

auto build_controlled_commander_status(const CommanderStatusInput& input)
    -> QVariantMap;

} // namespace App::Core
