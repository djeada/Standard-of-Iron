#include "creature_render_graph.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>

#include "../../../game/core/component.h"
#include "../../../game/core/entity.h"
#include "../../entity/registry.h"
#include "../../gl/humanoid/humanoid_types.h"
#include "../../graphics_settings.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../profiling/frame_profile.h"
#include "../archetype_registry.h"
#include "../bpat/bpat_format.h"
#include "../bpat/bpat_registry.h"
#include "creature_asset.h"
#include "creature_prepared_state.h"
#include "humanoid_animation_selection.h"
#include "preparation_common.h"

namespace Render::Creature::Pipeline {

namespace {

constexpr float k_humanoid_full_distance = 10.0F;
constexpr float k_humanoid_minimal_distance = 40.0F;

constexpr float k_horse_full_distance = 20.0F;
constexpr float k_horse_minimal_distance = 60.0F;

constexpr float k_elephant_full_distance = 20.0F;
constexpr float k_elephant_minimal_distance = 60.0F;

constexpr float k_temporal_minimal_distance = 45.0F;
constexpr std::uint32_t k_temporal_minimal_period = 3;

constexpr float k_ultra_troop_full_distance = std::numeric_limits<float>::max() / 4.0F;
constexpr float k_ultra_troop_minimal_distance =
    std::numeric_limits<float>::max() / 2.0F;

[[nodiscard]] auto ultra_troop_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = k_ultra_troop_full_distance;
  config.thresholds.minimal = k_ultra_troop_minimal_distance;
  config.temporal.distance_minimal = k_ultra_troop_minimal_distance;
  config.temporal.period_minimal = 1U;
  config.apply_visibility_budget = false;
  return config;
}

} // namespace

auto humanoid_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = k_humanoid_full_distance;
  config.thresholds.minimal = k_humanoid_minimal_distance;
  config.temporal.distance_minimal = k_temporal_minimal_distance;
  config.temporal.period_minimal = k_temporal_minimal_period;
  config.apply_visibility_budget = false;
  return config;
}

auto horse_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = k_horse_full_distance;
  config.thresholds.minimal = k_horse_minimal_distance;
  config.temporal.distance_minimal = k_temporal_minimal_distance;
  config.temporal.period_minimal = k_temporal_minimal_period;
  config.apply_visibility_budget = false;
  return config;
}

auto elephant_lod_config() noexcept -> CreatureLodConfig {
  CreatureLodConfig config;
  config.thresholds.full = k_elephant_full_distance;
  config.thresholds.minimal = k_elephant_minimal_distance;
  config.temporal.distance_minimal = k_temporal_minimal_distance;
  config.temporal.period_minimal = k_temporal_minimal_period;
  config.apply_visibility_budget = false;
  return config;
}

auto humanoid_lod_config_from_settings() noexcept -> CreatureLodConfig {
  const auto& gs = Render::GraphicsSettings::instance();
  if (gs.quality() == Render::GraphicsQuality::Ultra) {
    return ultra_troop_lod_config();
  }

  CreatureLodConfig config;
  config.thresholds.full = gs.humanoid_full_detail_distance();
  config.thresholds.minimal = gs.humanoid_minimal_detail_distance();
  config.temporal.distance_minimal = k_temporal_minimal_distance;
  config.temporal.period_minimal = k_temporal_minimal_period;
  config.apply_visibility_budget = gs.visibility_budget().enabled;
  return config;
}

auto horse_lod_config_from_settings() noexcept -> CreatureLodConfig {
  const auto& gs = Render::GraphicsSettings::instance();
  if (gs.quality() == Render::GraphicsQuality::Ultra) {
    return ultra_troop_lod_config();
  }

  CreatureLodConfig config;
  config.thresholds.full = gs.horse_full_detail_distance();
  config.thresholds.minimal = gs.horse_minimal_detail_distance();
  config.temporal.distance_minimal = k_temporal_minimal_distance;
  config.temporal.period_minimal = k_temporal_minimal_period;
  config.apply_visibility_budget = gs.visibility_budget().enabled;
  return config;
}

auto elephant_lod_config_from_settings() noexcept -> CreatureLodConfig {
  const auto& gs = Render::GraphicsSettings::instance();
  if (gs.quality() == Render::GraphicsQuality::Ultra) {
    return ultra_troop_lod_config();
  }

  CreatureLodConfig config;
  config.thresholds.full = gs.elephant_full_detail_distance();
  config.thresholds.minimal = gs.elephant_minimal_detail_distance();
  config.temporal.distance_minimal = k_temporal_minimal_distance;
  config.temporal.period_minimal = k_temporal_minimal_period;
  config.apply_visibility_budget = gs.visibility_budget().enabled;
  return config;
}

auto quadruped_lod_from_settings(CreatureKind kind, float distance) noexcept
    -> Render::Creature::CreatureLOD {
  switch (kind) {
  case CreatureKind::Horse:
    return select_distance_lod(distance, horse_lod_config_from_settings().thresholds);
  case CreatureKind::Elephant:
    return select_distance_lod(distance,
                               elephant_lod_config_from_settings().thresholds);
  case CreatureKind::Humanoid:
  case CreatureKind::Mounted:
    break;
  }
  return select_distance_lod(distance, humanoid_lod_config_from_settings().thresholds);
}

auto evaluate_creature_lod(const CreatureGraphInputs& inputs,
                           const CreatureLodConfig& config) noexcept
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

[[nodiscard]] auto
build_request(const CreatureGraphOutput& output,
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
  req.seed = output.seed;
  req.world_key = compose_world_key(req.entity_id, req.seed);
  req.lod = output.lod;
  req.pass = output.pass_intent;
  req.world_already_grounded = output.world_already_grounded;
  return req;
}

[[nodiscard]] auto
default_archetype_for(CreatureKind kind) noexcept -> Render::Creature::ArchetypeId {
  switch (kind) {
  case CreatureKind::Horse:
    return Render::Creature::ArchetypeRegistry::k_horse_base;
  case CreatureKind::Elephant:
    return Render::Creature::ArchetypeRegistry::k_elephant_base;
  case CreatureKind::Humanoid:
    return Render::Creature::ArchetypeRegistry::k_humanoid_base;
  case CreatureKind::Mounted:
    return Render::Creature::k_invalid_archetype;
  }
  return Render::Creature::k_invalid_archetype;
}

template <typename Variant>
auto variant_hash(const Variant& variant) noexcept -> std::uint64_t;

[[nodiscard]] auto hash_combine(std::uint64_t seed,
                                std::uint64_t value) noexcept -> std::uint64_t {
  return seed ^ (value + 0x9E3779B97F4A7C15ULL + (seed << 6U) + (seed >> 2U));
}

[[nodiscard]] auto hash_float(float value) noexcept -> std::uint64_t {
  std::uint32_t bits = 0U;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

[[nodiscard]] auto hash_vec3(const QVector3D& v) noexcept -> std::uint64_t {
  std::uint64_t h = 0xCBF29CE484222325ULL;
  h = hash_combine(h, hash_float(v.x()));
  h = hash_combine(h, hash_float(v.y()));
  h = hash_combine(h, hash_float(v.z()));
  return h;
}

[[nodiscard]] auto
hash_palette(const Render::GL::HumanoidPalette& p) noexcept -> std::uint64_t {
  std::uint64_t h = 0x84222325CBF29CE4ULL;
  h = hash_combine(h, hash_vec3(p.cloth));
  h = hash_combine(h, hash_vec3(p.skin));
  h = hash_combine(h, hash_vec3(p.leather));
  h = hash_combine(h, hash_vec3(p.leather_dark));
  h = hash_combine(h, hash_vec3(p.wood));
  h = hash_combine(h, hash_vec3(p.metal));
  return h;
}

template <>
auto variant_hash(const Render::GL::HumanoidVariant& variant) noexcept
    -> std::uint64_t {
  std::uint64_t h = hash_palette(variant.palette);
  h = hash_combine(h, static_cast<std::uint64_t>(variant.facial_hair.style));
  h = hash_combine(h, hash_vec3(variant.facial_hair.color));
  h = hash_combine(h, hash_float(variant.facial_hair.length));
  h = hash_combine(h, hash_float(variant.facial_hair.thickness));
  h = hash_combine(h, hash_float(variant.facial_hair.coverage));
  h = hash_combine(h, hash_float(variant.facial_hair.greyness));
  h = hash_combine(h, hash_float(variant.muscularity));
  h = hash_combine(h, hash_float(variant.scarring));
  h = hash_combine(h, hash_float(variant.weathering));
  h = hash_combine(h, hash_float(variant.grime));
  h = hash_combine(h, hash_float(variant.bloodiness));
  h = hash_combine(h, hash_float(variant.pattern_seed));
  return h;
}

template <>
auto variant_hash(const Render::GL::HorseVariant& variant) noexcept -> std::uint64_t {
  std::uint64_t h = 0xB492B66FBE98F273ULL;
  h = hash_combine(h, hash_vec3(variant.coat_color));
  h = hash_combine(h, hash_vec3(variant.mane_color));
  h = hash_combine(h, hash_vec3(variant.tail_color));
  h = hash_combine(h, hash_vec3(variant.muzzle_color));
  h = hash_combine(h, hash_vec3(variant.hoof_color));
  h = hash_combine(h, hash_vec3(variant.saddle_color));
  h = hash_combine(h, hash_vec3(variant.blanket_color));
  h = hash_combine(h, hash_vec3(variant.tack_color));
  h = hash_combine(h, static_cast<std::uint64_t>(variant.coat_kind));
  h = hash_combine(h, hash_float(variant.dapple_amount));
  h = hash_combine(h, variant.sock_mask);
  h = hash_combine(h, variant.has_blaze ? 1U : 0U);
  h = hash_combine(h, variant.has_star ? 1U : 0U);
  return h;
}

template <>
auto variant_hash(const Render::GL::ElephantVariant& variant) noexcept
    -> std::uint64_t {
  std::uint64_t h = 0x9AE16A3B2F90404FULL;
  h = hash_combine(h, hash_vec3(variant.skin_color));
  h = hash_combine(h, hash_vec3(variant.skin_highlight));
  h = hash_combine(h, hash_vec3(variant.skin_shadow));
  h = hash_combine(h, hash_vec3(variant.ear_inner_color));
  h = hash_combine(h, hash_vec3(variant.tusk_color));
  h = hash_combine(h, hash_vec3(variant.toenail_color));
  h = hash_combine(h, hash_vec3(variant.howdah_wood_color));
  h = hash_combine(h, hash_vec3(variant.howdah_fabric_color));
  h = hash_combine(h, hash_vec3(variant.howdah_metal_color));
  return h;
}

struct RoleColorCacheKey {
  Render::Creature::Pipeline::CreatureAssetId asset{
      Render::Creature::Pipeline::k_invalid_creature_asset};
  Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};
  std::uint64_t variant_hash{0U};

  auto operator==(const RoleColorCacheKey& other) const noexcept -> bool {
    return asset == other.asset && archetype == other.archetype &&
           variant_hash == other.variant_hash;
  }
};

struct RoleColorCacheKeyHash {
  auto operator()(const RoleColorCacheKey& key) const noexcept -> std::size_t {
    auto h = static_cast<std::uint64_t>(key.asset);
    h = hash_combine(h, key.archetype);
    h = hash_combine(h, key.variant_hash);
    return static_cast<std::size_t>(h);
  }
};

struct RoleColorCacheValue {
  std::array<QVector3D, Render::Creature::CreatureRenderRequest::k_role_color_capacity>
      colors{};
  std::uint8_t count{0U};
};

template <typename Variant>
void populate_role_colors_uncached(Render::Creature::CreatureRenderRequest& req,
                                   const Variant& variant) {
  const auto* asset = Render::Creature::Pipeline::CreatureAssetRegistry::instance().get(
      req.creature_asset_id);
  std::uint32_t count = 0;
  if (asset != nullptr && asset->fill_role_colors != nullptr) {
    count = asset->fill_role_colors(static_cast<const void*>(&variant),
                                    req.role_colors.data(),
                                    req.role_colors.size());
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
      count = fn(static_cast<const void*>(&variant),
                 req.role_colors.data(),
                 count,
                 req.role_colors.size());
    }
  }
  req.role_color_count = static_cast<std::uint8_t>(std::min<std::uint32_t>(
      count, static_cast<std::uint32_t>(req.role_colors.size())));
}

template <typename Variant>
void populate_role_colors(Render::Creature::CreatureRenderRequest& req,
                          const Variant& variant) {
  using Cache =
      std::unordered_map<RoleColorCacheKey, RoleColorCacheValue, RoleColorCacheKeyHash>;
  thread_local Cache cache;

  RoleColorCacheKey key{};
  key.asset = req.creature_asset_id;
  key.archetype = req.archetype;
  key.variant_hash = variant_hash(variant);

  if (auto it = cache.find(key); it != cache.end()) {
    req.role_colors = it->second.colors;
    req.role_color_count = it->second.count;
    return;
  }

  populate_role_colors_uncached(req, variant);

  constexpr std::size_t k_max_role_color_cache_entries = 8192;
  if (cache.size() >= k_max_role_color_cache_entries) {
    cache.clear();
  }
  RoleColorCacheValue value;
  value.colors = req.role_colors;
  value.count = req.role_color_count;
  cache.emplace(key, value);
}

} // namespace

void CreatureRenderBatch::add_humanoid(
    const CreatureGraphOutput& output,
    const Render::GL::HumanoidPose& pose,
    const Render::GL::HumanoidVariant& variant,
    const Render::GL::HumanoidAnimationContext& anim) {
  if (output.culled) {
    return;
  }

  auto const selection = output.humanoid_selection.has_value()
                             ? *output.humanoid_selection
                             : resolve_humanoid_animation_selection(
                                   output.spec, anim, output.seed, &variant);
  const auto* asset = CreatureAssetRegistry::instance().resolve(output.spec);
  if (asset == nullptr) {
    return;
  }

  if (output.pass_intent == RenderPassIntent::Shadow) {
    auto row = make_prepared_humanoid_row(output.spec,
                                          pose,
                                          variant,
                                          anim,
                                          output.world_matrix,
                                          output.seed,
                                          output.lod,
                                          output.entity_id,
                                          output.pass_intent);
    rows_.push_back(std::move(row));
  }

  auto req = build_request(
      output, selection.resolved_archetype, selection.state, selection.phase);
  req.creature_asset_id = asset->id;
  bool created_handle = false;
  {
    auto& profile = Render::Profiling::global_profile();
    Render::Profiling::AccumulatorScope const scope(
        output.pass_intent == RenderPassIntent::Main
            ? &profile.render_asset_cache_lookup_us
            : nullptr);
    req.render_asset_handle =
        CreatureRenderAssetHandleRegistry::instance().get_or_create(
            asset->id, selection.resolved_archetype, &created_handle);
    if (output.pass_intent == RenderPassIntent::Main) {
      if (created_handle) {
        ++profile.render_asset_cache_misses;
      } else {
        ++profile.render_asset_cache_hits;
      }
    }
  }
  req.clip_variant = selection.clip_variant;
  populate_role_colors(req, variant);
  req.wear_params = QVector4D(
      variant.weathering, variant.grime, variant.bloodiness, variant.pattern_seed);
  req.full_body_blend.archetype = selection.full_body_blend.archetype;
  req.full_body_blend.state = selection.full_body_blend.state;
  req.full_body_blend.phase = selection.full_body_blend.phase;
  req.full_body_blend.weight = selection.full_body_blend.weight;
  req.full_body_blend.clip_variant = selection.full_body_blend.clip_variant;
  req.full_body_blend.mode = selection.full_body_blend.mode;
  req.upper_body_overlay.archetype = selection.upper_body_overlay.archetype;
  req.upper_body_overlay.state = selection.upper_body_overlay.state;
  req.upper_body_overlay.phase = selection.upper_body_overlay.phase;
  req.upper_body_overlay.weight = selection.upper_body_overlay.weight;
  req.upper_body_overlay.clip_variant = selection.upper_body_overlay.clip_variant;
  req.upper_body_overlay.mode = selection.upper_body_overlay.mode;
  requests_.push_back(req);
}

void CreatureRenderBatch::add_humanoid(const PreparedHumanoidBodyState& state) {
  add_humanoid(state.graph, state.pose, state.variant, state.animation);
}

void CreatureRenderBatch::add_quadruped(const CreatureGraphOutput& output,
                                        const Render::GL::HorseVariant& variant,
                                        Render::Creature::AnimationStateId state,
                                        float phase,
                                        std::uint32_t clip_variant) {
  if (output.culled) {
    return;
  }
  const auto* asset =
      CreatureAssetRegistry::instance().for_species(CreatureKind::Horse);
  if (asset == nullptr) {
    return;
  }
  if (output.pass_intent == RenderPassIntent::Shadow) {
    auto row = make_prepared_creature_row(output.spec,
                                          CreatureKind::Horse,
                                          output.world_matrix,
                                          output.seed,
                                          output.lod,
                                          output.entity_id,
                                          output.pass_intent);
    rows_.push_back(std::move(row));
  }
  auto const archetype_id =
      (output.spec.archetype_id != Render::Creature::k_invalid_archetype)
          ? output.spec.archetype_id
          : default_archetype_for(CreatureKind::Horse);
  auto req = build_request(output, archetype_id, state, phase);
  req.creature_asset_id = asset->id;
  bool created_handle = false;
  {
    auto& profile = Render::Profiling::global_profile();
    Render::Profiling::AccumulatorScope const scope(
        output.pass_intent == RenderPassIntent::Main
            ? &profile.render_asset_cache_lookup_us
            : nullptr);
    req.render_asset_handle =
        CreatureRenderAssetHandleRegistry::instance().get_or_create(
            asset->id, archetype_id, &created_handle);
    if (output.pass_intent == RenderPassIntent::Main) {
      if (created_handle) {
        ++profile.render_asset_cache_misses;
      } else {
        ++profile.render_asset_cache_hits;
      }
    }
  }
  req.clip_variant = static_cast<std::uint8_t>(clip_variant);
  populate_role_colors(req, variant);
  requests_.push_back(req);
}

void CreatureRenderBatch::add_quadruped(const PreparedHorseBodyState& state) {
  add_quadruped(state.graph,
                state.variant,
                state.animation_state,
                state.phase,
                state.clip_variant);
}

void CreatureRenderBatch::add_quadruped(const CreatureGraphOutput& output,
                                        const Render::GL::ElephantVariant& variant,
                                        Render::Creature::AnimationStateId state,
                                        float phase,
                                        std::uint32_t clip_variant) {
  if (output.culled) {
    return;
  }
  const auto* asset =
      CreatureAssetRegistry::instance().for_species(CreatureKind::Elephant);
  if (asset == nullptr) {
    return;
  }
  if (output.pass_intent == RenderPassIntent::Shadow) {
    auto row = make_prepared_creature_row(output.spec,
                                          CreatureKind::Elephant,
                                          output.world_matrix,
                                          output.seed,
                                          output.lod,
                                          output.entity_id,
                                          output.pass_intent);
    rows_.push_back(std::move(row));
  }
  auto const archetype_id =
      (output.spec.archetype_id != Render::Creature::k_invalid_archetype)
          ? output.spec.archetype_id
          : default_archetype_for(CreatureKind::Elephant);
  auto req = build_request(output, archetype_id, state, phase);
  req.creature_asset_id = asset->id;
  bool created_handle = false;
  {
    auto& profile = Render::Profiling::global_profile();
    Render::Profiling::AccumulatorScope const scope(
        output.pass_intent == RenderPassIntent::Main
            ? &profile.render_asset_cache_lookup_us
            : nullptr);
    req.render_asset_handle =
        CreatureRenderAssetHandleRegistry::instance().get_or_create(
            asset->id, archetype_id, &created_handle);
    if (output.pass_intent == RenderPassIntent::Main) {
      if (created_handle) {
        ++profile.render_asset_cache_misses;
      } else {
        ++profile.render_asset_cache_hits;
      }
    }
  }
  req.clip_variant = static_cast<std::uint8_t>(clip_variant);
  populate_role_colors(req, variant);
  requests_.push_back(req);
}

void CreatureRenderBatch::add_quadruped(const PreparedElephantBodyState& state) {
  add_quadruped(state.graph,
                state.variant,
                state.animation_state,
                state.phase,
                state.clip_variant);
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
