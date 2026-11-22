// CreatureRenderBatch's humanoid entry points: the body request with its
// animation selection, blend layers, role colours and wear.

#include <QVector4D>

#include <cstdint>
#include <utility>

#include "animation/clip_manifest.h"
#include "render/creature/humanoid/humanoid_body_state.h"
#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/creature_render_batch_support.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/humanoid_animation_selection.h"
#include "render/profiling/frame_profile.h"

namespace Render::Creature::Pipeline {

namespace {

using BatchSupport::hash_combine;
using BatchSupport::hash_float;
using BatchSupport::hash_vec3;

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

auto humanoid_role_color_cache() -> BatchSupport::RoleColorCache& {
  thread_local BatchSupport::RoleColorCache cache;
  return cache;
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

  auto req = BatchSupport::build_request(
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
    BatchSupport::populate_role_colors(
        humanoid_role_color_cache(), req, &variant, variant_hash(variant));
    cached_humanoid_variant_ = &variant;
    cached_humanoid_role_colors_ = req.role_colors;
  }
  req.wear_params = QVector4D(
      variant.weathering, variant.grime, variant.bloodiness, variant.pattern_seed);
  req.team_emphasis =
      anim.inputs.is_in_melee_lock || anim.inputs.is_attacking ? 1.0F : 0.0F;
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
  req.transition = selection.transition;
  requests_.push_back(req);
}

void CreatureRenderBatch::add_humanoid(const PreparedHumanoidBodyState& state) {
  add_humanoid(state.graph, state.pose, state.variant, state.animation);
}

} // namespace Render::Creature::Pipeline
