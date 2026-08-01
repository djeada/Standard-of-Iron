#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <unordered_set>
#include <vector>

#include "core/component.h"
#include "core/world.h"
#include "formation/army_formation_planner.h"
#include "formation/army_formation_registry.h"
#include "formation/army_formation_service.h"
#include "formation/formation_doctrine.h"
#include "systems/command_service.h"
#include "systems/nation_registry.h"
#include "systems/pathfinding.h"
#include "systems/troop_profile_service.h"

namespace {

using Game::Formation::ArmyFormationIntent;
using Game::Formation::ArmyFormationPlan;
using Game::Formation::ArmyFormationPlanner;
using Game::Formation::ArmyFormationRegistry;
using Game::Formation::ArmyFormationRequest;
using Game::Formation::ArmyFormationService;
using Game::Formation::ArmyRole;
using Game::Formation::FlankPreference;
using Game::Formation::SlotStatus;
using Game::Systems::NationID;

auto add_unit(Engine::Core::World& world,
              Game::Units::SpawnType spawn_type,
              NationID nation_id,
              float x,
              float z = 0.0F) -> Engine::Core::EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  transform->position = {x, 0.0F, z};
  unit->spawn_type = spawn_type;
  unit->nation_id = nation_id;
  return entity->get_id();
}

auto slot_of(const ArmyFormationPlan& plan,
             Engine::Core::EntityID id) -> const Game::Formation::FormationSlot* {
  return plan.slot_for(id);
}

auto local_depth(const ArmyFormationPlan& plan, Engine::Core::EntityID id) -> float {
  const auto* slot = slot_of(plan, id);
  return slot == nullptr ? 0.0F : slot->local_offset.z();
}

auto local_lateral(const ArmyFormationPlan& plan, Engine::Core::EntityID id) -> float {
  const auto* slot = slot_of(plan, id);
  return slot == nullptr ? 0.0F : slot->local_offset.x();
}

auto lateral_span(const ArmyFormationPlan& plan) -> float {
  if (plan.slot_list.empty()) {
    return 0.0F;
  }
  auto const [min_it, max_it] = std::minmax_element(
      plan.slot_list.begin(), plan.slot_list.end(), [](const auto& a, const auto& b) {
        return a.local_offset.x() < b.local_offset.x();
      });
  return max_it->local_offset.x() - min_it->local_offset.x();
}

auto plan_for(Engine::Core::World& world,
              const std::vector<Engine::Core::EntityID>& units,
              ArmyFormationIntent intent,
              const QVector3D& anchor = QVector3D(0.0F, 0.0F, 20.0F),
              float facing = 0.0F) -> ArmyFormationPlan {
  ArmyFormationRequest request;
  request.members = units;
  request.anchor = anchor;
  request.facing = facing;
  request.intent = intent;
  request.spacing = 1.5F;
  request.preserve_previous_slots = false;
  return ArmyFormationPlanner::plan(world, request);
}

class ArmyFormationPlannerTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::CommandService::initialize(256, 256);
    auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
    if (pathfinder != nullptr) {
      pathfinder->update_navigation_grid();
    }

    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    nations.register_nation({.id = NationID::RomanRepublic,
                             .display_name = "Roman Republic",
                             .doctrine = "rome"});
    nations.register_nation(
        {.id = NationID::Carthage, .display_name = "Carthage", .doctrine = "carthage"});
    nations.register_nation({.id = NationID::IronSepulcher,
                             .display_name = "The Iron Sepulcher",
                             .doctrine = "iron_sepulcher"});
    Game::Systems::TroopProfileService::instance().clear();
    Game::Formation::DoctrineRegistry::instance().reset_to_defaults();
    ArmyFormationRegistry::instance().clear();
  }

  void TearDown() override {
    ArmyFormationRegistry::instance().clear();
    Game::Systems::TroopProfileService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }
};

} // namespace

TEST_F(ArmyFormationPlannerTest, PlacesEveryEligibleMemberExactlyOnce) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  Game::Units::SpawnType const spawns[] = {Game::Units::SpawnType::Knight,
                                           Game::Units::SpawnType::Spearman,
                                           Game::Units::SpawnType::Archer,
                                           Game::Units::SpawnType::MountedKnight,
                                           Game::Units::SpawnType::HorseArcher,
                                           Game::Units::SpawnType::Catapult,
                                           Game::Units::SpawnType::Elephant,
                                           Game::Units::SpawnType::Healer,
                                           Game::Units::SpawnType::Builder};
  float x = -8.0F;
  for (auto spawn : spawns) {
    units.push_back(add_unit(world, spawn, NationID::RomanRepublic, x));
    x += 2.0F;
  }

  auto const plan = plan_for(world, units, ArmyFormationIntent::FactionDefault);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;
  ASSERT_EQ(plan.slot_list.size(), units.size());

  std::unordered_set<Engine::Core::EntityID> seen;
  for (const auto& slot : plan.slot_list) {
    EXPECT_NE(slot.occupant, 0U);
    EXPECT_TRUE(seen.insert(slot.occupant).second) << "duplicate " << slot.occupant;
  }
  EXPECT_EQ(seen.size(), units.size());
}

TEST_F(ArmyFormationPlannerTest, RangedTroopsSitBehindTheMeleeFront) {
  Engine::Core::World world;
  auto const spear =
      add_unit(world, Game::Units::SpawnType::Spearman, NationID::RomanRepublic, -2.0F);
  auto const sword =
      add_unit(world, Game::Units::SpawnType::Knight, NationID::RomanRepublic, 0.0F);
  auto const archer =
      add_unit(world, Game::Units::SpawnType::Archer, NationID::RomanRepublic, 2.0F);

  auto const plan =
      plan_for(world, {spear, sword, archer}, ArmyFormationIntent::FactionDefault);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;

  EXPECT_GT(local_depth(plan, spear), local_depth(plan, archer));
  EXPECT_GT(local_depth(plan, sword), local_depth(plan, archer));
}

TEST_F(ArmyFormationPlannerTest, SiegeEnginesAreNotPlacedInTheFrontRank) {
  Engine::Core::World world;
  auto const sword =
      add_unit(world, Game::Units::SpawnType::Knight, NationID::RomanRepublic, -2.0F);
  auto const catapult =
      add_unit(world, Game::Units::SpawnType::Catapult, NationID::RomanRepublic, 0.0F);
  auto const ballista =
      add_unit(world, Game::Units::SpawnType::Ballista, NationID::RomanRepublic, 2.0F);

  auto const plan =
      plan_for(world, {sword, catapult, ballista}, ArmyFormationIntent::SiegeEscort);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;

  EXPECT_GT(local_depth(plan, sword), local_depth(plan, catapult));
  EXPECT_GT(local_depth(plan, sword), local_depth(plan, ballista));
  EXPECT_EQ(slot_of(plan, catapult)->role, ArmyRole::Siege);
}

TEST_F(ArmyFormationPlannerTest, CavalryIsSplitOntoBothFlanks) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 4; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             NationID::RomanRepublic,
                             static_cast<float>(i) - 2.0F));
  }
  auto const left_horse = add_unit(
      world, Game::Units::SpawnType::MountedKnight, NationID::RomanRepublic, -10.0F);
  auto const right_horse = add_unit(
      world, Game::Units::SpawnType::MountedKnight, NationID::RomanRepublic, 10.0F);
  units.push_back(left_horse);
  units.push_back(right_horse);

  auto const plan = plan_for(world, units, ArmyFormationIntent::FactionDefault);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;

  EXPECT_LT(local_lateral(plan, left_horse), 0.0F);
  EXPECT_GT(local_lateral(plan, right_horse), 0.0F);
  EXPECT_EQ(slot_of(plan, left_horse)->role, ArmyRole::LeftFlank);
  EXPECT_EQ(slot_of(plan, right_horse)->role, ArmyRole::RightFlank);
}

TEST_F(ArmyFormationPlannerTest, RomeAndCarthageProduceDistinctLinesForTheSameArmy) {
  auto build = [](Engine::Core::World& world, NationID nation) {
    std::vector<Engine::Core::EntityID> units;
    for (int i = 0; i < 6; ++i) {
      units.push_back(add_unit(
          world, Game::Units::SpawnType::Knight, nation, static_cast<float>(i) - 3.0F));
    }
    for (int i = 0; i < 3; ++i) {
      units.push_back(add_unit(world,
                               Game::Units::SpawnType::Archer,
                               nation,
                               static_cast<float>(i) - 1.0F,
                               -3.0F));
    }
    return units;
  };

  Engine::Core::World roman_world;
  auto const roman_units = build(roman_world, NationID::RomanRepublic);
  auto const roman_plan = plan_for(roman_world, roman_units, ArmyFormationIntent::Line);

  Engine::Core::World carthage_world;
  auto const carthage_units = build(carthage_world, NationID::Carthage);
  auto const carthage_plan =
      plan_for(carthage_world, carthage_units, ArmyFormationIntent::Line);

  ASSERT_TRUE(roman_plan.valid);
  ASSERT_TRUE(carthage_plan.valid);
  EXPECT_EQ(roman_plan.doctrine, "rome");
  EXPECT_EQ(carthage_plan.doctrine, "carthage");
  EXPECT_GT(lateral_span(carthage_plan), lateral_span(roman_plan));
}

TEST_F(ArmyFormationPlannerTest, IronSepulcherPutsExpendableTroopsInFront) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  auto const skeleton = add_unit(
      world, Game::Units::SpawnType::SkeletonSwordsman, NationID::IronSepulcher, -2.0F);
  auto const priest = add_unit(
      world, Game::Units::SpawnType::GravePriest, NationID::IronSepulcher, 0.0F);
  auto const skeleton_archer = add_unit(
      world, Game::Units::SpawnType::SkeletonArcher, NationID::IronSepulcher, 2.0F);
  units = {skeleton, priest, skeleton_archer};

  auto const plan = plan_for(world, units, ArmyFormationIntent::FactionDefault);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;
  EXPECT_EQ(plan.doctrine, "iron_sepulcher");
  EXPECT_GT(local_depth(plan, skeleton), local_depth(plan, priest));
  EXPECT_GT(local_depth(plan, skeleton), local_depth(plan, skeleton_archer));
}

TEST_F(ArmyFormationPlannerTest, LineIntentIsWiderThanColumnIntent) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 10; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             NationID::RomanRepublic,
                             static_cast<float>(i) - 5.0F));
  }

  auto const line = plan_for(world, units, ArmyFormationIntent::Line);
  auto const column = plan_for(world, units, ArmyFormationIntent::Column);
  ASSERT_TRUE(line.valid);
  ASSERT_TRUE(column.valid);
  EXPECT_GT(line.frontage, column.frontage);
  EXPECT_GT(column.depth, line.depth);
}

TEST_F(ArmyFormationPlannerTest, ExplicitFrontageOverridesTheTemplateWidth) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 8; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             NationID::RomanRepublic,
                             static_cast<float>(i) - 4.0F));
  }

  ArmyFormationRequest request;
  request.members = units;
  request.anchor = QVector3D(0.0F, 0.0F, 20.0F);
  request.intent = ArmyFormationIntent::Line;
  request.spacing = 1.5F;
  request.frontage = 30.0F;
  request.resolve_terrain = false;
  request.preserve_previous_slots = false;

  auto const plan = ArmyFormationPlanner::plan(world, request);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;
  EXPECT_NEAR(plan.frontage, 30.0F, 1.0F);
}

TEST_F(ArmyFormationPlannerTest, FlankPreferenceMovesTheCavalryWeight) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 4; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             NationID::RomanRepublic,
                             static_cast<float>(i) - 2.0F));
  }
  std::vector<Engine::Core::EntityID> horses;
  for (int i = 0; i < 6; ++i) {
    horses.push_back(add_unit(world,
                              Game::Units::SpawnType::MountedKnight,
                              NationID::RomanRepublic,
                              static_cast<float>(i) * 2.0F - 6.0F,
                              -4.0F));
  }
  units.insert(units.end(), horses.begin(), horses.end());

  auto count_side = [&](FlankPreference preference, float sign) {
    ArmyFormationRequest request;
    request.members = units;
    request.anchor = QVector3D(0.0F, 0.0F, 20.0F);
    request.intent = ArmyFormationIntent::FactionDefault;
    request.spacing = 1.5F;
    request.resolve_terrain = false;
    request.preserve_previous_slots = false;
    request.options.flank_preference = preference;
    auto const plan = ArmyFormationPlanner::plan(world, request);
    int count = 0;
    for (auto const horse : horses) {
      const auto* slot = plan.slot_for(horse);
      if (slot != nullptr && slot->local_offset.x() * sign > 0.0F) {
        ++count;
      }
    }
    return count;
  };

  EXPECT_GT(count_side(FlankPreference::StrongRight, 1.0F),
            count_side(FlankPreference::StrongLeft, 1.0F));
}

TEST_F(ArmyFormationPlannerTest, SiegeEscortIsRejectedWithoutSiegeEngines) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 4; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             NationID::RomanRepublic,
                             static_cast<float>(i) - 2.0F));
  }

  auto const plan = plan_for(world, units, ArmyFormationIntent::SiegeEscort);
  EXPECT_FALSE(plan.valid);
  EXPECT_FALSE(plan.rejection_reason.empty());
}

TEST_F(ArmyFormationPlannerTest, EncirclementIsRejectedWithoutCavalry) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 5; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             NationID::RomanRepublic,
                             static_cast<float>(i) - 2.0F));
  }

  auto const plan = plan_for(world, units, ArmyFormationIntent::Encirclement);
  EXPECT_FALSE(plan.valid);
  EXPECT_NE(plan.rejection_reason.find("cavalry"), std::string::npos);
}

TEST_F(ArmyFormationPlannerTest, MajorityDoctrineWinsForMixedSelections) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 5; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             NationID::Carthage,
                             static_cast<float>(i)));
  }
  units.push_back(
      add_unit(world, Game::Units::SpawnType::Knight, NationID::RomanRepublic, -4.0F));

  auto const plan = plan_for(world, units, ArmyFormationIntent::FactionDefault);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;
  EXPECT_EQ(plan.doctrine, "carthage");
}

TEST_F(ArmyFormationPlannerTest, CompositeByRoleIgnoresFactionBlocks) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 3; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             NationID::Carthage,
                             static_cast<float>(i)));
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Archer,
                             NationID::RomanRepublic,
                             static_cast<float>(i),
                             -4.0F));
  }

  ArmyFormationRequest request;
  request.members = units;
  request.anchor = QVector3D(0.0F, 0.0F, 20.0F);
  request.intent = ArmyFormationIntent::FactionDefault;
  request.spacing = 1.5F;
  request.resolve_terrain = false;
  request.preserve_previous_slots = false;
  request.options.mixed_policy = Game::Formation::MixedDoctrinePolicy::CompositeByRole;

  auto const plan = ArmyFormationPlanner::plan(world, request);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;
  EXPECT_EQ(plan.doctrine, Game::Formation::k_neutral_doctrine);
}

TEST_F(ArmyFormationPlannerTest, CommanderDoctrinePolicyFollowsTheCommander) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 4; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             NationID::Carthage,
                             static_cast<float>(i)));
  }
  units.push_back(add_unit(world,
                           Game::Units::SpawnType::RomanVeteranConsul,
                           NationID::RomanRepublic,
                           -4.0F));

  ArmyFormationRequest request;
  request.members = units;
  request.anchor = QVector3D(0.0F, 0.0F, 20.0F);
  request.intent = ArmyFormationIntent::FactionDefault;
  request.spacing = 1.5F;
  request.resolve_terrain = false;
  request.preserve_previous_slots = false;
  request.options.mixed_policy =
      Game::Formation::MixedDoctrinePolicy::CommanderDoctrine;

  auto const plan = ArmyFormationPlanner::plan(world, request);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;
  EXPECT_EQ(plan.doctrine, "rome");
}

TEST_F(ArmyFormationPlannerTest, BlockedSlotsNeverCollapseOntoOneFallbackPoint) {
  Game::Systems::CommandService::initialize(64, 64);
  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 6; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Archer,
                             NationID::RomanRepublic,
                             static_cast<float>(i) - 3.0F));
  }

  QVector3D const target(16.0F, 0.0F, 16.0F);
  auto const centre =
      Game::Systems::CommandService::world_to_grid(target.x(), target.z());
  for (int dz = -2; dz <= 2; ++dz) {
    for (int dx = -2; dx <= 2; ++dx) {
      if (dx == 0 && dz == 0) {
        continue;
      }
      pathfinder->set_obstacle(centre.x + dx, centre.y + dz, true);
    }
  }

  auto const plan = plan_for(world, units, ArmyFormationIntent::Line, target);

  std::vector<QVector3D> placed;
  for (const auto& slot : plan.slot_list) {
    if (slot.status == SlotStatus::Blocked) {
      continue;
    }
    for (const auto& other : placed) {
      float const dx = other.x() - slot.world_position.x();
      float const dz = other.z() - slot.world_position.z();
      EXPECT_GT(std::sqrt(dx * dx + dz * dz), 0.4F)
          << "two placed slots share a position";
    }
    placed.push_back(slot.world_position);
  }
}

TEST_F(ArmyFormationPlannerTest, SlotsAroundAnObstacleAreMarkedAdjustedNotBlocked) {
  Game::Systems::CommandService::initialize(64, 64);
  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 5; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             NationID::RomanRepublic,
                             static_cast<float>(i) - 2.0F));
  }

  QVector3D const target(20.0F, 0.0F, 20.0F);
  auto const centre =
      Game::Systems::CommandService::world_to_grid(target.x(), target.z());
  pathfinder->set_obstacle(centre.x, centre.y, true);
  pathfinder->set_obstacle(centre.x + 1, centre.y, true);

  auto const plan = plan_for(world, units, ArmyFormationIntent::Line, target);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;
  EXPECT_LT(plan.blocked_count, static_cast<int>(units.size()));
  for (const auto& slot : plan.slot_list) {
    if (slot.status == SlotStatus::Blocked) {
      continue;
    }
    EXPECT_TRUE(
        Game::Systems::CommandService::is_world_position_walkable(slot.world_position));
  }
}

TEST_F(ArmyFormationPlannerTest, FacingRotatesTheWholeDeployment) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 6; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             NationID::RomanRepublic,
                             static_cast<float>(i) - 3.0F));
  }

  QVector3D const anchor(0.0F, 0.0F, 0.0F);
  auto const north = plan_for(world, units, ArmyFormationIntent::Line, anchor, 0.0F);
  auto const east = plan_for(world, units, ArmyFormationIntent::Line, anchor, 90.0F);
  ASSERT_TRUE(north.valid);
  ASSERT_TRUE(east.valid);

  auto extent = [](const ArmyFormationPlan& plan, bool use_x) {
    float lo = 0.0F;
    float hi = 0.0F;
    for (const auto& slot : plan.slot_list) {
      float const value = use_x ? slot.world_position.x() : slot.world_position.z();
      lo = std::min(lo, value);
      hi = std::max(hi, value);
    }
    return hi - lo;
  };

  EXPECT_GT(extent(north, true), extent(north, false));
  EXPECT_GT(extent(east, false), extent(east, true));
  for (const auto& slot : east.slot_list) {
    EXPECT_FLOAT_EQ(slot.facing, 90.0F);
  }
}

TEST_F(ArmyFormationPlannerTest, ServiceReportsWhyAnIntentIsUnavailable) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 4; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             NationID::RomanRepublic,
                             static_cast<float>(i) - 2.0F));
  }

  EXPECT_TRUE(
      ArmyFormationService::availability(world, units, ArmyFormationIntent::Line)
          .empty());
  EXPECT_FALSE(
      ArmyFormationService::availability(world, units, ArmyFormationIntent::SiegeEscort)
          .empty());
}
