#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <string>

#include "render/entity/building_archetype_catalog.h"
#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_state.h"
#include "render/entity/nation_motifs.h"

namespace {

using Render::GL::BuildingArchetypeDesc;
using Render::GL::BuildingState;
using Render::GL::BuildingStateMask;
namespace Motif = Render::GL::Motif;

auto count_named(const BuildingArchetypeDesc& desc, std::string_view name) -> int {
  return static_cast<int>(
      std::count_if(desc.parts().begin(), desc.parts().end(), [&](const auto& part) {
        return part.name == name;
      }));
}

} // namespace

TEST(NationMotifSurface, FindsTheTopOfWhateverStandsUnderAPoint) {
  BuildingArchetypeDesc desc("surface_probe");
  desc.add_box(QVector3D(0.0F, 0.1F, 0.0F), QVector3D(1.0F, 0.1F, 1.0F), {});
  desc.add_box(QVector3D(0.5F, 0.6F, 0.0F), QVector3D(0.2F, 0.4F, 0.2F), {});

  desc.add_rotated_box(QVector3D(0.0F, 2.0F, 0.0F),
                       QVector3D(1.0F, 0.02F, 0.5F),
                       QVector3D(30.0F, 0.0F, 0.0F),
                       {});
  desc.add_cylinder(
      QVector3D(-0.6F, 0.2F, -0.6F), QVector3D(-0.6F, 0.9F, -0.6F), 0.1F, {});

  desc.add_box(QVector3D(0.0F, 3.0F, 0.0F),
               QVector3D(0.3F, 0.3F, 0.3F),
               {},
               BuildingStateMask::Damaged);

  EXPECT_FLOAT_EQ(Motif::surface_top(desc, -0.9F, 0.9F), 0.2F) << "the slab";
  EXPECT_FLOAT_EQ(Motif::surface_top(desc, 5.0F, 5.0F), 0.0F) << "nothing: the ground";
  EXPECT_FLOAT_EQ(Motif::surface_top(desc, -0.6F, -0.6F, 1.5F), 0.9F) << "the post";

  EXPECT_GT(Motif::surface_top(desc, 0.5F, 0.0F), 1.9F);
  EXPECT_FLOAT_EQ(Motif::surface_top(desc, 0.5F, 0.0F, 1.5F), 1.0F) << "the pier";

  const float high = Motif::surface_top(desc, 0.0F, 0.3F);
  const float low = Motif::surface_top(desc, 0.0F, -0.3F);
  EXPECT_NEAR(std::abs(high - low), 2.0F * 0.3F * std::tan(30.0F * 0.0174533F), 0.01F);

  const Motif::Setter whole(desc, BuildingState::Normal);
  const Motif::Setter damaged(desc, BuildingState::Damaged);
  EXPECT_LT(whole.at(0.0F, 0.0F).y(), 2.5F);
  EXPECT_FLOAT_EQ(damaged.at(0.0F, 0.0F).y(), 3.3F)
      << "an ornament is set down on the building as it stands in that state";
  EXPECT_GT(whole.highest(-1.0F, 1.0F, -1.0F, 1.0F).y(), 2.0F);
}

TEST(NationMotifs, EveryNationKeepsItsOwnFireAtItsOwnBuildings) {
  struct Expectation {
    const char* name;
    bool ghost;
  };
  for (const Expectation& building : {Expectation{"roman_home", false},
                                      Expectation{"roman_barracks", false},
                                      Expectation{"roman_defense_tower", false},
                                      Expectation{"roman_marketplace", false},
                                      Expectation{"roman_temple", false},
                                      Expectation{"roman_dock_variant_0", false},
                                      Expectation{"carthage_home", true},
                                      Expectation{"carthage_barracks", true},
                                      Expectation{"carthage_defense_tower", true},
                                      Expectation{"carthage_marketplace", true},
                                      Expectation{"carthage_temple", true},
                                      Expectation{"carthage_dock_variant_0", true}}) {
    bool found = false;
    for (const auto& entry : Render::GL::building_archetype_catalog()) {
      if (entry.name != building.name) {
        continue;
      }
      found = true;
      const BuildingArchetypeDesc desc = entry.build(BuildingState::Normal);
      const int orange = count_named(desc, Motif::k_fire_label);
      const int ghost = count_named(desc, Motif::k_ghost_fire_label);
      EXPECT_GT(building.ghost ? ghost : orange, 0)
          << building.name << " has lost its nation's fire";
      EXPECT_EQ(building.ghost ? orange : ghost, 0)
          << building.name << " burns the other nation's fire";

      const BuildingArchetypeDesc ruin = entry.build(BuildingState::Destroyed);
      for (const auto& part : ruin.parts()) {
        if (part.name.starts_with(Motif::k_fire_label)) {
          EXPECT_EQ(part.states & BuildingStateMask::Destroyed, BuildingStateMask::None)
              << building.name;
        }
      }
    }
    EXPECT_TRUE(found) << building.name << " is not in the bake catalog";
  }
}

TEST(NationMotifs, AFireIsAsBigAsItsBowl) {
  BuildingArchetypeDesc desc("fires");
  Motif::add_roman_tripod_fire(desc, QVector3D(0.0F, 0.0F, 0.0F), 2.0F);
  Motif::add_punic_incense_stand(desc, QVector3D(1.0F, 0.0F, 0.0F), 0.5F);
  int fires = 0;
  for (const auto& part : desc.parts()) {
    if (part.name == Motif::k_fire_label) {
      ++fires;
      EXPECT_FLOAT_EQ(part.radius / Motif::k_fire_radius, 2.0F);
      EXPECT_GT(part.point_a.y(), 0.5F) << "the flame starts at the rim of the bowl";
    } else if (part.name == Motif::k_ghost_fire_label) {
      ++fires;
      EXPECT_FLOAT_EQ(part.radius / Motif::k_fire_radius, 0.5F);
    }
  }
  EXPECT_EQ(fires, 2);
}
