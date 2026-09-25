#pragma once

#include <vector>

#include "render/creature/pipeline/creature_asset.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/creature/spec.h"

namespace Render::GL {

// One humanoid body the renderer can be asked to draw: the creature asset it
// skins and the archetype that decides its attachments. The template prewarm
// bakes each one before it forbids render-time bakes, because a body that was
// not baked by then is not drawn at all.
struct HumanoidPrewarmTarget {
  Render::Creature::Pipeline::CreatureAssetId asset{
      Render::Creature::Pipeline::k_invalid_creature_asset};
  Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};

  [[nodiscard]] auto
  operator==(const HumanoidPrewarmTarget& other) const noexcept -> bool = default;
};

// Every archetype a unit's variant table can swap in: the builder's hammer,
// saw, chisel and sickle, which a construction job or a seed picks while the
// builder works, the civilian's cudgel, and so on. Drawing a unit at idle
// never reaches them.
[[nodiscard]] auto
variant_table_prewarm_targets(const Render::Creature::Pipeline::UnitVisualSpec& spec)
    -> std::vector<HumanoidPrewarmTarget>;

// Every body the building-side actors draw for both nations: siege engine
// crews, townsfolk and priests, farm workers and the resident with the soup
// bowl. They are drawn straight from these archetypes, never through a unit
// renderer, and at Minimal detail once they are small on screen on presets
// that use creature LOD.
[[nodiscard]] auto
civilian_actor_prewarm_targets() -> std::vector<HumanoidPrewarmTarget>;

} // namespace Render::GL
