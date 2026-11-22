#pragma once

#include <cstdint>

#include "render/creature/horse/dimensions.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/render_request.h"

namespace Render::Creature::Pipeline {

// A prepared horse body, ready for CreatureRenderBatch::add_quadruped.
struct PreparedHorseBodyState {
  CreatureGraphOutput graph{};
  Render::GL::HorseVariant variant{};
  Render::Creature::AnimationStateId animation_state{
      Render::Creature::AnimationStateId::Idle};
  float phase{0.0F};
  std::uint32_t clip_variant{0U};
};

} // namespace Render::Creature::Pipeline
