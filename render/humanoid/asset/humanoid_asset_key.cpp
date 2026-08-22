#include "render/humanoid/asset/humanoid_asset_key.h"

namespace Render::Humanoid {

auto HumanoidAssetKeyHash::operator()(const HumanoidAssetKey& key) const noexcept
    -> std::size_t {
  std::uint64_t hash = 0xcbf29ce484222325ULL;
  auto mix = [&hash](std::uint64_t value) {
    hash ^= value;
    hash *= 0x100000001b3ULL;
  };
  mix(key.archetype);
  mix(static_cast<std::uint64_t>(key.lod));
  mix(key.geometry_variant);
  mix(key.attachment_set);
  mix(key.creature_asset);
  mix(key.attachments_hash);
  mix(key.skin_species);
  return static_cast<std::size_t>(hash);
}

auto asset_key_of(const Render::Creature::CreatureRenderRequest& request,
                  const Render::Creature::Pipeline::CreatureRenderAssetHandle*
                      handle) noexcept -> HumanoidAssetKey {
  HumanoidAssetKey key;
  key.archetype = request.archetype;
  key.lod = request.lod;
  key.geometry_variant = request.variant;
  key.creature_asset = request.creature_asset_id;
  if (handle != nullptr) {
    key.attachment_set = handle->attachment_set_id;
    key.attachments_hash = handle->attachments_hash;
    if (handle->asset != nullptr) {
      key.skin_species = handle->asset->bpat_species_id;
    }
  }
  return key;
}

auto instance_params_of(const Render::Creature::CreatureRenderRequest& request) noexcept
    -> HumanoidInstanceParams {
  HumanoidInstanceParams params;
  params.world = request.world;
  params.animation = request.state;
  params.clip = request.clip_id;
  params.clip_variant = request.clip_variant;
  params.frame = request.phase;
  params.blend =
      request.full_body_blend.active() ? request.full_body_blend.weight : 0.0F;
  params.team_tint = request.base_color;
  params.wear = request.wear_params;
  params.seed = request.seed;
  params.hit_reaction =
      request.upper_body_overlay.active() ? request.upper_body_overlay.weight : 0.0F;
  params.combat_emphasis = request.upper_body_overlay.phase;
  return params;
}

auto rigged_cache_key(const Render::Creature::Pipeline::CreatureAsset& asset,
                      const HumanoidAssetKey& key) noexcept
    -> Render::GL::RiggedMeshCache::Key {
  return Render::GL::RiggedMeshCache::Key{.spec = asset.spec,
                                          .lod = key.lod,
                                          .skin_species_id = key.skin_species,
                                          .attachment_set_id = key.attachment_set,
                                          .attachments_hash = key.attachments_hash};
}

} // namespace Render::Humanoid
