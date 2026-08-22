#include "quadruped_asset_key.h"

namespace Render::Creature::Quadruped {

auto QuadrupedAssetKeyHash::operator()(const QuadrupedAssetKey& key) const noexcept
    -> std::size_t {
  std::uint64_t hash = 0xcbf29ce484222325ULL;
  auto mix = [&hash](std::uint64_t value) {
    hash ^= value;
    hash *= 0x100000001b3ULL;
  };
  mix(static_cast<std::uint64_t>(key.species));
  mix(key.archetype);
  mix(key.creature_asset);
  mix(static_cast<std::uint64_t>(key.geometry_lod));
  mix(key.body_variant);
  mix(key.equipment);
  return static_cast<std::size_t>(hash);
}

auto asset_key_of(const Render::Creature::CreatureRenderRequest& request,
                  Render::Creature::Pipeline::CreatureKind species) noexcept
    -> QuadrupedAssetKey {
  QuadrupedAssetKey key;
  key.species = species;
  key.archetype = request.archetype;
  key.creature_asset = request.creature_asset_id;
  key.geometry_lod = request.lod;
  key.body_variant = static_cast<GeometryVariantId>(request.variant);
  return key;
}

auto instance_state_of(const Render::Creature::CreatureRenderRequest& request) noexcept
    -> QuadrupedInstanceState {
  QuadrupedInstanceState state;
  state.world = request.world;
  state.animation = request.state;
  state.phase = request.phase;
  state.visual_seed = request.seed;
  return state;
}

} // namespace Render::Creature::Quadruped
