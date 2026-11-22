// The CreatureRenderBatch elephant entry points, with the outgoing clip blend.

#include "render/creature/elephant/elephant_render_batch.h"

#include <algorithm>
#include <cstdint>

#include "render/creature/elephant/dimensions.h"
#include "render/creature/elephant/elephant_body_state.h"
#include "render/creature/pipeline/creature_render_batch_support.h"
#include "render/creature/pipeline/creature_render_graph.h"

namespace Render::Creature::Pipeline {

namespace {

using BatchSupport::hash_combine;
using BatchSupport::hash_float;
using BatchSupport::hash_vec3;

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

auto elephant_role_color_cache() -> BatchSupport::RoleColorCache& {
  thread_local BatchSupport::RoleColorCache cache;
  return cache;
}

} // namespace

void add_quadruped(CreatureRenderBatch& batch,
                   const CreatureGraphOutput& output,
                   const Render::GL::ElephantVariant& variant,
                   Render::Creature::AnimationStateId state,
                   float phase,
                   std::uint32_t clip_variant) {
  Render::Creature::CreatureRenderRequest req;
  if (!BatchSupport::build_quadruped_body(batch,
                                          output,
                                          CreatureKind::Elephant,
                                          state,
                                          phase,
                                          clip_variant,
                                          true,
                                          req)) {
    return;
  }
  BatchSupport::populate_role_colors(
      elephant_role_color_cache(), req, &variant, variant_hash(variant));
  batch.add_request(req);
}

void add_quadruped(CreatureRenderBatch& batch, const PreparedElephantBodyState& state) {
  std::size_t const before = batch.requests().size();
  add_quadruped(batch,
                state.graph,
                state.variant,
                state.animation_state,
                state.phase,
                state.clip_variant);
  if (state.outgoing_weight <= 0.0F || batch.requests().size() == before) {
    return;
  }
  auto& req = batch.mutable_requests().back();
  req.full_body_blend.archetype = req.archetype;
  req.full_body_blend.state = state.outgoing_state;
  req.full_body_blend.phase = state.outgoing_phase;
  req.full_body_blend.weight = std::clamp(state.outgoing_weight, 0.0F, 1.0F);
  req.full_body_blend.mode = Render::Creature::PlaybackLayerMode::FullBodyBlend;
}

} // namespace Render::Creature::Pipeline
