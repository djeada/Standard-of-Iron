

#include "render/creature/horse/horse_render_batch.h"

#include <algorithm>
#include <cstdint>

#include "render/creature/horse/dimensions.h"
#include "render/creature/horse/horse_body_state.h"
#include "render/creature/pipeline/creature_render_batch_support.h"
#include "render/creature/pipeline/creature_render_graph.h"

namespace Render::Creature::Pipeline {

namespace {

using BatchSupport::hash_combine;
using BatchSupport::hash_float;
using BatchSupport::hash_vec3;

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

auto horse_role_color_cache() -> BatchSupport::RoleColorCache& {
  thread_local BatchSupport::RoleColorCache cache;
  return cache;
}

} // namespace

void add_quadruped(CreatureRenderBatch& batch,
                   const CreatureGraphOutput& output,
                   const Render::GL::HorseVariant& variant,
                   Render::Creature::AnimationStateId state,
                   float phase,
                   std::uint32_t clip_variant) {
  Render::Creature::CreatureRenderRequest req;
  if (!BatchSupport::build_quadruped_body(
          batch, output, CreatureKind::Horse, state, phase, clip_variant, true, req)) {
    return;
  }
  BatchSupport::populate_role_colors(
      horse_role_color_cache(), req, &variant, variant_hash(variant));
  batch.add_request(req);
}

void add_quadruped(CreatureRenderBatch& batch, const PreparedHorseBodyState& state) {
  add_quadruped(batch,
                state.graph,
                state.variant,
                state.animation_state,
                state.phase,
                state.clip_variant);
}

} // namespace Render::Creature::Pipeline
