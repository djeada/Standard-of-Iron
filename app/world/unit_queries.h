#pragma once

#include <QString>

#include "game/core/entity.h"
#include "game/systems/unit_activity.h"

namespace Engine::Core {
class World;
}

namespace App::World {

struct UnitDescription {
  QString name;
  QString nation;
  int health = 0;
  int max_health = 0;
  int soldiers = 0;
  int max_soldiers = 0;
  bool is_building = false;
  bool alive = false;
};

struct UnitStamina {
  float ratio = 1.0F;
  bool is_running = false;
  bool can_run = false;
};

[[nodiscard]] auto describe_unit(const Engine::Core::World* world,
                                 Engine::Core::EntityID id,
                                 UnitDescription& out) -> bool;

[[nodiscard]] auto unit_type_key(const Engine::Core::World* world,
                                 Engine::Core::EntityID id,
                                 QString& out) -> bool;

[[nodiscard]] auto describe_unit_stamina(const Engine::Core::World* world,
                                         Engine::Core::EntityID id,
                                         UnitStamina& out) -> bool;

[[nodiscard]] auto
unit_activity(const Engine::Core::World* world,
              Engine::Core::EntityID id) -> Game::Systems::UnitActivity;

} // namespace App::World
