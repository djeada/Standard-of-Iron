#pragma once

#include "render/creature/animation_state_components.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "wildlife_variant.h"

namespace Render::GL {
struct DrawContext;
class ISubmitter;
} // namespace Render::GL

namespace Render::Wildlife {

struct WildlifeRenderInputs {
  Render::Creature::Pipeline::CreatureKind kind{
      Render::Creature::Pipeline::CreatureKind::Sheep};
  Render::GL::WildlifeVariant variant{};
  Render::Creature::AnimationStateId state{Render::Creature::AnimationStateId::Idle};
  float phase{0.0F};

  Render::Creature::AnimationStateId outgoing_state{
      Render::Creature::AnimationStateId::Idle};
  float outgoing_phase{0.0F};
  float outgoing_weight{0.0F};
};

void submit_wildlife(const Render::GL::DrawContext& ctx,
                     const WildlifeRenderInputs& inputs,
                     Render::GL::ISubmitter& out);

} // namespace Render::Wildlife
