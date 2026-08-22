#include "creature_render_graph.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_registry.h"
#include "creature_asset.h"
#include "creature_prepared_state.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "humanoid_animation_selection.h"
#include "preparation_common.h"
#include "render/creature/archetype_registry.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/graphics_settings.h"
#include "render/humanoid/asset/humanoid_spec.h"
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

auto quadruped_lod_from_settings(CreatureKind kind, float distance) noexcept
    -> Render::Creature::CreatureLOD {
  switch (kind) {
  case CreatureKind::Horse:
    return select_distance_lod(distance, horse_lod_config_from_settings().thresholds);
  case CreatureKind::Elephant:
    return select_distance_lod(distance,
                               elephant_lod_config_from_settings().thresholds);
  case CreatureKind::Sheep:
  case CreatureKind::Wolf:
    return select_distance_lod(distance, horse_lod_config_from_settings().thresholds);
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
  cached_humanoid_variant_ = nullptr;
  cached_humanoid_asset_ = k_invalid_creature_asset;
  cached_humanoid_archetype_ = Render::Creature::k_invalid_archetype;
  cached_humanoid_handle_ = Render::Creature::k_invalid_creature_render_asset_handle;
  cached_humanoid_role_colors_.reset();
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
  req.instance_index = output.instance_index;
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

template <>
auto variant_hash(const Render::GL::WildlifeVariant& variant) noexcept
    -> std::uint64_t {
  std::uint64_t h = 0x27D4EB2F165667C5ULL;
  h = hash_combine(h, static_cast<std::uint64_t>(variant.role_count));
  for (std::uint8_t i = 0; i < variant.role_count; ++i) {
    h = hash_combine(h, hash_vec3(variant.roles[i]));
  }
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

template <typename Variant>
void populate_role_colors_uncached(Render::Creature::CreatureRenderRequest& req,
                                   const Variant& variant) {
  auto palette = std::make_shared<Render::RoleColorPalette>();
  const auto* asset = Render::Creature::Pipeline::CreatureAssetRegistry::instance().get(
      req.creature_asset_id);
  std::uint32_t count = 0;
  if (asset != nullptr && asset->fill_role_colors != nullptr) {
    count = asset->fill_role_colors(static_cast<const void*>(&variant),
                                    palette->colors.data(),
                                    palette->colors.size());
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
                 palette->colors.data(),
                 count,
                 palette->colors.size());
    }
  }
  palette->count = static_cast<std::uint8_t>(std::min<std::uint32_t>(
      count, static_cast<std::uint32_t>(palette->colors.size())));
  req.role_color_count = palette->count;
  req.role_colors = std::move(palette);
}

template <typename Variant>
void populate_role_colors(Render::Creature::CreatureRenderRequest& req,
                          const Variant& variant) {
  using Cache = std::unordered_map<RoleColorCacheKey,
                                   std::shared_ptr<const Render::RoleColorPalette>,
                                   RoleColorCacheKeyHash>;
  thread_local Cache cache;

  RoleColorCacheKey key{};
  key.asset = req.creature_asset_id;
  key.archetype = req.archetype;
  key.variant_hash = variant_hash(variant);

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
  const bool cached_asset =
      cached_humanoid_asset_ == asset->id &&
      cached_humanoid_archetype_ == selection.resolved_archetype &&
      cached_humanoid_handle_ !=
          Render::Creature::k_invalid_creature_render_asset_handle;
  if (cached_asset) {
    req.render_asset_handle = cached_humanoid_handle_;
    if (output.pass_intent == RenderPassIntent::Main) {
      ++Render::Profiling::global_profile().render_asset_cache_hits;
    }
  } else {
    bool created_handle = false;
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
    cached_humanoid_asset_ = asset->id;
    cached_humanoid_archetype_ = selection.resolved_archetype;
    cached_humanoid_handle_ = req.render_asset_handle;
  }
  req.clip_variant = selection.clip_variant;
  req.clip_id = selection.clip_id.value_or(Animation::k_unmapped_clip);
  if (cached_asset && cached_humanoid_variant_ == &variant &&
      cached_humanoid_role_colors_ != nullptr) {
    req.role_colors = cached_humanoid_role_colors_;
    req.role_color_count = cached_humanoid_role_colors_->count;
  } else {
    populate_role_colors(req, variant);
    cached_humanoid_variant_ = &variant;
    cached_humanoid_role_colors_ = req.role_colors;
  }
  req.wear_params = QVector4D(
      variant.weathering, variant.grime, variant.bloodiness, variant.pattern_seed);
  req.full_body_blend.archetype = selection.full_body_blend.archetype;
  req.full_body_blend.state = selection.full_body_blend.state;
  req.full_body_blend.phase = selection.full_body_blend.phase;
  req.full_body_blend.weight = selection.full_body_blend.weight;
  req.full_body_blend.clip_variant = selection.full_body_blend.clip_variant;
  req.full_body_blend.clip_id =
      selection.full_body_blend.clip_id.value_or(Animation::k_unmapped_clip);
  req.full_body_blend.mode = selection.full_body_blend.mode;
  req.upper_body_overlay.archetype = selection.upper_body_overlay.archetype;
  req.upper_body_overlay.state = selection.upper_body_overlay.state;
  req.upper_body_overlay.phase = selection.upper_body_overlay.phase;
  req.upper_body_overlay.weight = selection.upper_body_overlay.weight;
  req.upper_body_overlay.clip_variant = selection.upper_body_overlay.clip_variant;
  req.upper_body_overlay.clip_id =
      selection.upper_body_overlay.clip_id.value_or(Animation::k_unmapped_clip);
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

void CreatureRenderBatch::add_quadruped(const PreparedWildlifeBodyState& state) {
  const CreatureGraphOutput& output = state.graph;
  if (output.culled) {
    return;
  }
  const auto* asset = CreatureAssetRegistry::instance().for_species(state.kind);
  if (asset == nullptr) {
    return;
  }
  if (output.pass_intent == RenderPassIntent::Shadow) {
    rows_.push_back(make_prepared_creature_row(output.spec,
                                               state.kind,
                                               output.world_matrix,
                                               output.seed,
                                               output.lod,
                                               output.entity_id,
                                               output.pass_intent));
  }
  auto const archetype_id =
      (output.spec.archetype_id != Render::Creature::k_invalid_archetype)
          ? output.spec.archetype_id
          : default_archetype_for(state.kind);
  auto req = build_request(output, archetype_id, state.animation_state, state.phase);
  req.creature_asset_id = asset->id;
  bool created_handle = false;
  req.render_asset_handle = CreatureRenderAssetHandleRegistry::instance().get_or_create(
      asset->id, archetype_id, &created_handle);
  req.clip_variant = static_cast<std::uint8_t>(state.clip_variant);
  if (state.outgoing_weight > 0.0F) {
    req.full_body_blend.archetype = archetype_id;
    req.full_body_blend.state = state.outgoing_state;
    req.full_body_blend.phase = state.outgoing_phase;
    req.full_body_blend.weight = state.outgoing_weight;
    req.full_body_blend.mode = Render::Creature::PlaybackLayerMode::FullBodyBlend;
  }
  populate_role_colors(req, state.variant);
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
