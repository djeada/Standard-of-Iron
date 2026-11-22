// CreatureRenderBatch's sheep and wolf entry point, with the outgoing clip blend.

#include <algorithm>
#include <cstdint>

#include "render/creature/pipeline/creature_render_batch_support.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/wildlife/wildlife_body_state.h"
#include "render/creature/wildlife/wildlife_variant.h"

namespace Render::Creature::Pipeline {

namespace {

using BatchSupport::hash_combine;
using BatchSupport::hash_float;
using BatchSupport::hash_vec3;

auto variant_hash(const Render::GL::WildlifeVariant& variant) noexcept
    -> std::uint64_t {
  std::uint64_t h = 0x27D4EB2F165667C5ULL;
  h = hash_combine(h, static_cast<std::uint64_t>(variant.role_count));
  for (std::uint8_t i = 0; i < variant.role_count; ++i) {
    h = hash_combine(h, hash_vec3(variant.roles[i]));
  }
  return h;
}

auto wildlife_role_color_cache() -> BatchSupport::RoleColorCache& {
  thread_local BatchSupport::RoleColorCache cache;
  return cache;
}

} // namespace

void CreatureRenderBatch::add_quadruped(const PreparedWildlifeBodyState& state) {
  const CreatureGraphOutput& output = state.graph;
  Render::Creature::CreatureRenderRequest req;
  if (!BatchSupport::build_quadruped_body(*this,
                                          output,
                                          state.kind,
                                          state.animation_state,
                                          state.phase,
                                          state.clip_variant,
                                          false,
                                          req)) {
    return;
  }
  if (state.outgoing_weight > 0.0F) {
    req.full_body_blend.archetype = req.archetype;
    req.full_body_blend.state = state.outgoing_state;
    req.full_body_blend.phase = state.outgoing_phase;
    req.full_body_blend.weight = state.outgoing_weight;
    req.full_body_blend.mode = Render::Creature::PlaybackLayerMode::FullBodyBlend;
  }
  BatchSupport::populate_role_colors(
      wildlife_role_color_cache(), req, &state.variant, variant_hash(state.variant));
  requests_.push_back(req);
}

} // namespace Render::Creature::Pipeline
