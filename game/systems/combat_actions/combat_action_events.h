#pragma once

#include <vector>

#include "../../core/component.h"
#include "combat_action_definition.h"

namespace Game::Systems::CombatActions {

[[nodiscard]] auto crossed_events_between(const CombatActionDefinition& definition,
                                          float previous_normalized_time,
                                          float current_normalized_time,
                                          std::uint8_t first_event_index = 0U)
    -> std::vector<CombatActionEvent>;

[[nodiscard]] auto advance_combat_action_events(
    Engine::Core::RpgCommanderActionComponent& action,
    float delta_time,
    const CombatActionDefinition& definition) -> std::vector<CombatActionEvent>;

void reset_combat_action_event_runtime(
    Engine::Core::RpgCommanderActionComponent& action);

} // namespace Game::Systems::CombatActions
