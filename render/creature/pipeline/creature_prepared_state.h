#pragma once

#include "../../elephant/attachment_frames.h"
#include "../../elephant/dimensions.h"
#include "../../entity/registry.h"
#include "../../gl/humanoid/humanoid_types.h"
#include "../../horse/attachment_frames.h"
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
struct AnimationInputs;
struct DrawContext;
struct ElephantProfile;
struct HorseProfile;
class HumanoidRendererBase;
class IFormationCalculator;
class Mesh;
class Shader;
} // namespace Render::GL

namespace Render::Humanoid {

struct SoldierLayout {
  float offset_x{0.0F};
  float offset_z{0.0F};
  float vertical_jitter{0.0F};
  float yaw_offset{0.0F};
  float phase_offset{0.0F};
  std::uint32_t inst_seed{0};
};

struct SoldierLayoutInputs {
  int idx{0};
  int row{0};
  int col{0};
  int rows{0};
  int cols{0};
  float formation_spacing{0.0F};
  std::uint32_t seed{0};
  bool force_single_soldier{false};
  bool melee_attack{false};
  float animation_time{0.0F};
};

[[nodiscard]] auto build_soldier_layout(
    const Render::GL::IFormationCalculator &formation_calculator,
    const SoldierLayoutInputs &inputs) -> SoldierLayout;

struct HumanoidLocomotionInputs {
  Render::GL::AnimationInputs anim{};
  Render::GL::VariationParams variation{};
  float move_speed{0.0F};
  QVector3D locomotion_direction{0.0F, 0.0F, 1.0F};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
  bool has_movement_target{false};
  float animation_time{0.0F};
  float phase_offset{0.0F};
};

struct HumanoidLocomotionState {
  Render::GL::HumanoidMotionState motion_state{
      Render::GL::HumanoidMotionState::Idle};
  Render::GL::HumanoidGaitDescriptor gait{};
  QVector3D locomotion_direction{0.0F, 0.0F, 1.0F};
  QVector3D locomotion_velocity{0.0F, 0.0F, 0.0F};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
  float move_speed{0.0F};
  bool has_movement_target{false};
};

[[nodiscard]] auto build_humanoid_locomotion_state(
    const HumanoidLocomotionInputs &inputs) -> HumanoidLocomotionState;

} // namespace Render::Humanoid

namespace Render::Creature::Pipeline {

struct PreparedCreatureVisualState {
  UnitVisualSpec spec{};
  CreatureKind kind{CreatureKind::Humanoid};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  RenderPassIntent pass{RenderPassIntent::Main};
  std::uint32_t seed{0U};
  QMatrix4x4 world_from_unit{};
  bool world_already_grounded{true};
  EntityId entity_id{0};
};

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

struct PreparedHorseMotionState {
  Render::GL::HorseMotionSample motion{};
  Render::Creature::AnimationStateId animation_state{
      Render::Creature::AnimationStateId::Idle};
  std::uint16_t clip_id{0U};
};

struct HorseMotionStateInputs {
  const Render::GL::DrawContext *ctx{nullptr};
  const Render::GL::AnimationInputs *anim{nullptr};
  const Render::GL::HumanoidAnimationContext *rider_ctx{nullptr};
  Render::GL::HorseProfile *profile{nullptr};
  const Render::GL::HorseMotionSample *shared_motion{nullptr};
};

struct PreparedElephantMotionState {
  Render::GL::ElephantMotionSample motion{};
  Render::GL::HowdahAttachmentFrame howdah{};
  Render::Creature::AnimationStateId animation_state{
      Render::Creature::AnimationStateId::Idle};
};

struct ElephantMotionStateInputs {
  const Render::GL::DrawContext *ctx{nullptr};
  const Render::GL::AnimationInputs *anim{nullptr};
  Render::GL::ElephantProfile *profile{nullptr};
  const Render::GL::HowdahAttachmentFrame *shared_howdah{nullptr};
  const Render::GL::ElephantMotionSample *shared_motion{nullptr};
};

struct PreparedQuadrupedFrameState {
  Render::GL::DrawContext ctx{};
  CreatureGraphOutput graph{};
};

struct QuadrupedFrameStateInputs {
  const Render::GL::DrawContext *ctx{nullptr};
  const Render::GL::AnimationInputs *anim{nullptr};
  UnitVisualSpec spec{};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  std::uint32_t seed{0U};
  QVector3D pre_ground_translation{0.0F, 0.0F, 0.0F};
  float contact_y{0.0F};
  float ground_clearance{0.0F};
  bool use_contact_grounding{false};
};

struct PreparedHumanoidFormationState {
  Render::GL::FormationParams formation{};
  int rows{1};
  int cols{1};
  int visible_count{1};
  bool mounted{false};
};

struct HumanoidFormationStateInputs {
  const Render::GL::HumanoidRendererBase *owner{nullptr};
  const Render::GL::DrawContext *ctx{nullptr};
  const Render::GL::AnimationInputs *anim{nullptr};
  Engine::Core::UnitComponent *unit{nullptr};
};

struct HumanoidLodStateInputs {
  const Render::GL::DrawContext *ctx{nullptr};
  QVector3D soldier_world_pos{};
  CreatureLodConfig config{};
  std::uint32_t frame_index{0U};
  std::uint32_t instance_seed{0U};
};

struct PreparedGroundShadowState {
  bool enabled{false};
  RenderPassIntent pass{RenderPassIntent::Main};
  Render::GL::Mesh *mesh{nullptr};
  QMatrix4x4 model{};
  float alpha{0.0F};
};

struct GroundShadowStateInputs {
  const Render::GL::DrawContext *ctx{nullptr};
  const CreatureGraphOutput *graph{nullptr};
  Engine::Core::UnitComponent *unit{nullptr};
  QVector3D world_pos{};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  float camera_distance{0.0F};
  float shadow_size{0.16F};
  float width_scale{1.0F};
  float depth_scale{1.10F};
};

[[nodiscard]] inline auto pass_intent_for(
    const CreatureGraphOutput &graph) noexcept -> PreparedRenderPassIntent {
  PreparedRenderPassIntent intent;
  intent.pass = graph.pass_intent;
  intent.emits_body = !graph.culled;
  intent.emits_post_body_draws = graph.pass_intent == RenderPassIntent::Main;
  return intent;
}

[[nodiscard]] auto make_prepared_visual_state(
    const CreatureGraphOutput &graph) noexcept -> PreparedCreatureVisualState;

[[nodiscard]] auto resolve_humanoid_animation_state(
    const Render::GL::DrawContext &ctx) -> PreparedAnimationState;

[[nodiscard]] auto resolve_elephant_animation_state(
    const Render::GL::DrawContext &ctx) -> PreparedAnimationState;

[[nodiscard]] auto resolve_horse_motion_state(
    const HorseMotionStateInputs &inputs) -> PreparedHorseMotionState;

[[nodiscard]] auto resolve_elephant_motion_state(
    const ElephantMotionStateInputs &inputs) -> PreparedElephantMotionState;

[[nodiscard]] auto
resolve_humanoid_formation_state(const HumanoidFormationStateInputs &inputs)
    -> PreparedHumanoidFormationState;

[[nodiscard]] auto prepare_quadruped_frame_state(
    const QuadrupedFrameStateInputs &inputs) -> PreparedQuadrupedFrameState;

[[nodiscard]] auto resolve_humanoid_lod_state(
    const HumanoidLodStateInputs &inputs) -> PreparedCreatureLodState;

[[nodiscard]] auto prepare_ground_shadow_state(
    const GroundShadowStateInputs &inputs) -> PreparedGroundShadowState;

} // namespace Render::Creature::Pipeline
