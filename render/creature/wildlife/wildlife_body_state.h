#pragma once

#include <cstdint>

#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/render_request.h"
#include "render/creature/wildlife/wildlife_variant.h"

namespace Render::Creature::Pipeline {

struct PreparedWildlifeBodyState {
  CreatureGraphOutput graph{};
  Render::GL::WildlifeVariant variant{};
  Render::Creature::Pipeline::CreatureKind kind{
      Render::Creature::Pipeline::CreatureKind::Sheep};
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
