#pragma once

#include <cstdint>

#include "render/creature/elephant/dimensions.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/render_request.h"

namespace Render::Creature::Pipeline {

struct PreparedElephantBodyState {
  CreatureGraphOutput graph{};
  Render::GL::ElephantVariant variant{};
  Render::Creature::AnimationStateId animation_state{
      Render::Creature::AnimationStateId::Idle};
  float phase{0.0F};
  std::uint32_t clip_variant{0U};
  Render::Creature::AnimationStateId outgoing_state{
      Render::Creature::AnimationStateId::Idle};
  float outgoing_phase{0.0F};
  float outgoing_weight{0.0F};
};

} // namespace Render::Creature::Pipeline
