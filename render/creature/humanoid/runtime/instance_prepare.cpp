#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include "render/creature/humanoid/pipeline/humanoid_render_batch.h"
#include "render/creature/humanoid/runtime/instance_prepare_internal.h"

namespace Render::Humanoid {

namespace instance_prepare_detail {

void append_soldier_shadow(const SoldierScope& sc,
                           const DrawContext& inst_ctx,
                           const SoldierShadowInputs& in,
                           HumanoidPreparation& out) {
  const AnimationInputs& a = *in.anim;
  RCP::HumanoidShadowStateInputs shadow_inputs{};
  shadow_inputs.ctx = &inst_ctx;
  shadow_inputs.graph = in.graph;
  shadow_inputs.unit = sc.unit.unit_comp;
  shadow_inputs.soldier_world_pos = in.world_pos;
  shadow_inputs.lod = in.lod;
  shadow_inputs.camera_distance = in.camera_distance;
  shadow_inputs.mounted = sc.unit.is_mounted_spawn;
  shadow_inputs.facing_yaw_degrees = in.yaw_degrees;
  shadow_inputs.intensity_scale = ((a.is_dying || a.is_dead) ? 0.45F : 1.0F) *
                                  RCP::corpse_shadow_scale(a.death_sink_progress);
  shadow_inputs.surface_world_y = in.grounding.surface_world_y;
  shadow_inputs.surface_height_valid = in.grounding.surface_height_valid;
  const auto shadow_state = RCP::prepare_humanoid_shadow_state(shadow_inputs);
  if (!shadow_state.enabled) {
    return;
  }
  if (out.shadow_batch.empty()) {
    out.shadow_batch.init(
        shadow_state.shader, shadow_state.mesh, shadow_state.light_dir);
  }
  out.shadow_batch.add(shadow_state.model,
                       shadow_state.alpha * in.lens_gap_visibility,
                       shadow_state.pass);
}

void append_soldier_bodies(const SoldierScope& sc,
                           const DrawContext& inst_ctx,
                           const RCP::CreatureGraphOutput& graph_output,
                           const HumanoidAnimationContext& anim_ctx,
                           HumanoidLOD lod,
                           float lens_gap_visibility,
                           HumanoidPreparation& out) {
  const auto& s = sc.unit;
  const DrawContext& ctx = *s.ctx;
  const HumanoidRendererBase& owner = *s.owner;
  const HumanoidVariant& variant = s.variant;
  auto& stats = *sc.animation.stats;
  HumanoidPose const pose{};
  auto const append_companions = [&]() {
    owner.append_companion_preparation(
        inst_ctx,
        variant,
        pose,
        anim_ctx,
        sc.layout.inst_seed,
        graph_output.lod,
        graph_output.humanoid_selection ? &*graph_output.humanoid_selection : nullptr,
        out);
  };

  const std::size_t first_body_request = out.bodies.requests().size();
  switch (lod) {
  case HumanoidLOD::Full:
    ++stats.lod_full;
    if (!s.is_mounted_spawn) {
      add_humanoid(out.bodies, graph_output, pose, variant, anim_ctx);
    }
    append_companions();
    break;

  case HumanoidLOD::Minimal:
    ++stats.lod_minimal;
    if (s.is_mounted_spawn && !ctx.allow_template_cache &&
        ctx.suppress_animation_state_persistence) {
      append_companions();
      break;
    }
    add_humanoid(out.bodies, graph_output, pose, variant, anim_ctx);
    break;

  case HumanoidLOD::Culled:
    break;
  }
  out.bodies.fade_requests_from(first_body_request, lens_gap_visibility);
}

void append_prepared_soldier(const HumanoidUnitSnapshot& s,
                             const HumanoidFormationRuntime& f,
                             HumanoidUnitAnimationRuntime& u,
                             const AnimationInputs& anim,
                             std::uint32_t frame_index,
                             HumanoidPreparation& out,
                             int idx,
                             const AnimationInputs& soldier_anim,
                             const CasualtyPose& casualty) {
  using namespace Render::GL;

  SoldierScope const sc = make_soldier_scope(s, f, u, anim, frame_index, idx);
  const DrawContext& ctx = *s.ctx;
  auto& stats = *u.stats;
  bool const record_animation_diagnostics = u.record_animation_diagnostics;

  SoldierReaction reaction = resolve_soldier_reaction(sc, soldier_anim);
  AnimationInputs& soldier_render_anim = reaction.anim;
  apply_guard_shield_pose(sc, soldier_render_anim);

  SoldierPlacement const placement = place_soldier(sc, casualty, soldier_render_anim);

  SoldierVisibility const visibility = evaluate_soldier_visibility(sc, placement);
  if (visibility.culled()) {
    count_soldier_cull(stats, visibility);
    if (record_animation_diagnostics) {
      SoldierDebugRecord record;
      record.attack_phase = soldier_render_anim.combat_visual.attack_phase;
      record.animation_state =
          Render::Creature::resolve_pose(soldier_render_anim).animation_state;
      record.lod = HumanoidLOD::Culled;
      record.cull_reason = visibility.cull_reason();
      record.root.position = placement.world_pos;
      record.root.yaw_degrees = placement.yaw_degrees;
      record_soldier_debug(sc,
                           soldier_render_anim,
                           {.swing_recoil_active = reaction.swing_recoil_active,
                            .last_swing_is_melee = soldier_render_anim.is_melee},
                           record);
    }
    return;
  }

  float const hit_reaction_tilt_degrees =
      record_animation_diagnostics
          ? Animation::resolve_humanoid_hit_reaction_transform(
                {
                    .active = soldier_render_anim.is_hit_reacting ||
                              reaction.swing_recoil_active,
                    .intensity = soldier_render_anim.hit_reaction_intensity,
                    .recoil_x = soldier_render_anim.hit_recoil_x,
                    .recoil_z = soldier_render_anim.hit_recoil_z,
                    .inst_seed = sc.layout.inst_seed,
                })
                .tilt_degrees
          : Animation::HumanoidHitReactionTransformSample{}.tilt_degrees;

  DrawContext& inst_ctx = u.inst_ctx;
  inst_ctx.model = placement.model;
  SoldierBasis const basis =
      resolve_soldier_basis(sc, placement, soldier_render_anim, inst_ctx);

  SoldierPersistentState const persistent = resolve_soldier_persistent_state(sc);
  bool const last_swing_is_melee = resolve_soldier_combat(sc, persistent, reaction);
  bool const render_has_locomotion =
      Render::Creature::is_moving_animation(soldier_render_anim.movement_state);
  HumanoidAnimationContext anim_ctx = build_soldier_animation_context(
      sc, placement, basis, persistent, render_has_locomotion, soldier_render_anim);

  SoldierGrounding const grounding =
      ground_soldier(sc, placement, basis, anim_ctx, inst_ctx);
  apply_soldier_wading(sc, inst_ctx, anim_ctx);
  bool const dodging = apply_commander_airborne_pose(sc, basis, persistent, inst_ctx);
  apply_casualty_settle(
      sc, placement, casualty.offset_y, soldier_render_anim, inst_ctx);
  apply_soldier_combat_root(sc, placement, basis, persistent, reaction, inst_ctx);
  anim_ctx.instance_position = inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));

  QVector3D const soldier_world_pos = anim_ctx.instance_position;
  SoldierDebugRoot debug_root;
  if (record_animation_diagnostics) {
    debug_root = measure_debug_root(inst_ctx.model,
                                    soldier_world_pos,
                                    placement.yaw_degrees,
                                    hit_reaction_tilt_degrees);
  }

  RCP::HumanoidLodStateInputs lod_inputs{};
  lod_inputs.ctx = &ctx;
  lod_inputs.soldier_world_pos = soldier_world_pos;
  lod_inputs.config = f.lod_config;
  const auto lod_state = RCP::resolve_humanoid_lod_state(lod_inputs);
  const auto& lod_decision = lod_state.decision;
  if (lod_decision.culled) {
    ++stats.soldiers_skipped_lod;
    if (record_animation_diagnostics) {
      SoldierDebugRecord record;
      record.attack_phase = anim_ctx.attack_phase;
      record.animation_state =
          Render::Creature::resolve_pose(anim_ctx.inputs).animation_state;
      record.lod = static_cast<HumanoidLOD>(lod_decision.lod);
      record.cull_reason = Render::Profiling::SoldierCullReason::Distance;
      record.root = debug_root;
      record.gait = debug_gait_of(anim_ctx);
      record_soldier_debug(sc,
                           anim_ctx.inputs,
                           {.swing_recoil_active = reaction.swing_recoil_active,
                            .last_swing_is_melee = last_swing_is_melee},
                           record);
    }
    return;
  }
  auto const soldier_lod = static_cast<HumanoidLOD>(lod_decision.lod);

  anim_ctx.idle_breath_phase = RCP::humanoid_idle_breath_phase_for_lod(
      anim.time, sc.layout.inst_seed, soldier_lod, ctx.prewarming_via_runtime_path);

  ++stats.soldiers_rendered;

  inst_ctx.soldier_index = static_cast<std::uint16_t>(idx);
  auto graph_output = select_soldier_animation(sc,
                                               inst_ctx,
                                               soldier_render_anim,
                                               render_has_locomotion,
                                               grounding.world_already_grounded,
                                               lod_decision,
                                               anim_ctx);
  apply_soldier_transition_link(sc, persistent, graph_output, anim_ctx);
  advance_soldier_overlay(sc, persistent, dodging, anim_ctx, graph_output);

  if (sc.cache != nullptr && sc.slot < sc.cache->visibility_frames.size()) {
    sc.cache->visibility_frames[sc.slot] = frame_index + 1U;
  }
  if (record_animation_diagnostics) {
    bool const transient_recovery_override =
        (soldier_render_anim.is_attacking != anim_ctx.inputs.is_attacking) ||
        (soldier_render_anim.combat_phase != anim_ctx.inputs.combat_phase) ||
        (std::abs(soldier_render_anim.combat_phase_progress -
                  anim_ctx.inputs.combat_phase_progress) > 1.0e-4F);
    SoldierDebugRecord record;
    record.attack_phase = anim_ctx.attack_phase;
    record.animation_state = graph_output.humanoid_selection->state;
    record.lod = soldier_lod;
    record.cull_reason = Render::Profiling::SoldierCullReason::None;
    record.transient_recovery_override = transient_recovery_override;
    record.root = debug_root;
    record.gait = debug_gait_of(anim_ctx);
    record.animation_clip = graph_output.humanoid_selection->clip_id;
    record_soldier_debug(
        sc,
        anim_ctx.inputs,
        {.swing_recoil_active = reaction.swing_recoil_active,
         .last_swing_is_melee = last_swing_is_melee,
         .combat_ready_stance = graph_output.humanoid_selection->clip_id ==
                                Animation::k_humanoid_combat_ready_clip},
        record);
  }

  append_soldier_shadow(sc,
                        inst_ctx,
                        {.graph = &graph_output,
                         .anim = &soldier_render_anim,
                         .world_pos = soldier_world_pos,
                         .lod = soldier_lod,
                         .camera_distance = lod_state.camera_distance,
                         .yaw_degrees = placement.yaw_degrees,
                         .grounding = grounding,
                         .lens_gap_visibility = visibility.lens_gap_visibility},
                        out);
  append_soldier_bodies(sc,
                        inst_ctx,
                        graph_output,
                        anim_ctx,
                        soldier_lod,
                        visibility.lens_gap_visibility,
                        out);
}

} // namespace instance_prepare_detail

using namespace instance_prepare_detail;

void prepare_humanoid_instances(const HumanoidRendererBase& owner,
                                const DrawContext& ctx,
                                const AnimationInputs& anim,
                                HumanoidRuntimeContext& runtime,
                                HumanoidPreparation& out) {
  using namespace Render::GL;

  const std::uint32_t frame_index = runtime.frame_index;
  const HumanoidUnitSnapshot snapshot = snapshot_humanoid_unit(owner, ctx, anim);
  const HumanoidFormationRuntime formation_runtime =
      prepare_formation_runtime(snapshot, anim, frame_index);
  HumanoidUnitAnimationRuntime unit_animation =
      resolve_unit_animation_runtime(snapshot, formation_runtime, anim, runtime);

  const int visible_count = snapshot.visible_count;
  const std::size_t active_casualty_count = snapshot.active_casualty_count;
  auto& stats = runtime.stats;
  stats.soldiers_total +=
      visible_count + static_cast<std::uint32_t>(active_casualty_count);

  out.bodies.reserve(out.bodies.size() + static_cast<std::size_t>(visible_count) +
                     active_casualty_count);

  const int total_layout_count = snapshot.total_layout_count;
  auto& live_slot_indices = *snapshot.live_slot_indices;
  auto& soldier_layouts = *formation_runtime.soldier_layout_storage;
  auto* casualties_comp = snapshot.casualties_comp;

  auto append_soldier = [&](int idx,
                            const AnimationInputs& soldier_anim,
                            const CasualtyPose& casualty = {}) {
    append_prepared_soldier(snapshot,
                            formation_runtime,
                            unit_animation,
                            anim,
                            frame_index,
                            out,
                            idx,
                            soldier_anim,
                            casualty);
  };

  for (int const idx : live_slot_indices) {
    append_soldier(idx, anim);
  }

  if (!ctx.force_single_soldier && casualties_comp != nullptr) {
    auto const& entries = casualties_comp->entries;
    auto* anchor_cache = formation_runtime.layout_cache_comp;
    if (anchor_cache != nullptr) {
      std::erase_if(anchor_cache->casualty_anchors, [&entries](auto const& anchor) {
        return std::none_of(
            entries.begin(), entries.end(), [&anchor](auto const& entry) {
              return entry.slot_index == anchor.slot_index;
            });
      });
    }
    for (const auto& entry : entries) {
      if (entry.slot_index >= total_layout_count ||
          std::find(live_slot_indices.begin(),
                    live_slot_indices.end(),
                    static_cast<int>(entry.slot_index)) != live_slot_indices.end()) {
        continue;
      }

      AnimationInputs casualty_anim{};
      casualty_anim.time = anim.time;
      casualty_anim.death_variant = entry.sequence_variant;
      if (entry.state == Engine::Core::DeathSequenceState::Dying) {
        casualty_anim.is_dying = true;
        casualty_anim.death_progress =
            (entry.state_duration > 0.0F)
                ? std::clamp(entry.state_time / entry.state_duration, 0.0F, 1.0F)
                : 1.0F;
      } else {
        casualty_anim.is_dead = true;
        casualty_anim.death_progress = 1.0F;
        casualty_anim.death_sink_progress = Engine::Core::death_sink_progress(entry);
      }
      float launch_velocity_x = entry.launch_velocity_x;
      float launch_velocity_z = entry.launch_velocity_z;
      auto const& casualty_layout =
          soldier_layouts[static_cast<std::size_t>(entry.slot_index)];
      float rest_x = entry.has_local_anchor ? entry.local_x : casualty_layout.offset_x;
      float rest_z = entry.has_local_anchor ? entry.local_z : casualty_layout.offset_z;
      float rest_yaw =
          entry.has_local_anchor ? entry.local_yaw : casualty_layout.yaw_offset;
      const QMatrix4x4* casualty_base = nullptr;
      float casualty_root_yaw = 0.0F;
      if (anchor_cache != nullptr) {
        auto anchor = std::find_if(anchor_cache->casualty_anchors.begin(),
                                   anchor_cache->casualty_anchors.end(),
                                   [&entry](auto const& candidate) {
                                     return candidate.slot_index == entry.slot_index;
                                   });
        if (anchor == anchor_cache->casualty_anchors.end()) {
          anchor_cache->casualty_anchors.push_back({entry.slot_index,
                                                    unit_animation.unit_base,
                                                    unit_animation.root_yaw,
                                                    rest_x,
                                                    rest_z,
                                                    rest_yaw,
                                                    launch_velocity_x,
                                                    launch_velocity_z});
          anchor = std::prev(anchor_cache->casualty_anchors.end());
        }
        casualty_base = &anchor->frame;
        casualty_root_yaw = anchor->root_yaw;
        rest_x = anchor->rest_x;
        rest_z = anchor->rest_z;
        rest_yaw = anchor->rest_yaw;
        launch_velocity_x = anchor->launch_velocity_x;
        launch_velocity_z = anchor->launch_velocity_z;
      }
      const CasualtyLaunch launch =
          resolve_casualty_launch(entry, launch_velocity_x, launch_velocity_z);
      append_soldier(static_cast<int>(entry.slot_index),
                     casualty_anim,
                     {.offset_x = rest_x - casualty_layout.offset_x + launch.x,
                      .offset_y = launch.y,
                      .offset_z = rest_z - casualty_layout.offset_z + launch.z,
                      .pitch = launch.pitch,
                      .roll = launch.roll,
                      .yaw = rest_yaw - casualty_layout.yaw_offset,
                      .base = casualty_base,
                      .root_yaw = casualty_root_yaw});
    }
  }
}

} // namespace Render::Humanoid
