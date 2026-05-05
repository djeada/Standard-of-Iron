#pragma once

#include "../../elephant/dimensions.h"
#include "../../gl/humanoid/humanoid_types.h"
#include "../../horse/dimensions.h"
#include "../render_request.h"
#include "creature_render_graph.h"

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <cstdint>

namespace Engine::Core {
class UnitComponent;
}

namespace Render::GL {
class Mesh;
class Shader;
} // namespace Render::GL

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

struct PreparedAnimationState {
  Render::GL::AnimationInputs inputs{};
  bool used_override{false};
};

struct PreparedCreatureLodState {
  CreatureLodDecision decision{};
  float camera_distance{0.0F};
  bool budget_granted_full{true};
};

struct HumanoidLodStateInputs {
  const Render::GL::DrawContext *ctx{nullptr};
  QVector3D soldier_world_pos{};
  CreatureLodConfig config{};
  std::uint32_t frame_index{0U};
  std::uint32_t instance_seed{0U};
};

struct PreparedHumanoidShadowState {
  bool enabled{false};
  RenderPassIntent pass{RenderPassIntent::Main};
  Render::GL::Shader *shader{nullptr};
  Render::GL::Mesh *mesh{nullptr};
  QMatrix4x4 model{};
  QVector2D light_dir{};
  float alpha{0.0F};
};

// Reuse the same state structure for quadruped (horse/elephant) shadows.
using PreparedQuadrupedShadowState = PreparedHumanoidShadowState;

struct HumanoidShadowStateInputs {
  const Render::GL::DrawContext *ctx{nullptr};
  const CreatureGraphOutput *graph{nullptr};
  Engine::Core::UnitComponent *unit{nullptr};
  QVector3D soldier_world_pos{};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  float camera_distance{0.0F};
  bool mounted{false};
};

struct QuadrupedShadowStateInputs {
  const Render::GL::DrawContext *ctx{nullptr};
  const CreatureGraphOutput *graph{nullptr};
  QVector3D world_pos{};
  CreatureKind kind{CreatureKind::Horse};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  float camera_distance{0.0F};
};

[[nodiscard]] inline auto pass_intent_for(
    const CreatureGraphOutput &graph) noexcept -> PreparedRenderPassIntent {
  PreparedRenderPassIntent intent;
  intent.pass = graph.pass_intent;
  intent.emits_body = !graph.culled;
  intent.emits_post_body_draws = graph.pass_intent == RenderPassIntent::Main;
  return intent;
}

[[nodiscard]] auto resolve_humanoid_animation_state(
    const Render::GL::DrawContext &ctx) -> PreparedAnimationState;

[[nodiscard]] auto resolve_elephant_animation_state(
    const Render::GL::DrawContext &ctx) -> PreparedAnimationState;

[[nodiscard]] auto resolve_humanoid_lod_state(
    const HumanoidLodStateInputs &inputs) -> PreparedCreatureLodState;

[[nodiscard]] auto prepare_humanoid_shadow_state(
    const HumanoidShadowStateInputs &inputs) -> PreparedHumanoidShadowState;

[[nodiscard]] auto prepare_quadruped_shadow_state(
    const QuadrupedShadowStateInputs &inputs) -> PreparedQuadrupedShadowState;

} // namespace Render::Creature::Pipeline
