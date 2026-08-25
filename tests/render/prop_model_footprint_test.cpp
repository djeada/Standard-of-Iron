

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <string_view>
#include <vector>

#include "game/map/map_definition.h"
#include "render/gl/backend/abandoned_home_parts.h"
#include "render/gl/backend/magic_shrine_parts.h"
#include "render/gl/backend/prop_parts.h"
#include "render/gl/backend/ruins_parts.h"
#include "render/gl/backend/statue_parts.h"
#include "render/gl/backend/supply_cart_parts.h"
#include "render/gl/backend/weapon_rack_parts.h"

namespace {

using Game::Map::WorldProp;
using namespace Render::GL::BackendPipelines;

auto ground_reach(const PropBoxPart& part) -> float {
  return std::max({std::abs(part.lo.x),
                   std::abs(part.hi.x),
                   std::abs(part.lo.z),
                   std::abs(part.hi.z)});
}

auto ground_reach(const PropOrientedBoxPart& part) -> float {
  float const spread = std::max(part.half_width, part.half_depth);
  return std::max({std::abs(part.a.x),
                   std::abs(part.b.x),
                   std::abs(part.a.z),
                   std::abs(part.b.z)}) +
         spread;
}

auto ground_reach(const PropBeamPart& part) -> float {
  float const spread = std::max(part.half_width, part.half_depth);
  return std::max({std::abs(part.a.x),
                   std::abs(part.b.x),
                   std::abs(part.a.z),
                   std::abs(part.b.z)}) +
         spread;
}

auto ground_reach(const PropLimbPart& part) -> float {
  float const spread = std::max(part.r0, part.r1);
  return std::max({std::abs(part.a.x),
                   std::abs(part.b.x),
                   std::abs(part.a.z),
                   std::abs(part.b.z)}) +
         spread;
}

auto ground_reach(const PropSlabPart& part) -> float {
  return std::max(part.half_bottom, part.half_top);
}

auto ground_reach(const PropFrustumPart& part) -> float {
  float const radius = std::max({part.rx0, part.rz0, part.rx1, part.rz1});
  return std::max(std::abs(part.cx), std::abs(part.cz)) + radius;
}

auto ground_reach(const PropTaperPart& part) -> float {
  return std::max(std::abs(part.cx), std::abs(part.cz)) + std::max(part.r0, part.r1);
}

auto ground_reach(const PropVertPrismPart& part) -> float {
  return std::max(std::abs(part.cx), std::abs(part.cz)) + part.r;
}

template <typename Parts>
auto widest(const Parts& parts) -> float {
  float reach = 0.0F;
  for (auto const& part : parts) {
    reach = std::max(reach, ground_reach(part));
  }
  return reach;
}

template <typename... Parts>
auto model_reach(const Parts&... parts) -> float {
  return std::max({widest(parts)...});
}

struct MeasuredProp {
  std::string_view name;
  WorldProp::Type type;
  float reach;
};

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
  };
}

} // namespace

TEST(PropModelFootprintTest, EveryPropDeclaresAnExtentItsModelFitsInside) {
  for (auto const& prop : measured_props()) {
    float const declared = Game::Map::world_prop_model_half_extent(prop.type);
    EXPECT_GE(declared, prop.reach)
        << prop.name << " draws out to " << prop.reach
        << " but declares a half extent of " << declared
        << "; the navigation grid would leave " << (prop.reach - declared)
        << " units of visible structure open ground";
  }
}

TEST(PropModelFootprintTest, NoPropBlocksFarMoreGroundThanItDraws) {
  for (auto const& prop : measured_props()) {
    float const declared = Game::Map::world_prop_model_half_extent(prop.type);
    EXPECT_LE(declared, prop.reach * 1.5F + 0.1F)
        << prop.name << " blocks a disc of " << declared << " while drawing only "
        << prop.reach;
  }
}

TEST(PropModelFootprintTest, GroundFractionFollowsTheModelExceptForCanopies) {
  for (auto const& prop : measured_props()) {
    ASSERT_FALSE(Game::Map::world_prop_blocks_only_its_stem(prop.type))
        << prop.name << " is not a canopy prop";
    EXPECT_FLOAT_EQ(Game::Map::world_prop_ground_fraction(prop.type),
                    Game::Map::world_prop_model_half_extent(prop.type))
        << prop.name << " blocks a different share of itself than it declares";
  }

  for (auto const type : {WorldProp::Type::PineTree,
                          WorldProp::Type::OliveTree,
                          WorldProp::Type::CypressTree,
                          WorldProp::Type::PalmTree,
                          WorldProp::Type::DeadTree}) {
    EXPECT_TRUE(Game::Map::world_prop_blocks_only_its_stem(type));
    EXPECT_LT(Game::Map::world_prop_ground_fraction(type),
              Game::Map::world_prop_model_half_extent(type))
        << "a tree that blocks its whole crown seals the woodland";
  }
}

TEST(PropModelFootprintTest, TheGuardCoversEveryPropBuiltFromParts) {
  auto const props = measured_props();
  EXPECT_EQ(props.size(), 6U)
      << "a prop was added to or removed from render/gl/backend/*_parts.h "
         "without updating this guard";
  for (auto const& prop : props) {
    EXPECT_GT(prop.reach, 0.0F) << prop.name << " measured as having no size at all";
  }
}
