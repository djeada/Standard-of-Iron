#pragma once

#include "combat_action_definition.h"
#include "weapon_trace.h"
#include "weapon_trace_sampling.h"

namespace Game::Systems::CombatActions {

[[nodiscard]] auto
sample_baked_sword_trace_segment(const AttackerFrame& frame,
                                 const CombatActionDefinition& definition,
                                 float previous,
                                 float current) -> WeaponTraceSegment;

[[nodiscard]] auto
sample_baked_spear_trace_segment(const AttackerFrame& frame,
                                 const CombatActionDefinition& definition,
                                 float previous,
                                 float current) -> WeaponTraceSegment;

} // namespace Game::Systems::CombatActions
