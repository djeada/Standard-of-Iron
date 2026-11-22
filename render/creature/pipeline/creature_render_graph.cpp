#include "render/creature/pipeline/creature_render_graph.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_registry.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_render_batch_support.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/frame/draw_context.h"
#include "render/frame/graphics_settings.h"
#include "render/profiling/frame_profile.h"

namespace Render::Creature::Pipeline {

namespace {

constexpr float k_humanoid_full_distance = 10.0F;
constexpr float k_horse_full_distance = 20.0F;
constexpr float k_elephant_full_distance = 20.0F;
constexpr float k_authored_cull_distance = 200.0F;

[[nodiscard]] auto
lod_config_from_settings(float scaled_full_distance) noexcept -> CreatureLodConfig {
  const auto& lod = Render::GraphicsSettings::instance().creature_lod();
  CreatureLodConfig config;
  config.thresholds.full =
      lod.enabled ? scaled_full_distance : std::numeric_limits<float>::max() / 4.0F;
  config.thresholds.cull = lod.cull_distance;
  config.apply_visibility_budget = lod.enabled && lod.visibility_budget;
  return config;
}

} // namespace

auto humanoid_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = k_humanoid_full_distance;
  config.thresholds.cull = k_authored_cull_distance;
  config.apply_visibility_budget = false;
  return config;
}

auto horse_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = k_horse_full_distance;
  config.thresholds.cull = k_authored_cull_distance;
  config.apply_visibility_budget = false;
  return config;
}

auto elephant_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = k_elephant_full_distance;
  config.thresholds.cull = k_authored_cull_distance;
  config.apply_visibility_budget = false;
  return config;
}

auto humanoid_lod_config_from_settings() noexcept -> CreatureLodConfig {
  return lod_config_from_settings(
      Render::GraphicsSettings::instance().humanoid_full_detail_distance());
}

auto horse_lod_config_from_settings() noexcept -> CreatureLodConfig {
  return lod_config_from_settings(
      Render::GraphicsSettings::instance().horse_full_detail_distance());
}

auto elephant_lod_config_from_settings() noexcept -> CreatureLodConfig {
  return lod_config_from_settings(
      Render::GraphicsSettings::instance().elephant_full_detail_distance());
}

auto creature_lod_config(CreatureKind kind) noexcept -> CreatureLodConfig {
  switch (kind) {
  case CreatureKind::Horse:
  case CreatureKind::Sheep:
  case CreatureKind::Wolf:
    return horse_lod_config_from_settings();
  case CreatureKind::Elephant:
    return elephant_lod_config_from_settings();
  case CreatureKind::Humanoid:
  case CreatureKind::Mounted:
    break;
  }
  return humanoid_lod_config_from_settings();
}

auto quadruped_lod_from_settings(CreatureKind kind, float distance) noexcept
    -> Render::Creature::CreatureLOD {
  return select_distance_lod(distance, creature_lod_config(kind).thresholds);
}

auto evaluate_creature_lod(const CreatureGraphInputs& inputs,
                           const CreatureLodConfig& config) noexcept
    -> CreatureLodDecision {
  CreatureLodDecisionInputs lod_inputs;
  lod_inputs.forced_lod = inputs.forced_lod;
  lod_inputs.has_camera = inputs.has_camera;
  lod_inputs.distance = inputs.camera_distance;
  if (inputs.ctx != nullptr) {
    lod_inputs.apparent_size_scale = inputs.ctx->screen_metrics.apparent_size_scale();
  }
  lod_inputs.thresholds = config.thresholds;
  lod_inputs.apply_visibility_budget = config.apply_visibility_budget;
  lod_inputs.budget_grant_full = inputs.budget_grant_full;
  return decide_creature_lod(lod_inputs);
}

auto build_base_graph_output(const CreatureGraphInputs& inputs,
                             const CreatureLodDecision& lod_decision) noexcept
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
    output.entity_id = static_cast<EntityId>(inputs.entity->get_id());
  }

  return output;
}

void CreatureRenderBatch::clear() noexcept {
  rows_.clear();
  requests_.clear();
  reuse_cache_ = {};
}

void CreatureRenderBatch::reserve(std::size_t n) {
  rows_.reserve(n);
  requests_.reserve(n);
}

namespace {

[[nodiscard]] auto
compose_world_key(std::uint32_t entity_id,
                  std::uint32_t seed) noexcept -> Render::Creature::WorldKey {
  auto const key = (static_cast<Render::Creature::WorldKey>(entity_id) << 32U) |
                   static_cast<Render::Creature::WorldKey>(seed);
  return key != 0U ? key : Render::Creature::WorldKey{1U};
}

void populate_role_colors_uncached(Render::Creature::CreatureRenderRequest& req,
                                   const void* variant) {
  auto palette = std::make_shared<Render::RoleColorPalette>();
  const auto* asset = Render::Creature::Pipeline::CreatureAssetRegistry::instance().get(
      req.creature_asset_id);
  std::uint32_t count = 0;
  if (asset != nullptr && asset->fill_role_colors != nullptr) {
    count = asset->fill_role_colors(
        variant, palette->colors.data(), palette->colors.size());
  }

  const auto* desc = Render::Creature::ArchetypeRegistry::instance().get(req.archetype);
  if (desc != nullptr) {
    for (std::size_t i = 0;
         i < static_cast<std::size_t>(desc->extra_role_color_fn_count);
         ++i) {
      auto const fn = desc->extra_role_color_fns[i];
      if (fn == nullptr) {
        continue;
      }
      count = fn(variant, palette->colors.data(), count, palette->colors.size());
    }
  }
  palette->count = static_cast<std::uint8_t>(std::min<std::uint32_t>(
      count, static_cast<std::uint32_t>(palette->colors.size())));
  req.role_color_count = palette->count;
  req.role_colors = std::move(palette);
}

} // namespace

namespace BatchSupport {

auto build_request(const CreatureGraphOutput& output,
                   Render::Creature::ArchetypeId archetype,
                   Render::Creature::AnimationStateId state,
                   float phase) noexcept -> Render::Creature::CreatureRenderRequest {
  Render::Creature::CreatureRenderRequest req;
  req.archetype = archetype;
  req.variant = Render::Creature::k_canonical_variant;
  req.state = state;
  req.phase = phase;
  req.world = output.world_matrix;
  req.entity_id = static_cast<std::uint32_t>(output.entity_id);
  req.instance_index = output.instance_index;
  req.seed = output.seed;
  req.world_key = compose_world_key(req.entity_id, req.seed);
  req.lod = output.lod;
  req.pass = output.pass_intent;
  req.world_already_grounded = output.world_already_grounded;
  return req;
}

auto default_archetype_for(CreatureKind kind) noexcept
    -> Render::Creature::ArchetypeId {
  switch (kind) {
  case CreatureKind::Horse:
    return Render::Creature::ArchetypeRegistry::k_horse_base;
  case CreatureKind::Elephant:
    return Render::Creature::ArchetypeRegistry::k_elephant_base;
  case CreatureKind::Sheep:
    return Render::Creature::ArchetypeRegistry::k_sheep_base;
  case CreatureKind::Wolf:
    return Render::Creature::ArchetypeRegistry::k_wolf_base;
  case CreatureKind::Humanoid:
    return Render::Creature::ArchetypeRegistry::k_humanoid_base;
  case CreatureKind::Mounted:
    return Render::Creature::k_invalid_archetype;
  }
  return Render::Creature::k_invalid_archetype;
}

void populate_role_colors(RoleColorCache& cache,
                          Render::Creature::CreatureRenderRequest& req,
                          const void* variant,
                          std::uint64_t variant_hash) {
  RoleColorCacheKey key{};
  key.asset = req.creature_asset_id;
  key.archetype = req.archetype;
  key.variant_hash = variant_hash;

  if (auto it = cache.find(key); it != cache.end()) {
    req.role_colors = it->second;
    req.role_color_count = it->second->count;
    return;
  }

  populate_role_colors_uncached(req, variant);

  constexpr std::size_t k_max_role_color_cache_entries = 8192;
  if (cache.size() >= k_max_role_color_cache_entries) {
    cache.clear();
  }
  cache.emplace(key, req.role_colors);
}

auto build_quadruped_body(CreatureRenderBatch& batch,
                          const CreatureGraphOutput& output,
                          CreatureKind kind,
                          Render::Creature::AnimationStateId state,
                          float phase,
                          std::uint32_t clip_variant,
                          bool profile_asset_lookup,
                          Render::Creature::CreatureRenderRequest& out) -> bool {
  if (output.culled) {
    return false;
  }
  const auto* asset = CreatureAssetRegistry::instance().for_species(kind);
  if (asset == nullptr) {
    return false;
  }
  if (output.pass_intent == RenderPassIntent::Shadow) {
    batch.add_row(make_prepared_creature_row(output.spec,
                                             kind,
                                             output.world_matrix,
                                             output.seed,
                                             output.lod,
                                             output.entity_id,
                                             output.pass_intent));
  }
  auto const archetype_id =
      (output.spec.archetype_id != Render::Creature::k_invalid_archetype)
          ? output.spec.archetype_id
          : default_archetype_for(kind);
  out = build_request(output, archetype_id, state, phase);
  out.creature_asset_id = asset->id;
  bool created_handle = false;
  if (profile_asset_lookup) {
    auto& profile = Render::Profiling::global_profile();
    Render::Profiling::AccumulatorScope const scope(
        output.pass_intent == RenderPassIntent::Main
            ? &profile.render_asset_cache_lookup_us
            : nullptr);
    out.render_asset_handle =
        CreatureRenderAssetHandleRegistry::instance().get_or_create(
            asset->id, archetype_id, &created_handle);
    if (output.pass_intent == RenderPassIntent::Main) {
      if (created_handle) {
        ++profile.render_asset_cache_misses;
      } else {
        ++profile.render_asset_cache_hits;
      }
    }
  } else {
    out.render_asset_handle =
        CreatureRenderAssetHandleRegistry::instance().get_or_create(
            asset->id, archetype_id, &created_handle);
  }
  out.clip_variant = static_cast<std::uint8_t>(clip_variant);
  return true;
}

} // namespace BatchSupport

void CreatureRenderBatch::fade_requests_from(std::size_t first, float alpha) noexcept {
  if (alpha >= 1.0F) {
    return;
  }
  for (std::size_t index = first; index < requests_.size(); ++index) {
    requests_[index].alpha *= alpha;
  }
}

void CreatureRenderBatch::add_row(PreparedCreatureRenderRow row) {
  rows_.push_back(std::move(row));
}

void CreatureRenderBatch::add_request(
    const Render::Creature::CreatureRenderRequest& request) {
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
