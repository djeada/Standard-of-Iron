#include "creature_render_graph.h"

#include "../../elephant/elephant_spec.h"
#include "../../entity/registry.h"
#include "../../gl/humanoid/humanoid_types.h"
#include "../../graphics_settings.h"
#include "../../horse/horse_spec.h"
#include "../../humanoid/humanoid_spec.h"
#include "../archetype_registry.h"
#include "../bpat/bpat_format.h"
#include "../bpat/bpat_registry.h"
#include "creature_asset.h"
#include "preparation_common.h"

#include "../../../game/core/component.h"
#include "../../../game/core/entity.h"

#include <algorithm>
#include <cmath>

namespace Render::Creature::Pipeline {

namespace {

constexpr float kHumanoidFullDistance = 10.0F;
constexpr float kHumanoidReducedDistance = 20.0F;
constexpr float kHumanoidMinimalDistance = 40.0F;

constexpr float kHorseFullDistance = 20.0F;
constexpr float kHorseReducedDistance = 40.0F;
constexpr float kHorseMinimalDistance = 60.0F;

constexpr float kElephantFullDistance = 20.0F;
constexpr float kElephantReducedDistance = 40.0F;
constexpr float kElephantMinimalDistance = 60.0F;

constexpr float kTemporalReducedDistance = 35.0F;
constexpr float kTemporalMinimalDistance = 45.0F;
constexpr std::uint32_t kTemporalReducedPeriod = 2;
constexpr std::uint32_t kTemporalMinimalPeriod = 3;

} // namespace

auto humanoid_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = kHumanoidFullDistance;
  config.thresholds.reduced = kHumanoidReducedDistance;
  config.thresholds.minimal = kHumanoidMinimalDistance;
  config.temporal.distance_reduced = kTemporalReducedDistance;
  config.temporal.distance_minimal = kTemporalMinimalDistance;
  config.temporal.period_reduced = kTemporalReducedPeriod;
  config.temporal.period_minimal = kTemporalMinimalPeriod;
  config.apply_visibility_budget = false;
  return config;
}

auto horse_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = kHorseFullDistance;
  config.thresholds.reduced = kHorseReducedDistance;
  config.thresholds.minimal = kHorseMinimalDistance;
  config.temporal.distance_reduced = kTemporalReducedDistance;
  config.temporal.distance_minimal = kTemporalMinimalDistance;
  config.temporal.period_reduced = kTemporalReducedPeriod;
  config.temporal.period_minimal = kTemporalMinimalPeriod;
  config.apply_visibility_budget = false;
  return config;
}

auto elephant_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = kElephantFullDistance;
  config.thresholds.reduced = kElephantReducedDistance;
  config.thresholds.minimal = kElephantMinimalDistance;
  config.temporal.distance_reduced = kTemporalReducedDistance;
  config.temporal.distance_minimal = kTemporalMinimalDistance;
  config.temporal.period_reduced = kTemporalReducedPeriod;
  config.temporal.period_minimal = kTemporalMinimalPeriod;
  config.apply_visibility_budget = false;
  return config;
}

auto humanoid_lod_config_from_settings() noexcept -> CreatureLodConfig {
  const auto &gs = Render::GraphicsSettings::instance();
  CreatureLodConfig config;
  config.thresholds.full = gs.humanoid_full_detail_distance();
  config.thresholds.reduced = gs.humanoid_reduced_detail_distance();
  config.thresholds.minimal = gs.humanoid_minimal_detail_distance();
  config.temporal.distance_reduced = kTemporalReducedDistance;
  config.temporal.distance_minimal = kTemporalMinimalDistance;
  config.temporal.period_reduced = kTemporalReducedPeriod;
  config.temporal.period_minimal = kTemporalMinimalPeriod;
  config.apply_visibility_budget = gs.visibility_budget().enabled;
  return config;
}

auto horse_lod_config_from_settings() noexcept -> CreatureLodConfig {
  const auto &gs = Render::GraphicsSettings::instance();
  CreatureLodConfig config;
  config.thresholds.full = gs.horse_full_detail_distance();
  config.thresholds.reduced = gs.horse_reduced_detail_distance();
  config.thresholds.minimal = gs.horse_minimal_detail_distance();
  config.temporal.distance_reduced = kTemporalReducedDistance;
  config.temporal.distance_minimal = kTemporalMinimalDistance;
  config.temporal.period_reduced = kTemporalReducedPeriod;
  config.temporal.period_minimal = kTemporalMinimalPeriod;
  config.apply_visibility_budget = gs.visibility_budget().enabled;
  return config;
}

auto elephant_lod_config_from_settings() noexcept -> CreatureLodConfig {

  const auto &gs = Render::GraphicsSettings::instance();
  CreatureLodConfig config;
  config.thresholds.full = gs.horse_full_detail_distance();
  config.thresholds.reduced = gs.horse_reduced_detail_distance();
  config.thresholds.minimal = gs.horse_minimal_detail_distance();
  config.temporal.distance_reduced = kTemporalReducedDistance;
  config.temporal.distance_minimal = kTemporalMinimalDistance;
  config.temporal.period_reduced = kTemporalReducedPeriod;
  config.temporal.period_minimal = kTemporalMinimalPeriod;
  config.apply_visibility_budget = gs.visibility_budget().enabled;
  return config;
}

auto evaluate_creature_lod(const CreatureGraphInputs &inputs,
                           const CreatureLodConfig &config) noexcept
    -> CreatureLodDecision {
  CreatureLodDecisionInputs lod_inputs;
  lod_inputs.forced_lod = inputs.forced_lod;
  lod_inputs.has_camera = inputs.has_camera;
  lod_inputs.distance = inputs.camera_distance;
  lod_inputs.thresholds = config.thresholds;
  lod_inputs.apply_visibility_budget = config.apply_visibility_budget;
  lod_inputs.budget_grant_full = inputs.budget_grant_full;
  lod_inputs.temporal = config.temporal;
  lod_inputs.frame_index = inputs.frame_index;

  std::uint32_t seed = 0U;
  if (inputs.ctx != nullptr) {
    seed = derive_unit_seed(*inputs.ctx, inputs.unit);
  }
  lod_inputs.instance_seed = seed;

  return decide_creature_lod(lod_inputs);
}

auto build_base_graph_output(const CreatureGraphInputs &inputs,
                             const CreatureLodDecision &lod_decision) noexcept
    -> CreatureGraphOutput {
  CreatureGraphOutput output;
  output.lod = lod_decision.lod;
  output.culled = lod_decision.culled;
  output.cull_reason = lod_decision.reason;

  if (inputs.ctx != nullptr) {
    output.pass_intent = pass_intent_from_ctx(*inputs.ctx);
  }

  if (inputs.ctx != nullptr) {
    output.seed = derive_unit_seed(*inputs.ctx, inputs.unit);
  }

  if (inputs.ctx != nullptr) {
    output.world_matrix = inputs.ctx->model;
  }

  if (inputs.entity != nullptr) {
    output.entity_id = static_cast<EntityId>(
        reinterpret_cast<std::uintptr_t>(inputs.entity) & 0xFFFFFFFFU);
  }

  return output;
}

void CreatureRenderBatch::clear() noexcept {
  rows_.clear();
  requests_.clear();
}

void CreatureRenderBatch::reserve(std::size_t n) {
  rows_.reserve(n);
  requests_.reserve(n);
}

namespace {

auto resolve_playback(std::uint32_t species_id, std::uint16_t clip_id,
                      float phase) noexcept -> BpatPlayback {
  BpatPlayback pb{};
  if (clip_id == kInvalidBpatClip) {
    return pb;
  }
  const auto *blob =
      Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
  if (blob == nullptr || clip_id >= blob->clip_count()) {
    return pb;
  }
  auto const c = blob->clip(clip_id);
  if (c.frame_count == 0U) {
    return pb;
  }
  float p = phase - std::floor(phase);
  if (!c.loops && phase >= 1.0F) {
    pb.clip_id = clip_id;
    pb.frame_in_clip = static_cast<std::uint16_t>(c.frame_count - 1U);
    return pb;
  }
  if (p < 0.0F) {
    p += 1.0F;
  }
  auto const fc = static_cast<float>(c.frame_count);
  auto frame_idx = static_cast<int>(p * fc);
  if (frame_idx < 0) {
    frame_idx = 0;
  }
  if (frame_idx >= static_cast<int>(c.frame_count)) {
    frame_idx = static_cast<int>(c.frame_count) - 1;
  }
  pb.clip_id = clip_id;
  pb.frame_in_clip = static_cast<std::uint16_t>(frame_idx);
  return pb;
}

[[nodiscard]] auto humanoid_state_for_anim(
    const Render::GL::HumanoidAnimationContext &anim) noexcept
    -> Render::Creature::AnimationStateId {
  if (anim.inputs.is_in_hold_mode || anim.inputs.is_exiting_hold) {
    return Render::Creature::AnimationStateId::Hold;
  }
  if (anim.is_attacking()) {
    if (anim.inputs.is_melee) {
      return Render::Creature::AnimationStateId::AttackSword;
    }
    return Render::Creature::AnimationStateId::AttackBow;
  }
  switch (anim.motion_state) {
  case Render::GL::HumanoidMotionState::Idle:
    return Render::Creature::AnimationStateId::Idle;
  case Render::GL::HumanoidMotionState::Walk:
    return Render::Creature::AnimationStateId::Walk;
  case Render::GL::HumanoidMotionState::Run:
    return Render::Creature::AnimationStateId::Run;
  case Render::GL::HumanoidMotionState::Hold:
    return Render::Creature::AnimationStateId::Hold;
  default:
    return Render::Creature::AnimationStateId::Idle;
  }
}

[[nodiscard]] auto horse_state_for_clip(std::uint16_t clip) noexcept
    -> Render::Creature::AnimationStateId {
  switch (clip) {
  case 0U:
    return Render::Creature::AnimationStateId::Idle;
  case 1U:
    return Render::Creature::AnimationStateId::Walk;
  case 2U:
  case 3U:
  case 4U:
    return Render::Creature::AnimationStateId::Run;
  default:
    return Render::Creature::AnimationStateId::Idle;
  }
}

[[nodiscard]] auto elephant_state_for_clip(std::uint16_t clip) noexcept
    -> Render::Creature::AnimationStateId {
  switch (clip) {
  case 0U:
    return Render::Creature::AnimationStateId::Idle;
  case 1U:
    return Render::Creature::AnimationStateId::Walk;
  case 2U:
    return Render::Creature::AnimationStateId::Run;
  default:
    return Render::Creature::AnimationStateId::Idle;
  }
}

[[nodiscard]] auto
build_request(const CreatureGraphOutput &output,
              Render::Creature::ArchetypeId archetype,
              Render::Creature::AnimationStateId state,
              float phase) noexcept -> Render::Creature::CreatureRenderRequest {
  Render::Creature::CreatureRenderRequest req;
  req.archetype = archetype;
  req.variant = Render::Creature::kCanonicalVariant;
  req.state = state;
  req.phase = phase;
  req.world = output.world_matrix;
  req.entity_id = static_cast<std::uint32_t>(output.entity_id);
  req.seed = output.seed;
  req.lod = output.lod;
  return req;
}

template <typename Variant>
void populate_role_colors(Render::Creature::CreatureRenderRequest &req,
                          Render::Creature::Pipeline::CreatureKind kind,
                          const Variant &variant) {
  const auto *asset =
      Render::Creature::Pipeline::CreatureAssetRegistry::instance().for_species(
          kind);
  std::uint32_t count = 0;
  if (asset != nullptr && asset->fill_role_colors != nullptr) {
    count =
        asset->fill_role_colors(static_cast<const void *>(&variant),
                                req.role_colors.data(), req.role_colors.size());
  }

  const auto *desc =
      Render::Creature::ArchetypeRegistry::instance().get(req.archetype);
  if (desc != nullptr) {
    for (std::size_t i = 0;
         i < static_cast<std::size_t>(desc->extra_role_color_fn_count); ++i) {
      auto const fn = desc->extra_role_color_fns[i];
      if (fn == nullptr) {
        continue;
      }
      count = fn(static_cast<const void *>(&variant), req.role_colors.data(),
                 count, req.role_colors.size());
    }
  }
  req.role_color_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(
      count, static_cast<std::uint32_t>(req.role_colors.size())));
}

[[nodiscard]] auto
is_rider_archetype(Render::Creature::ArchetypeId archetype) noexcept -> bool {
  return archetype == Render::Creature::ArchetypeRegistry::kRiderBase;
}

} // namespace

void CreatureRenderBatch::add_humanoid(
    const CreatureGraphOutput &output, const Render::GL::HumanoidPose &pose,
    const Render::GL::HumanoidVariant &variant,
    const Render::GL::HumanoidAnimationContext &anim) {
  if (output.culled) {
    return;
  }

  auto const archetype_id =
      (output.spec.archetype_id != Render::Creature::kInvalidArchetype)
          ? output.spec.archetype_id
          : Render::Creature::ArchetypeRegistry::kHumanoidBase;

  auto const state = humanoid_state_for_anim(anim);
  bool const is_attack_state =
      (state == Render::Creature::AnimationStateId::AttackSword ||
       state == Render::Creature::AnimationStateId::AttackSpear ||
       state == Render::Creature::AnimationStateId::AttackBow);
  float const phase =
      is_attack_state ? anim.attack_phase : anim.gait.cycle_phase;
  std::uint8_t const clip_var =
      (is_attack_state &&
       state != Render::Creature::AnimationStateId::AttackBow)
          ? std::min<std::uint8_t>(anim.inputs.attack_variant, 2U)
          : 0U;

  auto const base_clip =
      Render::Creature::ArchetypeRegistry::instance().bpat_clip(archetype_id,
                                                                state);
  std::uint16_t const clip =
      (base_clip != Render::Creature::ArchetypeDescriptor::kUnmappedClip)
          ? static_cast<std::uint16_t>(base_clip + clip_var)
          : kInvalidBpatClip;

  auto row = make_prepared_humanoid_row(
      output.spec, pose, variant, anim, output.world_matrix, output.seed,
      output.lod, output.entity_id, output.pass_intent);
  row.bpat_playback =
      resolve_playback(Render::Creature::Bpat::kSpeciesHumanoid, clip, phase);
  rows_.push_back(std::move(row));
  auto req = build_request(output, archetype_id, state, phase);
  req.clip_variant = clip_var;
  if (is_rider_archetype(archetype_id)) {
    req.parent_entity_id = static_cast<std::uint32_t>(output.entity_id);
  }
  populate_role_colors(req, CreatureKind::Humanoid, variant);
  requests_.push_back(req);
}

void CreatureRenderBatch::add_horse(const CreatureGraphOutput &output,
                                    const Render::Horse::HorseSpecPose &pose,
                                    const Render::GL::HorseVariant &variant,
                                    std::uint16_t bpat_clip_id,
                                    float bpat_phase) {
  if (output.culled) {
    return;
  }
  auto row = make_prepared_horse_row(
      output.spec, pose, variant, output.world_matrix, output.seed, output.lod,
      output.entity_id, output.pass_intent);
  row.bpat_playback = resolve_playback(Render::Creature::Bpat::kSpeciesHorse,
                                       bpat_clip_id, bpat_phase);
  rows_.push_back(std::move(row));
  auto const archetype_id =
      (output.spec.archetype_id != Render::Creature::kInvalidArchetype)
          ? output.spec.archetype_id
          : Render::Creature::ArchetypeRegistry::kHorseBase;
  auto req = build_request(output, archetype_id,
                           horse_state_for_clip(bpat_clip_id), bpat_phase);
  populate_role_colors(req, CreatureKind::Horse, variant);
  requests_.push_back(req);
}

void CreatureRenderBatch::add_elephant(
    const CreatureGraphOutput &output,
    const Render::Elephant::ElephantSpecPose &pose,
    const Render::GL::ElephantVariant &variant, std::uint16_t bpat_clip_id,
    float bpat_phase) {
  if (output.culled) {
    return;
  }
  auto row = make_prepared_elephant_row(
      output.spec, pose, variant, output.world_matrix, output.seed, output.lod,
      output.entity_id, output.pass_intent);
  row.bpat_playback = resolve_playback(Render::Creature::Bpat::kSpeciesElephant,
                                       bpat_clip_id, bpat_phase);
  rows_.push_back(std::move(row));
  auto const archetype_id =
      (output.spec.archetype_id != Render::Creature::kInvalidArchetype)
          ? output.spec.archetype_id
          : Render::Creature::ArchetypeRegistry::kElephantBase;
  auto req = build_request(output, archetype_id,
                           elephant_state_for_clip(bpat_clip_id), bpat_phase);
  populate_role_colors(req, CreatureKind::Elephant, variant);
  requests_.push_back(req);
}

void CreatureRenderBatch::add_request(
    const Render::Creature::CreatureRenderRequest &request) {
  requests_.push_back(request);
}

auto CreatureRenderBatch::rows() const noexcept
    -> std::span<const PreparedCreatureRenderRow> {
  return rows_;
}

auto CreatureRenderBatch::requests() const noexcept
    -> std::span<const Render::Creature::CreatureRenderRequest> {
  return requests_;
}

auto CreatureRenderBatch::size() const noexcept -> std::size_t {
  return rows_.size();
}

auto CreatureRenderBatch::empty() const noexcept -> bool {
  return rows_.empty();
}

} // namespace Render::Creature::Pipeline
