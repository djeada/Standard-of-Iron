#pragma once

#include <vector>

#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/spec.h"

namespace Render::GL {

struct HumanoidPrewarmTarget {
  Render::Creature::Pipeline::CreatureAssetId asset{
      Render::Creature::Pipeline::k_invalid_creature_asset};
  Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};

  [[nodiscard]] auto
  operator==(const HumanoidPrewarmTarget& other) const noexcept -> bool = default;
};

[[nodiscard]] auto
variant_table_prewarm_targets(const Render::Creature::Pipeline::UnitVisualSpec& spec)
    -> std::vector<HumanoidPrewarmTarget>;

[[nodiscard]] auto
civilian_actor_prewarm_targets() -> std::vector<HumanoidPrewarmTarget>;

} // namespace Render::GL
