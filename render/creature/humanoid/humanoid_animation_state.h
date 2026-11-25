#pragma once

#include <cstdint>

#include "animation/ambient_pose_manifest.h"
#include "animation/playback_manifest.h"
#include "render/creature/animation_inputs.h"
#include "render/creature/combat_visual_state.h"
#include "render/creature/humanoid/runtime/combat_root_smoothing.h"
#include "render/creature/pipeline/humanoid_transition_continuity.h"

namespace Render::Creature {

struct HumanoidAnimationStateComponent {
  float idle_duration{0.0F};
  float last_sample_time{0.0F};
  bool initialized{false};

  Animation::HumanoidLocomotionPersistentState locomotion{};

  float guard_pose_progress{0.0F};
  float hold_pose_progress{0.0F};
  Animation::HumanoidAmbientRuntimeState ambient_idle{};
  Animation::HumanoidConstructionTransitionState construction_transition{};
  CombatVisualPersistentState combat_visual{};
  Render::Humanoid::CombatRootSmoothingState combat_root{};

  Pipeline::HumanoidTransitionMemory transition{};
  float jump_pitch_degrees{0.0F};
  float jump_pitch_time{-1.0F};

  float last_reaction_time{-1.0F};
  Pipeline::HumanoidOverlayMemory overlay{};
};

} // namespace Render::Creature
