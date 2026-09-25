#pragma once

#include <cstdint>

#include "creature_asset.h"
#include "render/rigged_mesh_cache.h"

namespace Render::Creature::Bpat {
class BpatBlob;
}

namespace Render::Creature::Pipeline {

[[nodiscard]] auto rigged_asset_key(const CreatureRenderAssetHandle& handle,
                                    Render::Creature::CreatureLOD lod,
                                    std::uint32_t skin_species_id) noexcept
    -> Render::GL::RiggedMeshCache::Key;

auto create_creature_render_asset(Render::GL::RiggedMeshCache& cache,
                                  const CreatureRenderAssetHandle& handle,
                                  Render::Creature::CreatureLOD lod,
                                  const Render::Creature::Bpat::BpatBlob& blob,
                                  std::uint16_t variant_bucket,
                                  bool upload_skin_ubo)
    -> const Render::GL::RiggedMeshEntry*;

} // namespace Render::Creature::Pipeline
