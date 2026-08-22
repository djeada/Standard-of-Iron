#include "render/humanoid/asset/humanoid_asset_prewarmer.h"

#include "animation/bpat/bpat_reader.h"
#include "render/creature/pipeline/creature_asset_init.h"
#include "render/humanoid/asset/humanoid_derived_meshes.h"
#include "render/rigged_mesh_cache.h"

namespace Render::Humanoid {

namespace {

auto representative_blob(const Render::Creature::Pipeline::CreatureRenderAssetHandle&
                             handle) -> const Render::Creature::Bpat::BpatBlob* {
  for (const auto& playback : handle.playback) {
    if (playback.blob != nullptr && playback.frame_count != 0U) {
      return playback.blob;
    }
  }
  return nullptr;
}

} // namespace

auto HumanoidAssetPrewarmer::prewarm(const HumanoidAssetRequest& request) -> bool {
  using Render::Creature::Pipeline::CreatureRenderAssetHandleRegistry;

  if (request.archetype == Render::Creature::k_invalid_archetype ||
      request.creature_asset == Render::Creature::Pipeline::k_invalid_creature_asset ||
      request.lod == Render::Creature::CreatureLOD::Culled) {
    return false;
  }

  build_humanoid_derived_meshes();

  auto& registry = CreatureRenderAssetHandleRegistry::instance();
  const auto handle_id =
      registry.get_or_create(request.creature_asset, request.archetype);
  const auto* handle = registry.get(handle_id);
  if (handle == nullptr || !handle->valid()) {
    return false;
  }

  const auto* blob = representative_blob(*handle);
  if (blob == nullptr) {
    return false;
  }

  return Render::Creature::Pipeline::create_creature_render_asset(
             m_cache,
             *handle,
             request.lod,
             *blob,
             static_cast<std::uint16_t>(request.variant),
             m_upload_gpu_resources) != nullptr;
}

} // namespace Render::Humanoid
