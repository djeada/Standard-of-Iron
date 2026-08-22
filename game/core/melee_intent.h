#pragma once

#include "../../animation/melee_swing_manifest.h"

namespace Engine::Core {

using MeleeIntent = Animation::MeleeIntent;
using MeleeRestDirection = Animation::MeleeRestDirection;

using Animation::blend_melee_intent;
using Animation::complete_melee_intent;
using Animation::k_melee_default_reach;
using Animation::k_melee_intent_min_axis;
using Animation::melee_intent_from_strike_angle;
using Animation::melee_intent_resting_direction;
using Animation::melee_intent_strike_delta;
using Animation::normalize_melee_intent;
using Animation::normalized_melee_intent;

} // namespace Engine::Core
