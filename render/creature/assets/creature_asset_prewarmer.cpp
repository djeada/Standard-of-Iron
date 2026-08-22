#include "creature_asset_prewarmer.h"

#include "animation/bpat/bpat_reader.h"
#include "render/creature/pipeline/creature_asset_init.h"
#include "render/creature/schema/skeleton_schema_hash.h"
#include "render/elephant/elephant_spec.h"
#include "render/horse/horse_spec.h"
#include "render/rigged_mesh_cache.h"
#include "render/wildlife/sheep_spec.h"
#include "render/wildlife/wolf_spec.h"

namespace Render::Creature {

namespace {

auto representative_blob(const Pipeline::CreatureRenderAssetHandle& handle)
    -> const Bpat::BpatBlob* {
  for (const auto& playback : handle.playback) {
    if (playback.blob != nullptr && playback.frame_count != 0U) {
      return playback.blob;
    }
  }
  return nullptr;
}

} // namespace

void initialize_species_lod_geometry(Pipeline::CreatureKind kind) {
  switch (kind) {
  case Pipeline::CreatureKind::Horse:
    Render::Horse::initialize_horse_asset();
    return;
  case Pipeline::CreatureKind::Elephant:
    Render::Elephant::initialize_elephant_asset();
    return;
  case Pipeline::CreatureKind::Sheep:
    Render::Wildlife::initialize_sheep_asset();
    return;
  case Pipeline::CreatureKind::Wolf:
    Render::Wildlife::initialize_wolf_asset();
    return;
  case Pipeline::CreatureKind::Humanoid:
  case Pipeline::CreatureKind::Mounted:

    return;
  }
}

auto CreatureAssetPrewarmer::prewarm(const CreatureAssetKey& key) -> bool {
  if (key.archetype == k_invalid_archetype ||
      key.creature_asset == Pipeline::k_invalid_creature_asset ||
      key.lod == CreatureLOD::Culled) {
    return false;
  }

  auto& registry = Pipeline::CreatureRenderAssetHandleRegistry::instance();
  const auto handle_id = registry.get_or_create(key.creature_asset, key.archetype);
  const auto* handle = registry.get(handle_id);
  if (handle == nullptr || !handle->valid()) {
    return false;
  }

  if (handle->asset != nullptr) {
    initialize_species_lod_geometry(handle->asset->kind);
  }

  const auto* blob = representative_blob(*handle);
  if (blob == nullptr) {
    return false;
  }

  if (handle->asset != nullptr && handle->asset->topology != nullptr &&
      !bone_parents_match(
          *handle->asset->topology, blob->bone_parents(), blob->bone_count())) {
    report_skeleton_schema_mismatch(handle->asset->debug_name,
                                    *handle->asset->topology,
                                    blob->bone_parents(),
                                    blob->bone_count());
  }

  return Pipeline::create_creature_render_asset(m_cache,
                                                *handle,
                                                key.lod,
                                                *blob,
                                                static_cast<std::uint16_t>(key.variant),
                                                m_upload_gpu_resources) != nullptr;
}

} // namespace Render::Creature
