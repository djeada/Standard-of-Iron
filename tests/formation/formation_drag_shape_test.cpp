

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

#include "core/component_core.h"
#include "core/world.h"
#include "formation/army_formation_planner.h"
#include "formation/army_formation_registry.h"
#include "formation/formation_doctrine.h"
#include "systems/nation_registry.h"
#include "systems/nav_grid.h"
#include "systems/pathfinding.h"
#include "systems/troop_profile_service.h"

namespace {

using Game::Formation::ArmyFormationIntent;
using Game::Formation::ArmyFormationPlan;
using Game::Formation::ArmyFormationPlanner;
using Game::Formation::ArmyFormationRegistry;
using Game::Formation::ArmyFormationRequest;
using Game::Formation::ArmyRole;
using Game::Formation::RangedPlacement;
using Game::Systems::NationID;

class FormationDragShapeTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NavGrid::initialize(256, 256);
    if (auto* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {
      pathfinder->update_navigation_grid();
    }
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    nations.register_nation({.id = NationID::RomanRepublic,
                             .display_name = "Roman Republic",
                             .doctrine = "rome"});
    Game::Systems::TroopProfileService::instance().clear();
    Game::Formation::DoctrineRegistry::instance().reset_to_defaults();
    ArmyFormationRegistry::instance().clear();
  }

  void TearDown() override {
    ArmyFormationRegistry::instance().clear();
    Game::Systems::TroopProfileService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  static auto
  mixed_army(Engine::Core::World& world) -> std::vector<Engine::Core::EntityID> {
    std::vector<Engine::Core::EntityID> ids;
    auto add = [&](Game::Units::SpawnType spawn, int count) {
      for (int i = 0; i < count; ++i) {
        auto* entity = world.create_entity();
        auto* transform = entity->add_component<Engine::Core::TransformComponent>();
        auto* unit = entity->add_component<Engine::Core::UnitComponent>();
        transform->position = {static_cast<float>(ids.size() % 8) * 3.0F,
                               0.0F,
                               static_cast<float>(ids.size() / 8) * 3.0F};
        unit->spawn_type = spawn;
        unit->nation_id = NationID::RomanRepublic;
        ids.push_back(entity->get_id());
      }
    };
    add(Game::Units::SpawnType::Swordsman, 16);
    add(Game::Units::SpawnType::Spearman, 11);
    add(Game::Units::SpawnType::Archer, 8);
    add(Game::Units::SpawnType::MountedSwordsman, 4);
    return ids;
  }

  static auto plan_with(Engine::Core::World& world,
                        const std::vector<Engine::Core::EntityID>& ids,
                        ArmyFormationIntent intent,
                        float frontage) -> ArmyFormationPlan {
    ArmyFormationRequest request;
    request.members = ids;
    request.anchor = QVector3D(0.0F, 0.0F, 0.0F);
    request.facing = 0.0F;
    request.frontage = frontage;
    request.intent = intent;
    request.spacing = 1.0F;
    request.resolve_terrain = false;
    request.preserve_previous_slots = false;
    return ArmyFormationPlanner::plan(world, request);
  }

  static auto tightest_gap(const ArmyFormationPlan& plan) -> float {
    float tightest = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < plan.slot_list.size(); ++i) {
      for (std::size_t j = i + 1; j < plan.slot_list.size(); ++j) {
        auto const delta =
            plan.slot_list[i].local_offset - plan.slot_list[j].local_offset;
        tightest = std::min(tightest, delta.length());
      }
    }
    return tightest;
  }
};

TEST_F(FormationDragShapeTest, ANarrowDragDeepensTheBlockInsteadOfCrushingIt) {
  Engine::Core::World world;
  auto const ids = mixed_army(world);

  auto const wide = plan_with(world, ids, ArmyFormationIntent::Line, 90.0F);
  auto const narrow = plan_with(world, ids, ArmyFormationIntent::Line, 15.0F);
  ASSERT_TRUE(wide.valid) << wide.rejection_reason;
  ASSERT_TRUE(narrow.valid) << narrow.rejection_reason;

  EXPECT_LT(narrow.file_count(), wide.file_count())
      << "a narrower drag must put fewer troops abreast";
  EXPECT_GT(narrow.rank_count(), wide.rank_count())
      << "the troops it takes out of the front rank have to go somewhere";
  EXPECT_GT(narrow.depth, wide.depth);
  EXPECT_LT(narrow.frontage, wide.frontage);
}

TEST_F(FormationDragShapeTest, TheDragWidthIsWhatTheBlockEndsUpBeing) {
  Engine::Core::World world;
  auto const ids = mixed_army(world);

  float previous = 0.0F;
  for (float const frontage : {12.0F, 25.0F, 45.0F, 80.0F}) {
    auto const plan = plan_with(world, ids, ArmyFormationIntent::Line, frontage);
    ASSERT_TRUE(plan.valid) << plan.rejection_reason;
    EXPECT_GT(plan.frontage, previous)
        << "a " << frontage << " m drag was not wider than the one before it";
    previous = plan.frontage;
  }
}

TEST_F(FormationDragShapeTest, TheReportedSlotSpacingIsWhatTheSlotsActuallyUse) {
  Engine::Core::World world;
  auto const ids = mixed_army(world);
  auto const plan = plan_with(world, ids, ArmyFormationIntent::Line, 0.0F);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;

  EXPECT_GT(plan.slot_spacing, plan.spacing);

  ASSERT_EQ(plan.slot_half_width.size(), plan.slot_list.size());
  ASSERT_EQ(plan.slot_half_depth.size(), plan.slot_list.size());
  float widest = 0.0F;
  for (auto const half : plan.slot_half_width) {
    widest = std::max(widest, half);
  }
  EXPECT_GT(widest, 2.0F) << "a squad of men is metres wide, not a token";
}

TEST_F(FormationDragShapeTest, MissileTroopsGoWhereThePlacementSaysTheyDo) {
  Engine::Core::World world;
  auto const ids = mixed_army(world);

  auto depth_of = [&](RangedPlacement placement) {
    ArmyFormationRequest request;
    request.members = ids;
    request.anchor = QVector3D(0.0F, 0.0F, 0.0F);
    request.intent = ArmyFormationIntent::Line;
    request.spacing = 1.0F;
    request.resolve_terrain = false;
    request.preserve_previous_slots = false;
    request.options.ranged_placement = placement;
    auto const plan = ArmyFormationPlanner::plan(world, request);
    EXPECT_TRUE(plan.valid) << plan.rejection_reason;
    float total = 0.0F;
    int count = 0;
    for (const auto& slot : plan.slot_list) {
      if (slot.role == ArmyRole::Ranged) {
        total += slot.local_offset.z();
        ++count;
      }
    }
    EXPECT_GT(count, 0);
    return count > 0 ? total / static_cast<float>(count) : 0.0F;
  };

  float const rear = depth_of(RangedPlacement::Rear);
  float const front = depth_of(RangedPlacement::Front);
  float const skirmish = depth_of(RangedPlacement::Skirmish);

  EXPECT_GT(front, rear) << "\"In front\" has to put the archers in front";
  EXPECT_GT(skirmish, front) << "skirmishers screen ahead of the front rank";
}

} // namespace
