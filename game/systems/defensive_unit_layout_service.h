#pragma once

#include <QVector3D>

#include "nation_registry.h"

namespace Engine::Core {
class Entity;
}

namespace Game::Systems {

struct DefensiveUnitLayoutDamageContext {
  bool is_missile{false};
  bool is_cavalry_impact{false};
  QVector3D attack_origin;
};

class DefensiveUnitLayoutService {
public:
  [[nodiscard]] static auto
  profile_for(const Engine::Core::Entity& entity) -> const DefensiveUnitLayoutProfile*;

  [[nodiscard]] static auto is_active(const Engine::Core::Entity& entity) -> bool;
  [[nodiscard]] static auto is_formed(const Engine::Core::Entity& entity) -> bool;

  [[nodiscard]] static auto
  damage_multiplier(const Engine::Core::Entity& target,
                    const DefensiveUnitLayoutDamageContext& context) -> float;

  [[nodiscard]] static auto
  attack_output_multiplier(const Engine::Core::Entity& entity) -> float;

  [[nodiscard]] static auto
  move_speed_multiplier(const Engine::Core::Entity& entity) -> float;

  [[nodiscard]] static auto
  turn_speed_multiplier(const Engine::Core::Entity& entity) -> float;

  [[nodiscard]] static auto blocks_charge(const Engine::Core::Entity& entity) -> bool;
  [[nodiscard]] static auto holds_position(const Engine::Core::Entity& entity) -> bool;
};

} // namespace Game::Systems
