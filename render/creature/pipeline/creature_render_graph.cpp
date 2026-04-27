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

namespace Render::Creature::Pipeline {

namespace {

constexpr float kHumanoidFullDistance = 10.0F;
constexpr float kHumanoidMinimalDistance = 40.0F;

constexpr float kHorseFullDistance = 20.0F;
constexpr float kHorseMinimalDistance = 60.0F;

constexpr float kElephantFullDistance = 20.0F;
constexpr float kElephantMinimalDistance = 60.0F;

constexpr float kTemporalMinimalDistance = 45.0F;
constexpr std::uint32_t kTemporalMinimalPeriod = 3;

} // namespace

auto humanoid_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = kHumanoidFullDistance;
  config.thresholds.minimal = kHumanoidMinimalDistance;
  config.temporal.distance_minimal = kTemporalMinimalDistance;
  config.temporal.period_minimal = kTemporalMinimalPeriod;
  config.apply_visibility_budget = false;
  return config;
}

auto horse_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = kHorseFullDistance;
  config.thresholds.minimal = kHorseMinimalDistance;
  config.temporal.distance_minimal = kTemporalMinimalDistance;
  config.temporal.period_minimal = kTemporalMinimalPeriod;
  config.apply_visibility_budget = false;
  return config;
}

auto elephant_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = kElephantFullDistance;
  config.thresholds.minimal = kElephantMinimalDistance;
  config.temporal.distance_minimal = kTemporalMinimalDistance;
  config.temporal.period_minimal = kTemporalMinimalPeriod;
  config.apply_visibility_budget = false;
  return config;
}

auto humanoid_lod_config_from_settings() noexcept -> CreatureLodConfig {
  const auto &gs = Render::GraphicsSettings::instance();
  CreatureLodConfig config;
  config.thresholds.full = gs.humanoid_full_detail_distance();
  config.thresholds.minimal = gs.humanoid_minimal_detail_distance();
  config.temporal.distance_minimal = kTemporalMinimalDistance;
  config.temporal.period_minimal = kTemporalMinimalPeriod;
  config.apply_visibility_budget = gs.visibility_budget().enabled;
  return config;
}

auto horse_lod_config_from_settings() noexcept -> CreatureLodConfig {
  const auto &gs = Render::GraphicsSettings::instance();
  CreatureLodConfig config;
  config.thresholds.full = gs.horse_full_detail_distance();
  config.thresholds.minimal = gs.horse_minimal_detail_distance();
  config.temporal.distance_minimal = kTemporalMinimalDistance;
  config.temporal.period_minimal = kTemporalMinimalPeriod;
  config.apply_visibility_budget = gs.visibility_budget().enabled;
  return config;
}

auto elephant_lod_config_from_settings() noexcept -> CreatureLodConfig {
  const auto &gs = Render::GraphicsSettings::instance();
  CreatureLodConfig config;
  config.thresholds.full = gs.elephant_full_detail_distance();
  config.thresholds.minimal = gs.elephant_minimal_detail_distance();
  config.temporal.distance_minimal = kTemporalMinimalDistance;
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
  case 5U:
    return Render::Creature::AnimationStateId::AttackMelee;
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
  case 3U:
    return Render::Creature::AnimationStateId::AttackMelee;
  default:
    return Render::Creature::AnimationStateId::Idle;
  }
}

[[nodiscard]] auto
compose_world_key(std::uint32_t entity_id,
                  std::uint32_t seed) noexcept -> Render::Creature::WorldKey {
  auto const key = (static_cast<Render::Creature::WorldKey>(entity_id) << 32U) |
                   static_cast<Render::Creature::WorldKey>(seed);
  return key != 0U ? key : Render::Creature::WorldKey{1U};
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
  req.world_key = compose_world_key(req.entity_id, req.seed);
  req.lod = output.lod;
  req.pass = output.pass_intent;
  req.world_already_grounded = output.world_already_grounded;
  return req;
}

template <typename Variant>
void populate_role_colors(Render::Creature::CreatureRenderRequest &req,
                          const Variant &variant) {
  const auto *asset =
      Render::Creature::Pipeline::CreatureAssetRegistry::instance().get(
          req.creature_asset_id);
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
  float const phase = humanoid_phase_for_anim(anim);
  std::uint8_t const clip_var = humanoid_clip_variant_for_anim(anim);
  const auto *asset = CreatureAssetRegistry::instance().resolve(output.spec);
  if (asset == nullptr) {
    return;
  }

  if (output.pass_intent == RenderPassIntent::Shadow) {
    auto row = make_prepared_humanoid_row(
        output.spec, pose, variant, anim, output.world_matrix, output.seed,
        output.lod, output.entity_id, output.pass_intent);
    rows_.push_back(std::move(row));
  }

  auto req = build_request(output, archetype_id, state, phase);
  req.creature_asset_id = asset->id;
  req.clip_variant = clip_var;
  populate_role_colors(req, variant);
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
  const auto *asset =
      CreatureAssetRegistry::instance().for_species(CreatureKind::Horse);
  if (asset == nullptr) {
    return;
  }
  if (output.pass_intent == RenderPassIntent::Shadow) {
    auto row = make_prepared_horse_row(
        output.spec, pose, variant, output.world_matrix, output.seed,
        output.lod, output.entity_id, output.pass_intent);
    rows_.push_back(std::move(row));
  }
  auto const archetype_id =
      (output.spec.archetype_id != Render::Creature::kInvalidArchetype)
          ? output.spec.archetype_id
          : Render::Creature::ArchetypeRegistry::kHorseBase;
  auto req = build_request(output, archetype_id,
                           horse_state_for_clip(bpat_clip_id), bpat_phase);
  req.creature_asset_id = asset->id;
  populate_role_colors(req, variant);
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
  const auto *asset =
      CreatureAssetRegistry::instance().for_species(CreatureKind::Elephant);
  if (asset == nullptr) {
    return;
  }
  if (output.pass_intent == RenderPassIntent::Shadow) {
    auto row = make_prepared_elephant_row(
        output.spec, pose, variant, output.world_matrix, output.seed,
        output.lod, output.entity_id, output.pass_intent);
    rows_.push_back(std::move(row));
  }
  auto const archetype_id =
      (output.spec.archetype_id != Render::Creature::kInvalidArchetype)
          ? output.spec.archetype_id
          : Render::Creature::ArchetypeRegistry::kElephantBase;
  auto req = build_request(output, archetype_id,
                           elephant_state_for_clip(bpat_clip_id), bpat_phase);
  req.creature_asset_id = asset->id;
  populate_role_colors(req, variant);
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
  return std::max(rows_.size(), requests_.size());
}

auto CreatureRenderBatch::empty() const noexcept -> bool {
  return rows_.empty() && requests_.empty();
}

} // namespace Render::Creature::Pipeline
