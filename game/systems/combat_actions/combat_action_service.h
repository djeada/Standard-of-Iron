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
  std::uint16_t target_soldier_slot{
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot};
  int move_right_axis{0};
  int move_forward_axis{0};
  float primary_held_duration{0.0F};
  Engine::Core::CommanderCombatIntentType intent_type{
      Engine::Core::CommanderCombatIntentType::Light};

  bool has_swing{false};
  Engine::Core::MeleeIntent swing{};
};

struct AttackRequestResult {
  bool accepted{false};
  bool buffered{false};

  Engine::Core::CombatIntentOutcome outcome{
      Engine::Core::CombatIntentOutcome::Accepted};
  CombatActionId action_id{CombatActionId::None};
  const CombatActionDefinition* definition{nullptr};
};

void expire_stale_intents(Engine::Core::CombatIntentQueueComponent& queue,
                          float delta_time);

class CombatActionService {
public:
  [[nodiscard]] static auto
  request_attack(Engine::Core::World& world,
                 const AttackRequest& request) -> AttackRequestResult;
};

} // namespace Game::Systems::CombatActions
