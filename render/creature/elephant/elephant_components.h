#pragma once

#include <cstdint>

#include "render/creature/elephant/dimensions.h"
#include "render/creature/elephant/runtime/gait_state.h"
#include "render/creature/render_request.h"

namespace Render::Creature {

struct ElephantAnimationStateComponent {
  Render::GL::ElephantGaitState gait_state{};
  float locomotion_phase{0.0F};
  float locomotion_phase_time{0.0F};
  bool locomotion_phase_valid{false};
  float body_yaw{0.0F};
  float body_yaw_time{0.0F};
  float turn_rate{0.0F};
  bool body_yaw_valid{false};
  bool pivoting{false};
  AnimationStateId clip_state{AnimationStateId::Idle};
  float clip_phase{0.0F};
  AnimationStateId outgoing_state{AnimationStateId::Idle};
  float outgoing_phase{0.0F};
  float blend_started_at{-1.0F};
  float blend_duration{0.0F};
  float clip_time{-1.0F};
  bool clip_valid{false};
};

struct ElephantAnatomyComponent {
  Render::GL::ElephantProfile profile{};
  Render::GL::HowdahAttachmentFrame howdah_frame{};
  std::uint32_t seed{0};
  bool baked{false};
};

} // namespace Render::Creature
