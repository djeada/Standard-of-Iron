#pragma once

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>

#include <cstdint>

#include "render/creature/animation_inputs.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/render_request.h"

namespace Engine::Core {
class UnitComponent;
}

namespace Render::GL {
class Mesh;
class Shader;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

struct PreparedRenderPassIntent {
  RenderPassIntent pass{RenderPassIntent::Main};
  bool emits_body{true};
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
  const Render::GL::DrawContext* ctx{nullptr};
  QVector3D soldier_world_pos{};
  CreatureLodConfig config{};
};

struct PreparedHumanoidShadowState {
  bool enabled{false};
  RenderPassIntent pass{RenderPassIntent::Main};
  Render::GL::Shader* shader{nullptr};
  Render::GL::Mesh* mesh{nullptr};
  QMatrix4x4 model{};
  QVector2D light_dir{};
  float alpha{0.0F};
};

using PreparedQuadrupedShadowState = PreparedHumanoidShadowState;

struct HumanoidShadowStateInputs {
  const Render::GL::DrawContext* ctx{nullptr};
  const CreatureGraphOutput* graph{nullptr};
  Engine::Core::UnitComponent* unit{nullptr};
  QVector3D soldier_world_pos{};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  float camera_distance{0.0F};
  bool mounted{false};

  float facing_yaw_degrees{0.0F};

  float intensity_scale{1.0F};
  float surface_world_y{0.0F};
  bool surface_height_valid{false};
};

struct QuadrupedShadowStateInputs {
  const Render::GL::DrawContext* ctx{nullptr};
  const CreatureGraphOutput* graph{nullptr};
  QVector3D world_pos{};
  CreatureKind kind{CreatureKind::Horse};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  float camera_distance{0.0F};
  float facing_yaw_degrees{0.0F};
  float intensity_scale{1.0F};
  float surface_world_y{0.0F};
  bool surface_height_valid{false};
};

[[nodiscard]] inline auto
pass_intent_for(const CreatureGraphOutput& graph) noexcept -> PreparedRenderPassIntent {
  PreparedRenderPassIntent intent;
  intent.pass = graph.pass_intent;
  intent.emits_body = !graph.culled;
  return intent;
}

[[nodiscard]] auto resolve_humanoid_lod_state(const HumanoidLodStateInputs& inputs)
    -> PreparedCreatureLodState;

[[nodiscard]] auto prepare_humanoid_shadow_state(
    const HumanoidShadowStateInputs& inputs) -> PreparedHumanoidShadowState;

[[nodiscard]] auto prepare_quadruped_shadow_state(
    const QuadrupedShadowStateInputs& inputs) -> PreparedQuadrupedShadowState;

} // namespace Render::Creature::Pipeline
