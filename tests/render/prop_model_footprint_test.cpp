

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <string_view>
#include <vector>

#include "game/map/map_definition.h"
#include "render/gl/backend/abandoned_home_parts.h"
#include "render/gl/backend/cursed_gold_vein_parts.h"
#include "render/gl/backend/dead_tree_mesh.h"
#include "render/gl/backend/magic_shrine_parts.h"
#include "render/gl/backend/prop_parts.h"
#include "render/gl/backend/ruins_parts.h"
#include "render/gl/backend/statue_parts.h"
#include "render/gl/backend/supply_cart_parts.h"
#include "render/gl/backend/tent_parts.h"
#include "render/gl/backend/weapon_rack_parts.h"

namespace {

using Game::Map::WorldProp;
using namespace Render::GL::BackendPipelines;

struct GroundReach {
  float x = 0.0F;
  float z = 0.0F;

  void widen(float reach_x, float reach_z) {
    x = std::max(x, std::abs(reach_x));
    z = std::max(z, std::abs(reach_z));
  }
};

void take(GroundReach& reach, const PropBoxPart& part) {
  reach.widen(part.lo.x, part.lo.z);
  reach.widen(part.hi.x, part.hi.z);
}

void take(GroundReach& reach, const PropOrientedBoxPart& part) {
  float const spread = std::max(part.half_width, part.half_depth);
  reach.widen(std::abs(part.a.x) + spread, std::abs(part.a.z) + spread);
  reach.widen(std::abs(part.b.x) + spread, std::abs(part.b.z) + spread);
}

void take(GroundReach& reach, const PropBeamPart& part) {
  float const spread = std::max(part.half_width, part.half_depth);
  reach.widen(std::abs(part.a.x) + spread, std::abs(part.a.z) + spread);
  reach.widen(std::abs(part.b.x) + spread, std::abs(part.b.z) + spread);
}

void take(GroundReach& reach, const PropLimbPart& part) {
  float const spread = std::max(part.r0, part.r1);
  reach.widen(std::abs(part.a.x) + spread, std::abs(part.a.z) + spread);
  reach.widen(std::abs(part.b.x) + spread, std::abs(part.b.z) + spread);
}

void take(GroundReach& reach, const PropSlabPart& part) {
  float const spread = std::max(part.half_bottom, part.half_top);
  reach.widen(spread, spread);
}

void take(GroundReach& reach, const PropFrustumPart& part) {
  reach.widen(std::abs(part.cx) + std::max(part.rx0, part.rx1),
              std::abs(part.cz) + std::max(part.rz0, part.rz1));
}

void take(GroundReach& reach, const PropTaperPart& part) {
  float const spread = std::max(part.r0, part.r1);
  reach.widen(std::abs(part.cx) + spread, std::abs(part.cz) + spread);
}

void take(GroundReach& reach, const PropVertPrismPart& part) {
  reach.widen(std::abs(part.cx) + part.r, std::abs(part.cz) + part.r);
}

template <typename Parts>
void widest(GroundReach& reach, const Parts& parts) {
  for (auto const& part : parts) {
    take(reach, part);
  }
}

template <typename... Parts>
auto model_reach(const Parts&... parts) -> GroundReach {
  GroundReach reach;
  (widest(reach, parts), ...);
  return reach;
}

struct MeasuredProp {
  std::string_view name;
  WorldProp::Type type;
  GroundReach reach;
};

auto dead_tree_reach() -> GroundReach {
  GroundReach reach;
  for (auto const& vertex : build_dead_tree_mesh().vertices) {
    reach.widen(vertex.first.x(), vertex.first.z());
  }
  return reach;
}

auto measured_props() -> std::vector<MeasuredProp> {
  return {
      {"ruins",
       WorldProp::Type::Ruins,
       model_reach(RuinsParts::k_ruins_boxes,
                   RuinsParts::k_ruins_prisms,
                   RuinsParts::k_ruins_oriented_boxes)},
      {"abandoned_home",
       WorldProp::Type::AbandonedHome,
       model_reach(AbandonedHomeParts::k_abandoned_home_boxes,
                   AbandonedHomeParts::k_abandoned_home_oriented_boxes)},
      {"magic_shrine",
       WorldProp::Type::MagicShrine,
       model_reach(MagicShrineParts::k_magic_shrine_boxes,
                   MagicShrineParts::k_magic_shrine_prisms,
                   MagicShrineParts::k_magic_shrine_oriented_boxes)},
      {"cursed_gold_vein",
       WorldProp::Type::CursedGoldVein,
       model_reach(CursedGoldVeinParts::k_cursed_gold_vein_mounds,
                   CursedGoldVeinParts::k_cursed_gold_vein_rubble,
                   CursedGoldVeinParts::k_cursed_gold_vein_shards)},
      {"supply_cart",
       WorldProp::Type::SupplyCart,
       model_reach(SupplyCartParts::k_supply_cart_boxes,
                   SupplyCartParts::k_supply_cart_beams,
                   SupplyCartParts::k_supply_cart_tapers)},
      {"weapon_rack",
       WorldProp::Type::WeaponRack,
       model_reach(WeaponRackParts::k_weapon_rack_boxes,
                   WeaponRackParts::k_weapon_rack_beams,
                   WeaponRackParts::k_weapon_rack_tapers)},
      {"statue",
       WorldProp::Type::Statue,
       model_reach(StatueParts::k_statue_slabs,
                   StatueParts::k_statue_beams,
                   StatueParts::k_statue_limbs,
                   StatueParts::k_statue_frustums)},
      {"tent",
       WorldProp::Type::Tent,
       GroundReach{TentParts::k_ground_half_x, TentParts::k_ground_half_z}},
      {"dead_tree", WorldProp::Type::DeadTree, dead_tree_reach()},
  };
}

} // namespace

constexpr float k_extent_epsilon = 1.0e-4F;

TEST(PropModelFootprintTest, EveryPropDeclaresAnExtentItsModelFitsInside) {
  for (auto const& prop : measured_props()) {
    auto const declared = Game::Map::world_prop_model_half_extents(prop.type);
    EXPECT_GE(declared.x + k_extent_epsilon, prop.reach.x)
        << prop.name << " draws out to " << prop.reach.x
        << " along x but declares a half extent of " << declared.x
        << "; the navigation grid would leave " << (prop.reach.x - declared.x)
        << " units of visible structure open ground";
    EXPECT_GE(declared.z + k_extent_epsilon, prop.reach.z)
        << prop.name << " draws out to " << prop.reach.z
        << " along z but declares a half extent of " << declared.z
        << "; the navigation grid would leave " << (prop.reach.z - declared.z)
        << " units of visible structure open ground";
  }
}

TEST(PropModelFootprintTest, NoPropBlocksFarMoreGroundThanItDraws) {
  for (auto const& prop : measured_props()) {
    auto const declared = Game::Map::world_prop_model_half_extents(prop.type);
    EXPECT_LE(declared.x, prop.reach.x * 1.5F + 0.1F)
        << prop.name << " blocks " << declared.x << " along x while drawing only "
        << prop.reach.x;
    EXPECT_LE(declared.z, prop.reach.z * 1.5F + 0.1F)
        << prop.name << " blocks " << declared.z << " along z while drawing only "
        << prop.reach.z;
  }
}

TEST(PropModelFootprintTest, GroundFractionFollowsTheModelExceptForCanopies) {
  for (auto const& prop : measured_props()) {
    ASSERT_FALSE(Game::Map::world_prop_blocks_only_its_stem(prop.type))
        << prop.name << " is not a canopy prop";
  }

  for (auto const type : {WorldProp::Type::PineTree,
                          WorldProp::Type::OliveTree,
                          WorldProp::Type::CypressTree,
                          WorldProp::Type::PalmTree}) {
    EXPECT_TRUE(Game::Map::world_prop_blocks_only_its_stem(type));
    EXPECT_LT(Game::Map::world_prop_ground_fraction(type),
              Game::Map::world_prop_model_half_extent(type))
        << "a tree that blocks its whole crown seals the woodland";
  }

  EXPECT_FALSE(Game::Map::world_prop_blocks_only_its_stem(WorldProp::Type::DeadTree))
      << "a fallen log lies on the ground; there is no crown to walk under";
}

TEST(PropModelFootprintTest, ARectangularPropDoesNotBlockItsShortFlank) {

  auto const ruins =
      Game::Map::world_prop_ground_half_extents(WorldProp::Type::Ruins, 1.0F);
  EXPECT_GT(ruins.x, ruins.z * 1.2F) << "the ruins body has stopped being a rectangle";

  constexpr float k_off_the_flank = 2.4F;
  EXPECT_LE(
      Game::Map::world_prop_overlap_depth(
          WorldProp::Type::Ruins, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, k_off_the_flank, 0.0F),
      0.0F)
      << "open ground beside a ruin is still being reserved";
  EXPECT_GT(Game::Map::world_prop_overlap_depth(
                WorldProp::Type::Ruins, 1.0F, 0.0F, 0.0F, 0.0F, 2.6F, 1.9F, 0.0F),
            0.0F)
      << "the plinth corner is still unclaimed, so scatter may stand on it";
}

TEST(PropModelFootprintTest, TheGuardCoversEveryPropBuiltFromParts) {
  auto const props = measured_props();
  EXPECT_EQ(props.size(), 9U)
      << "a prop was added to or removed from render/gl/backend/*_parts.h "
         "without updating this guard";
  for (auto const& prop : props) {
    EXPECT_GT(prop.reach.x, 0.0F) << prop.name << " measured as having no width at all";
    EXPECT_GT(prop.reach.z, 0.0F) << prop.name << " measured as having no depth at all";
  }
}
