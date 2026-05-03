#pragma once

#include "../../elephant/dimensions.h"
#include "../../gl/humanoid/humanoid_types.h"
#include "../../horse/dimensions.h"
#include "../render_request.h"
#include "creature_render_graph.h"

#include <cstdint>

namespace Render::Creature::Pipeline {

struct PreparedHumanoidBodyState {
  CreatureGraphOutput graph{};
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext animation{};
};

struct PreparedHorseBodyState {
  CreatureGraphOutput graph{};
  Render::GL::HorseVariant variant{};
  Render::Creature::AnimationStateId animation_state{
      Render::Creature::AnimationStateId::Idle};
  float phase{0.0F};
  std::uint32_t clip_variant{0U};
};

struct PreparedElephantBodyState {
  CreatureGraphOutput graph{};
  Render::GL::ElephantVariant variant{};
  Render::Creature::AnimationStateId animation_state{
      Render::Creature::AnimationStateId::Idle};
  float phase{0.0F};
  std::uint32_t clip_variant{0U};
};

struct PreparedRenderPassIntent {
  RenderPassIntent pass{RenderPassIntent::Main};
  bool emits_body{true};
  bool emits_post_body_draws{false};
};

[[nodiscard]] inline auto pass_intent_for(
    const CreatureGraphOutput &graph) noexcept -> PreparedRenderPassIntent {
  PreparedRenderPassIntent intent;
  intent.pass = graph.pass_intent;
  intent.emits_body = !graph.culled;
  intent.emits_post_body_draws = graph.pass_intent == RenderPassIntent::Main;
  return intent;
}

} // namespace Render::Creature::Pipeline
