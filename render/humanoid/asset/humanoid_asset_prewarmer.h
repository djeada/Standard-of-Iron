#pragma once

#include "render/creature/part_graph.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/render_request.h"

namespace Render::GL {
class RiggedMeshCache;
}

namespace Render::Humanoid {

struct HumanoidAssetRequest {
  Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};
  Render::Creature::VariantId variant{Render::Creature::k_canonical_variant};
  Render::Creature::Pipeline::CreatureAssetId creature_asset{
      Render::Creature::Pipeline::k_invalid_creature_asset};
};

class HumanoidAssetPrewarmer {
public:
  explicit HumanoidAssetPrewarmer(Render::GL::RiggedMeshCache& cache,
                                  bool upload_gpu_resources = true) noexcept
      : m_cache(cache)
      , m_upload_gpu_resources(upload_gpu_resources) {}

  auto prewarm(const HumanoidAssetRequest& request) -> bool;

private:
  Render::GL::RiggedMeshCache& m_cache;
  bool m_upload_gpu_resources;
};

} // namespace Render::Humanoid
