#pragma once

#include <QString>
#include <QVariantMap>

#include <cstdint>
#include <vector>

#include "game/core/entity.h"
#include "game/units/spawn_type.h"

namespace Engine::Core {
class World;
}

namespace App::Core {

struct FocusTargetInfo {
  bool valid = false;
  Engine::Core::EntityID id = Engine::Core::NULL_ENTITY;
  QString name;
  QString nation;
  QString type_key;
  int owner_id = 0;
  bool is_building = false;
  bool is_enemy = false;
  bool is_own = false;
  int health = 0;
  int max_health = 0;
  double health_ratio = 0.0;
  QString activity = QStringLiteral("idle");
  QString activity_state = QStringLiteral("active");

  int attacked_by_selection = 0;
  int attacked_by_local = 0;
  int attackers_incoming = 0;
};

[[nodiscard]] auto
resolve_focus_entity(Engine::Core::World* world,
                     const std::vector<Engine::Core::EntityID>& selection,
                     Engine::Core::EntityID inspected,
                     int local_owner_id) -> Engine::Core::EntityID;

[[nodiscard]] auto primary_attack_target(
    Engine::Core::World* world,
    const std::vector<Engine::Core::EntityID>& selection) -> Engine::Core::EntityID;

[[nodiscard]] auto count_units_attacking(Engine::Core::World* world,
                                         Engine::Core::EntityID target,
                                         int owner_id) -> int;

[[nodiscard]] auto
count_selection_attacking(Engine::Core::World* world,
                          const std::vector<Engine::Core::EntityID>& selection,
                          Engine::Core::EntityID target) -> int;

[[nodiscard]] auto count_enemies_attacking(Engine::Core::World* world,
                                           Engine::Core::EntityID target,
                                           int local_owner_id) -> int;

[[nodiscard]] auto building_display_name(Game::Units::SpawnType type) -> QString;

[[nodiscard]] auto focus_target_to_variant(const FocusTargetInfo& info) -> QVariantMap;

} // namespace App::Core
