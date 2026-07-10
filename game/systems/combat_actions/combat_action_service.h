#pragma once

#include "../../core/entity.h"
#include "combat_action_definition.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::CombatActions {

struct AttackRequest {
  Engine::Core::EntityID attacker_id{0};
  Engine::Core::EntityID target_hint_id{0};
  int move_right_axis{0};
  int move_forward_axis{0};
  float primary_held_duration{0.0F};
};

struct AttackRequestResult {
  bool accepted{false};
  bool buffered{false};
  CombatActionId action_id{CombatActionId::None};
  const CombatActionDefinition* definition{nullptr};
};

class CombatActionService {
public:
  [[nodiscard]] static auto
  request_attack(Engine::Core::World& world,
                 const AttackRequest& request) -> AttackRequestResult;
};

} // namespace Game::Systems::CombatActions
