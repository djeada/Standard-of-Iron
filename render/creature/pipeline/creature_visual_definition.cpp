#include "render/creature/pipeline/creature_visual_definition.h"

#include "render/creature/pipeline/creature_asset.h"

namespace Render::Creature::Pipeline {

auto resolve_creature_visual_definition(const UnitVisualSpec& spec) noexcept
    -> const CreatureVisualDefinition* {
  if (spec.creature_definition != nullptr) {
    return spec.creature_definition;
  }
  const CreatureAsset* asset = CreatureAssetRegistry::instance().for_species(spec.kind);
  return asset != nullptr ? asset->visual_definition : nullptr;
}

} // namespace Render::Creature::Pipeline
