#pragma once

// Shared by the instance_prepare*.cpp translation units; not a public header.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "animation/clip_manifest.h"
#include "animation/combat_root_motion_manifest.h"
#include "animation/layout_manifest.h"
#include "animation/locomotion_manifest.h"
#include "animation/micro_variation_manifest.h"
#include "game/core/component.h"
#include "game/core/death_sequence.h"
#include "game/core/entity.h"
#include "game/core/world.h"
#include "game/formation/unit_layout_state_system.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/units/spawn_type.h"
#include "render/creature/animation_core_bridge.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/combat_animation_diagnostics.h"
#include "render/creature/humanoid/animation/animation_inputs.h"
#include "render/creature/humanoid/humanoid_constants.h"
#include "render/creature/humanoid/pipeline/humanoid_playback.h"
#include "render/creature/humanoid/runtime/formation_instance_layout.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/humanoid_runtime_types.h"
#include "render/creature/humanoid/runtime/instance_state.h"
#include "render/creature/humanoid/runtime/runtime_context.h"
#include "render/creature/humanoid/runtime/runtime_stats.h"
#include "render/creature/humanoid/runtime/soldier_ground_motion.h"
#include "render/creature/humanoid/runtime/soldier_turn_smoothing.h"
#include "render/creature/humanoid/runtime/unit_layout_spacing.h"
#include "render/creature/pipeline/corpse_sink.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_prepared_state.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/humanoid_animation_selection.h"
#include "render/creature/pipeline/humanoid_transition_continuity.h"
#include "render/creature/pipeline/lod_decision.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pose_intent.h"
#include "render/frame/draw_context.h"
#include "render/frame/submission_visibility.h"
#include "render/frame/terrain_contact.h"
#include "render/profiling/frame_profile.h"
#include "scene/camera.h"

namespace Render::Humanoid {

namespace instance_prepare_detail {

constexpr float k_corpse_max_ground_tilt_degrees = 24.0F;

struct CasualtyLaunch {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
  float pitch{0.0F};
  float roll{0.0F};
};

constexpr float k_soldier_cull_enter_band = 1.5F;

constexpr float k_soldier_cull_keep_band = 5.0F;

constexpr std::uint32_t k_soldier_visibility_memory_frames = 12U;

constexpr std::uint32_t k_selection_refresh_period = 8U;

constexpr float k_wade_pose_start_depth = 0.12F;

constexpr float k_wade_pose_full_span = 0.20F;

namespace RCP = Render::Creature::Pipeline;

struct HumanoidUnitSnapshot {
  const HumanoidRendererBase* owner{nullptr};
  const DrawContext* ctx{nullptr};

  Engine::Core::UnitComponent* unit_comp{nullptr};
  Engine::Core::TransformComponent* transform_comp{nullptr};
  const Engine::Core::FormationPresentationComponent* formation_presentation{nullptr};
  const Engine::Core::CreaturePresentationComponent* creature_presentation{nullptr};
  const Engine::Core::FormationHitPresentationComponent* formation_hit{nullptr};
  Engine::Core::SoldierCasualtyAnimationComponent* casualties_comp{nullptr};

  FormationParams formation{};
  std::uint32_t seed{0U};
  int rows{1};
  int cols{1};
  int total_layout_count{0};
  int visible_count{0};
  std::size_t active_casualty_count{0U};
  bool is_mounted_spawn{false};
  bool has_entity_death{false};
  bool has_shared_formation_layout{false};

  std::vector<int>* live_slot_indices{nullptr};

  HumanoidVariant variant{};
  Render::Creature::Pipeline::UnitVisualSpec visual_spec{};
  float height_scale{1.0F};
  bool needs_height_scaling{false};
  float entity_ground_offset{0.0F};

  Game::Formation::UnitLayoutId unit_layout{Game::Formation::k_invalid_layout};
  float formed_ratio{1.0F};
  Game::Formation::UnitLayoutId blend_from_layout{Game::Formation::k_invalid_layout};
  float layout_blend_ratio{1.0F};
};

struct HumanoidFormationRuntime {
  HumanoidInstanceStateComponent* layout_cache_comp{nullptr};
  std::vector<Render::Entity::FormationInstance>* soldier_layout_storage{nullptr};
  RCP::CreatureLodConfig lod_config{};
  bool allow_animation_persistence{false};
  bool use_per_soldier_locomotion_state{false};
  bool preserve_soldier_state_prefix{false};
};

struct HumanoidUnitAnimationRuntime {
  Render::Creature::HumanoidAnimationStateComponent* humanoid_anim_state{nullptr};
  Render::GL::VisualMovementState visual_movement{};
  float local_enemy_pressure{0.0F};
  Animation::HumanoidCommanderJumpPose commander_jump{};
  Animation::HumanoidCommanderFlagRallyPose rally_pose{};
  float unit_health_ratio{1.0F};
  std::uint32_t unit_attack_target_id{0U};
  bool unit_attack_target_alive{false};
  bool record_animation_diagnostics{false};

  Render::Profiling::CombatAnimationDiagnostics* animation_diagnostics{nullptr};
  bool formation_fight_active{false};

  DrawContext inst_ctx{};
  QMatrix4x4 unit_base{};
  QVector3D root_position{};
  float root_yaw{0.0F};
  Animation::HumanoidLocomotionActionOverrideSample locomotion_override{};

  float turn_smoothing_dt{0.0F};
  bool turn_smoothing_active{false};
  float turn_smoothing_cap{2.5F};
  float ground_march_speed{2.0F};
  bool turn_smoothing_travel_yaw{true};
  bool turn_smoothing_stagger{true};
  bool turn_smoothing_pivot_wheel{true};

  bool unit_is_archer{false};
  std::uint32_t ctx_entity_id{0U};
  QVector3D unit_origin{};
  bool unit_fog_visible{true};

  Render::GL::HumanoidRenderStats* stats{nullptr};
};

struct CasualtyPose {
  float offset_x{0.0F};
  float offset_y{0.0F};
  float offset_z{0.0F};
  float pitch{0.0F};
  float roll{0.0F};
  float yaw{0.0F};
  const QMatrix4x4* base{nullptr};
  float root_yaw{0.0F};
};

struct SoldierScope {
  const HumanoidUnitSnapshot& unit;
  const HumanoidFormationRuntime& formation;
  HumanoidUnitAnimationRuntime& animation;
  const AnimationInputs& anim;
  std::uint32_t frame_index{0U};
  int idx{0};
  std::size_t slot{0U};
  const Render::Entity::FormationInstance& layout;
  const Engine::Core::FormationSoldierPresentation* presented{nullptr};
  HumanoidInstanceStateComponent* cache{nullptr};
};

struct SoldierDebugFlags {
  bool swing_recoil_active{false};
  bool last_swing_is_melee{false};
  bool combat_ready_stance{false};
};

struct SoldierDebugRoot {
  QVector3D position{};
  float yaw_degrees{0.0F};
  float up_y{1.0F};
  float scale_y{1.0F};
  float tilt_degrees{0.0F};
  float hit_reaction_tilt_degrees{0.0F};
};

struct SoldierDebugGait {
  float locomotion_blend{0.0F};
  float locomotion_presence{0.0F};
  float cycle_phase{0.0F};
  float travel_alignment{1.0F};
  float travel_lateral_share{0.0F};
  float action_link_weight{0.0F};
  bool persistent_valid{false};
  float persistent_last_sample_time{0.0F};
};

struct SoldierDebugRecord {
  float attack_phase{0.0F};
  Render::Creature::AnimationStateId animation_state{};
  HumanoidLOD lod{HumanoidLOD::Culled};
  Render::Profiling::SoldierCullReason cull_reason{
      Render::Profiling::SoldierCullReason::None};
  bool transient_recovery_override{false};
  SoldierDebugRoot root{};
  SoldierDebugGait gait{};
  std::optional<std::uint16_t> animation_clip{};
};

struct SoldierReaction {
  AnimationInputs anim;
  bool follows_authoritative_strike{true};
  float hit_reaction_base_intensity{0.0F};
  bool swing_recoil_active{false};
};

struct SoldierPlacement {
  QMatrix4x4 model{};
  QVector3D world_pos{};
  float yaw_degrees{0.0F};
  float entity_ground_offset{0.0F};
  SoldierTurnSmoothingResult turn_smoothing{};
  QVector3D turn_slot_world{};
  SoldierGroundMotion ground_motion{};
  bool has_shared_footsteps{false};
  bool elevated{false};
  bool casualty_body{false};
};

struct SoldierVisibility {
  bool outside_frustum{false};
  bool hidden_by_fog{false};
  float lens_gap_visibility{1.0F};

  [[nodiscard]] auto blocks_lens_gap() const noexcept -> bool {
    return lens_gap_visibility <= 0.0F;
  }
  [[nodiscard]] auto culled() const noexcept -> bool {
    return outside_frustum || hidden_by_fog || blocks_lens_gap();
  }
  [[nodiscard]] auto
  cull_reason() const noexcept -> Render::Profiling::SoldierCullReason {
    using Reason = Render::Profiling::SoldierCullReason;
    return outside_frustum ? Reason::Frustum
           : hidden_by_fog ? Reason::Fog
                           : Reason::LensGap;
  }
};

struct SoldierBasis {
  VariationParams variation;
  float combined_height_scale{1.0F};
  float yaw_radians{0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  VisualLocomotionSample visual_locomotion{};
};

struct SoldierPersistentState {
  Render::Creature::HumanoidAnimationStateComponent* animation{nullptr};
  Render::Creature::SoldierCombatLaneState* lane{nullptr};
};

struct SoldierFormationFight {
  bool formation_melee{false};
  bool in_formation_fight{false};
};

struct SoldierConstruction {
  Render::GL::ConstructionRole role{Render::GL::ConstructionRole::None};
  float blend{0.0F};
};

struct SoldierGrounding {
  bool world_already_grounded{false};
  bool surface_height_valid{false};
  float surface_world_y{0.0F};
};

struct SoldierShadowInputs {
  const RCP::CreatureGraphOutput* graph{nullptr};
  const AnimationInputs* anim{nullptr};
  QVector3D world_pos{};
  HumanoidLOD lod{HumanoidLOD::Full};
  float camera_distance{0.0F};
  float yaw_degrees{0.0F};
  SoldierGrounding grounding{};
  float lens_gap_visibility{1.0F};
};

auto resolve_casualty_launch(
    const Engine::Core::SoldierCasualtyAnimationComponent::Entry& entry,
    float velocity_x,
    float velocity_z) -> CasualtyLaunch;

auto humanoid_selection_is_steady(
    const Render::GL::HumanoidAnimationContext& anim_ctx) noexcept -> bool;

auto snapshot_humanoid_unit(const HumanoidRendererBase& owner,
                            const DrawContext& ctx,
                            const AnimationInputs& anim) -> HumanoidUnitSnapshot;

void ease_soldier_offsets(HumanoidInstanceStateComponent& cache,
                          std::vector<Render::Entity::FormationInstance>& instances,
                          float time,
                          bool about_faced,
                          bool enabled);

auto prepare_formation_runtime(const HumanoidUnitSnapshot& s,
                               const AnimationInputs& anim,
                               std::uint32_t frame_index) -> HumanoidFormationRuntime;

auto resolve_unit_animation_runtime(const HumanoidUnitSnapshot& s,
                                    const HumanoidFormationRuntime& f,
                                    const AnimationInputs& anim,
                                    HumanoidRuntimeContext& runtime)
    -> HumanoidUnitAnimationRuntime;

auto make_soldier_scope(const HumanoidUnitSnapshot& s,
                        const HumanoidFormationRuntime& f,
                        HumanoidUnitAnimationRuntime& u,
                        const AnimationInputs& anim,
                        std::uint32_t frame_index,
                        int idx) -> SoldierScope;

auto debug_gait_of(const HumanoidAnimationContext& anim_ctx) -> SoldierDebugGait;

auto measure_debug_root(const QMatrix4x4& model,
                        const QVector3D& world_pos,
                        float yaw_degrees,
                        float hit_reaction_tilt_degrees) -> SoldierDebugRoot;

void record_soldier_debug(const SoldierScope& sc,
                          const AnimationInputs& resolved_anim,
                          const SoldierDebugFlags& flags,
                          const SoldierDebugRecord& record);

auto resolve_soldier_reaction(const SoldierScope& sc,
                              const AnimationInputs& soldier_anim) -> SoldierReaction;

void apply_guard_shield_pose(const SoldierScope& sc, AnimationInputs& a);

auto resolve_soldier_footing(const SoldierScope& sc,
                             float applied_yaw_offset,
                             const AnimationInputs& a,
                             SoldierPlacement& p) -> bool;

void build_soldier_model(const SoldierScope& sc,
                         const CasualtyPose& casualty,
                         float applied_yaw_offset,
                         bool turn_smoothed,
                         SoldierPlacement& p);

void observe_soldier_steps(const SoldierScope& sc,
                           bool turn_smoothed,
                           SoldierPlacement& p,
                           AnimationInputs& a);

auto place_soldier(const SoldierScope& sc,
                   const CasualtyPose& casualty,
                   AnimationInputs& a) -> SoldierPlacement;

auto evaluate_soldier_visibility(const SoldierScope& sc,
                                 const SoldierPlacement& p) -> SoldierVisibility;

void count_soldier_cull(Render::GL::HumanoidRenderStats& stats,
                        const SoldierVisibility& visibility);

auto resolve_soldier_basis(const SoldierScope& sc,
                           const SoldierPlacement& p,
                           const AnimationInputs& a,
                           DrawContext& inst_ctx) -> SoldierBasis;

auto resolve_soldier_persistent_state(const SoldierScope& sc) -> SoldierPersistentState;

auto preferred_lane_for_role(Engine::Core::FormationSoldierCombatRole role,
                             Render::Creature::SoldierCombatLane fallback)
    -> Render::Creature::SoldierCombatLane;

auto resolve_soldier_lane(const SoldierScope& sc,
                          const SoldierPersistentState& persistent,
                          const AnimationInputs& a,
                          const SoldierFormationFight& fight)
    -> Render::Creature::CombatLaneProfile;

auto resolve_visual_attack_request(
    const SoldierScope& sc,
    const Render::Creature::CombatVisualPersistentState& previous,
    const SoldierFormationFight& fight,
    bool lone_lock_attack,
    AnimationInputs& a) -> bool;

auto build_raw_combat_inputs(const SoldierScope& sc,
                             const AnimationInputs& a,
                             const SoldierReaction& reaction,
                             const SoldierFormationFight& fight,
                             bool attack_requested,
                             std::uint8_t base_attack_variant)
    -> Render::Creature::CombatVisualRawInputs;

auto resolve_soldier_combat(const SoldierScope& sc,
                            const SoldierPersistentState& persistent,
                            SoldierReaction& reaction) -> bool;

auto resolve_soldier_locomotion(const SoldierScope& sc,
                                const SoldierPlacement& p,
                                const SoldierBasis& basis,
                                const SoldierPersistentState& persistent,
                                const AnimationInputs& a) -> HumanoidLocomotionState;

auto resolve_soldier_construction(const SoldierScope& sc,
                                  const SoldierPersistentState& persistent,
                                  AnimationInputs& a) -> SoldierConstruction;

void apply_soldier_ambient_idle(const SoldierScope& sc,
                                const SoldierPersistentState& persistent,
                                bool render_has_locomotion,
                                HumanoidAnimationContext& anim_ctx);

auto build_soldier_animation_context(const SoldierScope& sc,
                                     const SoldierPlacement& p,
                                     const SoldierBasis& basis,
                                     const SoldierPersistentState& persistent,
                                     bool render_has_locomotion,
                                     AnimationInputs& a) -> HumanoidAnimationContext;

auto ground_soldier(const SoldierScope& sc,
                    const SoldierPlacement& p,
                    const SoldierBasis& basis,
                    const HumanoidAnimationContext& anim_ctx,
                    DrawContext& inst_ctx) -> SoldierGrounding;

void apply_soldier_wading(const SoldierScope& sc,
                          const DrawContext& inst_ctx,
                          HumanoidAnimationContext& anim_ctx);

void apply_dodge_roll(const Engine::Core::CreaturePresentationComponent& presentation,
                      const QVector3D& fallback_axis,
                      DrawContext& inst_ctx);

auto commander_jump_pitch_target(const SoldierScope& sc) -> float;

auto apply_commander_airborne_pose(const SoldierScope& sc,
                                   const SoldierBasis& basis,
                                   const SoldierPersistentState& persistent,
                                   DrawContext& inst_ctx) -> bool;

void apply_casualty_settle(const SoldierScope& sc,
                           const SoldierPlacement& p,
                           float casualty_offset_y,
                           const AnimationInputs& a,
                           DrawContext& inst_ctx);

auto resolve_soldier_root_motion(const SoldierScope& sc,
                                 const SoldierPlacement& p,
                                 const SoldierBasis& basis,
                                 const SoldierReaction& reaction)
    -> Animation::CombatRootMotionSample;

void apply_soldier_combat_root(const SoldierScope& sc,
                               const SoldierPlacement& p,
                               const SoldierBasis& basis,
                               const SoldierPersistentState& persistent,
                               const SoldierReaction& reaction,
                               DrawContext& inst_ctx);

auto select_soldier_animation(const SoldierScope& sc,
                              const DrawContext& inst_ctx,
                              const AnimationInputs& a,
                              bool render_has_locomotion,
                              bool world_already_grounded,
                              const RCP::CreatureLodDecision& lod_decision,
                              const HumanoidAnimationContext& anim_ctx)
    -> RCP::CreatureGraphOutput;

void apply_soldier_transition_link(const SoldierScope& sc,
                                   const SoldierPersistentState& persistent,
                                   RCP::CreatureGraphOutput& graph_output,
                                   HumanoidAnimationContext& anim_ctx);

void advance_soldier_overlay(const SoldierScope& sc,
                             const SoldierPersistentState& persistent,
                             bool dodging,
                             const HumanoidAnimationContext& anim_ctx,
                             RCP::CreatureGraphOutput& graph_output);

void append_soldier_shadow(const SoldierScope& sc,
                           const DrawContext& inst_ctx,
                           const SoldierShadowInputs& in,
                           HumanoidPreparation& out);

void append_soldier_bodies(const SoldierScope& sc,
                           const DrawContext& inst_ctx,
                           const RCP::CreatureGraphOutput& graph_output,
                           const HumanoidAnimationContext& anim_ctx,
                           HumanoidLOD lod,
                           float lens_gap_visibility,
                           HumanoidPreparation& out);

void append_prepared_soldier(const HumanoidUnitSnapshot& s,
                             const HumanoidFormationRuntime& f,
                             HumanoidUnitAnimationRuntime& u,
                             const AnimationInputs& anim,
                             std::uint32_t frame_index,
                             HumanoidPreparation& out,
                             int idx,
                             const AnimationInputs& soldier_anim,
                             const CasualtyPose& casualty);

} // namespace instance_prepare_detail

} // namespace Render::Humanoid
