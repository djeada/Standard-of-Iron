#pragma once

// Shared by the humanoid_animation_selection*.cpp translation units; not a public
// header.

#include <QMatrix4x4>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "animation/locomotion_manifest.h"
#include "animation/selection_manifest.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/humanoid/asset/facial_hair_catalog.h"
#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/humanoid/pipeline/humanoid_playback.h"
#include "render/creature/humanoid/schema/skeleton_schema.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/humanoid_animation_selection.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/runtime_bake_guard.h"

namespace Render::Creature::Pipeline {

namespace humanoid_animation_selection_detail {

auto default_humanoid_archetype(Render::Creature::ArchetypeId archetype_id) noexcept
    -> Render::Creature::ArchetypeId;

auto steps_over_ground(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> bool;

void update_clip_id(HumanoidAnimationSelection& selection) noexcept;

void apply_named_sword_attack_state(
    HumanoidAnimationSelection& selection,
    const Render::GL::HumanoidAnimationContext& anim) noexcept;

void apply_authored_action_clip(
    HumanoidAnimationSelection& selection,
    const Render::GL::HumanoidAnimationContext& anim) noexcept;

void apply_melee_reaction_clip(
    HumanoidAnimationSelection& selection,
    const Render::GL::HumanoidAnimationContext& anim) noexcept;

void apply_combat_ready_clip(HumanoidAnimationSelection& selection,
                             const Render::GL::HumanoidAnimationContext& anim,
                             std::uint32_t seed) noexcept;

void apply_construction_clip(HumanoidAnimationSelection& selection,
                             const Render::GL::HumanoidAnimationContext& anim) noexcept;

void apply_showcase_clip(HumanoidAnimationSelection& selection,
                         const Render::GL::HumanoidAnimationContext& anim) noexcept;

[[nodiscard]] constexpr auto melee_variant_for_combo_step(
    Animation::SwordAttackAnimation step) noexcept -> std::uint8_t;

void apply_role_specific_combat_clip(
    HumanoidAnimationSelection& selection,
    const UnitVisualSpec& spec,
    const Render::GL::HumanoidAnimationContext& anim) noexcept;

constexpr float k_formed_shield_hold_phase = 0.0F;

[[nodiscard]] auto
defensive_layout_clip_for(Render::GL::ShieldFormationPose pose) -> std::uint16_t;

auto guard_shield_turn(Render::GL::ShieldFormationPose pose) -> QMatrix4x4;

auto guard_pose_suffix(Render::GL::ShieldFormationPose pose) -> const char*;

auto guard_shield_archetype(Render::Creature::ArchetypeId base_archetype,
                            Render::GL::ShieldFormationPose pose)
    -> Render::Creature::ArchetypeId;

auto build_selection_for_pose(const UnitVisualSpec& spec,
                              const Render::GL::HumanoidAnimationContext& anim,
                              const Render::Creature::ResolvedPose& pose,
                              std::uint32_t seed,
                              const Render::GL::HumanoidVariant* variant) noexcept
    -> HumanoidAnimationSelection;

auto playback_layer_from_selection(const HumanoidAnimationSelection& selection,
                                   float weight,
                                   Render::Creature::PlaybackLayerMode mode) noexcept
    -> HumanoidPlaybackLayerSelection;

auto selection_from_layer_source(Animation::PlaybackLayerSource source,
                                 const HumanoidAnimationSelection& base,
                                 const HumanoidAnimationSelection& action) noexcept
    -> const HumanoidAnimationSelection*;

auto locomotion_only_context(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> Render::GL::HumanoidAnimationContext;

auto action_only_context(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> Render::GL::HumanoidAnimationContext;

} // namespace humanoid_animation_selection_detail

} // namespace Render::Creature::Pipeline
