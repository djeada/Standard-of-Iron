#include "civilian_actor_prewarm.h"

#include <algorithm>
#include <initializer_list>

#include "civilian_actor.h"
#include "farm_activity.h"
#include "home_activity.h"
#include "render/creature/archetype_variant_table.h"
#include "render/humanoid/asset/facial_hair_catalog.h"

namespace Render::GL {

namespace {

void append_target(std::vector<HumanoidPrewarmTarget>& out,
                   Render::Creature::Pipeline::CreatureAssetId asset,
                   Render::Creature::ArchetypeId archetype) {
  if (asset == Render::Creature::Pipeline::k_invalid_creature_asset ||
      archetype == Render::Creature::k_invalid_archetype) {
    return;
  }
  HumanoidPrewarmTarget const target{.asset = asset, .archetype = archetype};
  if (std::find(out.begin(), out.end(), target) == out.end()) {
    out.push_back(target);
  }
}

auto resolved_asset(const Render::Creature::Pipeline::UnitVisualSpec& spec)
    -> Render::Creature::Pipeline::CreatureAssetId {
  const auto* asset =
      Render::Creature::Pipeline::CreatureAssetRegistry::instance().resolve(spec);
  return asset != nullptr ? asset->id
                          : Render::Creature::Pipeline::k_invalid_creature_asset;
}

void append_rig_targets(std::vector<HumanoidPrewarmTarget>& out,
                        const NationCivilianRig& rig) {
  if (!rig.valid()) {
    return;
  }
  auto const asset = resolved_asset(rig.spec);
  append_target(out, asset, rig.idle);
  append_target(out, asset, rig.working);
}

} // namespace

auto variant_table_prewarm_targets(const Render::Creature::Pipeline::UnitVisualSpec&
                                       spec) -> std::vector<HumanoidPrewarmTarget> {
  std::vector<HumanoidPrewarmTarget> out;
  const auto* table = spec.animation_manifest.variant_table;
  if (table == nullptr) {
    return out;
  }
  auto const asset = resolved_asset(spec);

  auto append_with_beards = [&](Render::Creature::ArchetypeId archetype) {
    append_target(out, asset, archetype);
    for (auto const style : {FacialHairStyle::ShortBeard,
                             FacialHairStyle::FullBeard,
                             FacialHairStyle::Goatee,
                             FacialHairStyle::MustacheAndBeard}) {
      append_target(
          out, asset, Render::Humanoid::facial_hair_body_archetype(archetype, style));
    }
  };
  for (auto const archetype : table->archetype_for_pose) {
    append_with_beards(archetype);
  }
  for (auto const archetype : table->archetype_for_variant) {
    append_with_beards(archetype);
  }
  return out;
}

auto civilian_actor_prewarm_targets() -> std::vector<HumanoidPrewarmTarget> {
  std::vector<HumanoidPrewarmTarget> out;
  for (bool const carthage : {false, true}) {
    append_rig_targets(out, nation_crew_rig(carthage));
    append_rig_targets(out, nation_priest_rig(carthage));

    const auto& civilian = nation_civilian_rig(carthage);
    append_rig_targets(out, civilian);
    if (!civilian.valid()) {
      continue;
    }
    auto const asset = resolved_asset(civilian.spec);
    const auto& farm = farm_worker_visual(carthage);
    for (auto const archetype : {farm.hatted_tending,
                                 farm.hatted_reaping,
                                 farm.hatted_gathering,
                                 farm.bare_gathering,
                                 farm.napping}) {
      append_target(out, asset, archetype);
    }
    append_target(out, asset, home_bowl_archetype(carthage));
  }
  return out;
}

} // namespace Render::GL
