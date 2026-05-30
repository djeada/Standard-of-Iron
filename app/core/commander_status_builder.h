#pragma once

#include <QString>
#include <QVariantMap>

#include <functional>

namespace Engine::Core {
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace App::Core {

struct CommanderStatusInput {
  const Engine::Core::World* world = nullptr;
  Engine::Core::EntityID controlled_commander_id = 0;
  bool dodge_active = false;
  Engine::Core::EntityID locked_target_id = 0;
  bool rally_placing = false;
  std::function<bool(
      Engine::Core::EntityID, QString&, int&, int&, bool&, bool&, QString&)>
      get_unit_info;
  std::function<bool(Engine::Core::EntityID, float&, bool&, bool&)>
      get_unit_stamina_info;
};

auto build_controlled_commander_status(const CommanderStatusInput& input)
    -> QVariantMap;

} // namespace App::Core
