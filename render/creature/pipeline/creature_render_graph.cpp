#include "creature_render_graph.h"

#include "../../entity/registry.h"
#include "../../gl/humanoid/humanoid_types.h"
#include "../../graphics_settings.h"
#include "preparation_common.h"

#include "../../../game/core/component.h"
#include "../../../game/core/entity.h"

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
  // Elephants use the same settings as horses for LOD (they're both large mounts)
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

  // Derive instance seed for temporal skipping
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

  // Derive render pass intent
  if (inputs.ctx != nullptr) {
    output.pass_intent = pass_intent_from_ctx(*inputs.ctx);
  }

  // Derive instance seed
  if (inputs.ctx != nullptr) {
    output.seed = derive_unit_seed(*inputs.ctx, inputs.unit);
  }

  // Copy world matrix from context
  if (inputs.ctx != nullptr) {
    output.world_matrix = inputs.ctx->model;
  }

  // Set entity ID
  if (inputs.entity != nullptr) {
    output.entity_id = static_cast<EntityId>(
        reinterpret_cast<std::uintptr_t>(inputs.entity) & 0xFFFFFFFFU);
  }

  return output;
}

// CreatureRenderBatch implementation

void CreatureRenderBatch::clear() noexcept {
  rows_.clear();
  legacy_contexts_.clear();
}

void CreatureRenderBatch::reserve(std::size_t n) { rows_.reserve(n); }

void CreatureRenderBatch::add_humanoid(
    const CreatureGraphOutput &output, const Render::GL::HumanoidPose &pose,
    const Render::GL::HumanoidVariant &variant,
    const Render::GL::HumanoidAnimationContext &anim,
    const Render::GL::DrawContext *legacy_ctx) {
  if (output.culled) {
    return;
  }
  const Render::GL::DrawContext *stored_ctx = legacy_ctx;
  if (legacy_ctx != nullptr) {
    legacy_contexts_.push_back(*legacy_ctx);
    stored_ctx = &legacy_contexts_.back();
  }
  rows_.push_back(make_prepared_humanoid_row(
      output.spec, pose, variant, anim, output.world_matrix, output.seed,
      output.lod, stored_ctx, output.entity_id, output.pass_intent));
}

void CreatureRenderBatch::add_horse(
    const CreatureGraphOutput &output,
    const Render::Horse::HorseSpecPose &pose,
    const Render::GL::HorseVariant &variant) {
  if (output.culled) {
    return;
  }
  rows_.push_back(make_prepared_horse_row(output.spec, pose, variant,
                                          output.world_matrix, output.seed,
                                          output.lod, output.entity_id,
                                          output.pass_intent));
}

void CreatureRenderBatch::add_elephant(
    const CreatureGraphOutput &output,
    const Render::Elephant::ElephantSpecPose &pose,
    const Render::GL::ElephantVariant &variant) {
  if (output.culled) {
    return;
  }
  rows_.push_back(make_prepared_elephant_row(output.spec, pose, variant,
                                             output.world_matrix, output.seed,
                                             output.lod, output.entity_id,
                                             output.pass_intent));
}

auto CreatureRenderBatch::rows() const noexcept
    -> std::span<const PreparedCreatureRenderRow> {
  return rows_;
}

auto CreatureRenderBatch::size() const noexcept -> std::size_t {
  return rows_.size();
}

auto CreatureRenderBatch::empty() const noexcept -> bool {
  return rows_.empty();
}

} // namespace Render::Creature::Pipeline
