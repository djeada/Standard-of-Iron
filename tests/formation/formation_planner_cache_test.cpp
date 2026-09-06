#include <QVector3D>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
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
using Game::Formation::ArmyFormationPlanner;
using Game::Formation::ArmyFormationRegistry;
using Game::Formation::ArmyFormationRequest;
using Game::Systems::NationID;

class FormationPlannerCacheTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NavGrid::initialize(256, 256);
    auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
    if (pathfinder != nullptr) {
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
};

auto build_army(Engine::Core::World& world,
                int count) -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> ids;
  ids.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    auto* entity = world.create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    transform->position = {
        static_cast<float>(i % 16) + 40.0F, 0.0F, static_cast<float>(i / 16) + 40.0F};
    unit->spawn_type = Game::Units::SpawnType::Knight;
    unit->nation_id = NationID::RomanRepublic;
    ids.push_back(entity->get_id());
  }
  return ids;
}

auto make_request(const std::vector<Engine::Core::EntityID>& ids)
    -> ArmyFormationRequest {
  ArmyFormationRequest request;
  request.members = ids;
  request.anchor = QVector3D(120.0F, 0.0F, 120.0F);
  request.facing = 30.0F;
  request.intent = ArmyFormationIntent::Line;
  request.spacing = 1.0F;
  request.resolve_terrain = true;
  request.preserve_previous_slots = false;
  return request;
}

TEST_F(FormationPlannerCacheTest, BuildLayoutThenPlaceMatchesASinglePlanCall) {
  Engine::Core::World world;
  auto const ids = build_army(world, 24);
  auto const request = make_request(ids);
  auto const members = ArmyFormationPlanner::collect_members(world, ids);

  auto const direct = ArmyFormationPlanner::plan(members, request);
  auto const split = ArmyFormationPlanner::place(
      ArmyFormationPlanner::build_layout(members, request), request);

  ASSERT_TRUE(direct.valid);
  ASSERT_TRUE(split.valid);
  EXPECT_EQ(direct.doctrine, split.doctrine);
  EXPECT_EQ(direct.blocked_count, split.blocked_count);
  EXPECT_EQ(direct.adjusted_count, split.adjusted_count);
  EXPECT_FLOAT_EQ(direct.frontage, split.frontage);
  EXPECT_FLOAT_EQ(direct.depth, split.depth);
  EXPECT_FLOAT_EQ(direct.spacing, split.spacing);
  ASSERT_EQ(direct.slot_list.size(), split.slot_list.size());
  for (std::size_t i = 0; i < direct.slot_list.size(); ++i) {
    EXPECT_EQ(direct.slot_list[i].occupant, split.slot_list[i].occupant);
    EXPECT_EQ(direct.slot_list[i].status, split.slot_list[i].status);
    EXPECT_FLOAT_EQ(direct.slot_list[i].world_position.x(),
                    split.slot_list[i].world_position.x());
    EXPECT_FLOAT_EQ(direct.slot_list[i].world_position.z(),
                    split.slot_list[i].world_position.z());
  }
}

TEST_F(FormationPlannerCacheTest, SignatureIsStableForUnchangedInputs) {
  Engine::Core::World world;
  auto const ids = build_army(world, 12);
  auto const request = make_request(ids);
  auto const members = ArmyFormationPlanner::collect_members(world, ids);

  auto const first = ArmyFormationPlanner::layout_signature(members, request);
  EXPECT_EQ(first, ArmyFormationPlanner::layout_signature(members, request));

  auto moved = request;
  moved.anchor = QVector3D(60.0F, 0.0F, 30.0F);
  moved.facing = 175.0F;
  EXPECT_EQ(first, ArmyFormationPlanner::layout_signature(members, moved));
}

TEST_F(FormationPlannerCacheTest, SignatureMovesForEveryLayoutInput) {
  Engine::Core::World world;
  auto const ids = build_army(world, 12);
  auto const base = make_request(ids);
  auto const members = ArmyFormationPlanner::collect_members(world, ids);
  auto const base_signature = ArmyFormationPlanner::layout_signature(members, base);

  auto expect_differs = [&](const char* what, const ArmyFormationRequest& changed) {
    EXPECT_NE(base_signature, ArmyFormationPlanner::layout_signature(members, changed))
        << what << " must invalidate a cached layout";
  };

  auto intent = base;
  intent.intent = ArmyFormationIntent::Column;
  expect_differs("intent", intent);

  auto frontage = base;
  frontage.frontage = 22.0F;
  expect_differs("requested frontage", frontage);

  auto spacing = base;
  spacing.spacing = 1.7F;
  expect_differs("spacing", spacing);

  auto frontage_scale = base;
  frontage_scale.options.frontage_scale = 1.45F;
  expect_differs("frontage scale", frontage_scale);

  auto depth_scale = base;
  depth_scale.options.depth_scale = 1.45F;
  expect_differs("depth scale", depth_scale);

  auto spacing_scale = base;
  spacing_scale.options.spacing_scale = 0.75F;
  expect_differs("spacing scale", spacing_scale);

  auto flank = base;
  flank.options.flank_preference = Game::Formation::FlankPreference::StrongLeft;
  expect_differs("flank preference", flank);

  auto ranged = base;
  ranged.options.ranged_placement = Game::Formation::RangedPlacement::Skirmish;
  expect_differs("ranged placement", ranged);

  auto reserve = base;
  reserve.options.reserve_rows = 2;
  expect_differs("reserve rows", reserve);

  auto order = base;
  order.options.preserve_member_order = true;
  expect_differs("member order", order);

  auto mixed = base;
  mixed.options.mixed_policy = Game::Formation::MixedDoctrinePolicy::CompositeByRole;
  expect_differs("mixed policy", mixed);

  auto fewer = base;
  fewer.members.pop_back();
  auto const fewer_members =
      ArmyFormationPlanner::collect_members(world, fewer.members);
  EXPECT_NE(base_signature,
            ArmyFormationPlanner::layout_signature(fewer_members, fewer))
      << "losing a member must invalidate a cached layout";
}

TEST_F(FormationPlannerCacheTest, NoTwoSlotsLandOnTopOfEachOther) {
  Engine::Core::World world;
  auto const ids = build_army(world, 120);
  auto const request = make_request(ids);

  auto const plan = ArmyFormationPlanner::plan(world, request);
  ASSERT_TRUE(plan.valid);

  float const floor_distance = plan.spacing * 0.6F;
  int compared = 0;
  for (std::size_t i = 0; i < plan.slot_list.size(); ++i) {
    if (plan.slot_list[i].status == Game::Formation::SlotStatus::Blocked) {
      continue;
    }
    for (std::size_t j = i + 1; j < plan.slot_list.size(); ++j) {
      if (plan.slot_list[j].status == Game::Formation::SlotStatus::Blocked) {
        continue;
      }
      auto const delta =
          plan.slot_list[i].world_position - plan.slot_list[j].world_position;
      float const distance =
          std::sqrt((delta.x() * delta.x()) + (delta.z() * delta.z()));
      EXPECT_GE(distance, floor_distance * 0.999F)
          << "slots " << i << " and " << j << " overlap";
      ++compared;
    }
  }
  EXPECT_GT(compared, 0);
}

TEST_F(FormationPlannerCacheTest, RankAndFileCountsDescribeTheWholeBlock) {
  Engine::Core::World world;
  auto const ids = build_army(world, 24);
  auto request = make_request(ids);
  request.resolve_terrain = false;

  auto const plan = ArmyFormationPlanner::plan(world, request);
  ASSERT_TRUE(plan.valid);

  auto const bands = plan.depth_bands();
  ASSERT_FALSE(bands.empty());

  int total = 0;
  for (int const in_band : bands) {
    total += in_band;
    EXPECT_GT(in_band, 0);
  }
  EXPECT_EQ(total, static_cast<int>(plan.slot_list.size()))
      << "every slot must land in exactly one depth band";
  EXPECT_EQ(plan.rank_count(), static_cast<int>(bands.size()));
  EXPECT_EQ(plan.file_count(), *std::max_element(bands.begin(), bands.end()));

  auto const column_request = [&] {
    auto column = make_request(ids);
    column.resolve_terrain = false;
    column.intent = ArmyFormationIntent::Column;
    return column;
  }();
  auto const column = ArmyFormationPlanner::plan(world, column_request);
  ASSERT_TRUE(column.valid);

  EXPECT_GT(column.rank_count(), plan.rank_count())
      << "a column must read as deeper than a line";
  EXPECT_LT(column.file_count(), plan.file_count())
      << "a column must read as narrower than a line";
}

TEST_F(FormationPlannerCacheTest, ReportsPlanCostAcrossArmySizes) {
  constexpr int k_iterations = 200;

  for (int const count : {20, 40, 80, 160}) {
    Engine::Core::World world;
    auto const ids = build_army(world, count);

    ArmyFormationRequest request;
    request.members = ids;
    request.anchor = QVector3D(120.0F, 0.0F, 120.0F);
    request.facing = 30.0F;
    request.intent = ArmyFormationIntent::Line;
    request.spacing = 1.0F;
    request.resolve_terrain = true;
    request.preserve_previous_slots = false;

    auto const members = ArmyFormationPlanner::collect_members(world, ids);

    auto const full_start = std::chrono::steady_clock::now();
    for (int i = 0; i < k_iterations; ++i) {
      request.anchor.setX(120.0F + static_cast<float>(i % 7) * 0.01F);
      auto const plan = ArmyFormationPlanner::plan(members, request);
      ASSERT_TRUE(plan.valid);
    }
    auto const full_end = std::chrono::steady_clock::now();

    auto const layout = ArmyFormationPlanner::build_layout(members, request);
    auto const place_start = std::chrono::steady_clock::now();
    for (int i = 0; i < k_iterations; ++i) {
      request.anchor.setX(120.0F + static_cast<float>(i % 7) * 0.01F);
      auto const plan = ArmyFormationPlanner::place(layout, request);
      ASSERT_TRUE(plan.valid);
    }
    auto const place_end = std::chrono::steady_clock::now();

    auto const full_us =
        std::chrono::duration_cast<std::chrono::microseconds>(full_end - full_start)
            .count() /
        static_cast<double>(k_iterations);
    auto const place_us =
        std::chrono::duration_cast<std::chrono::microseconds>(place_end - place_start)
            .count() /
        static_cast<double>(k_iterations);

    std::printf(
        "units=%3d  full_plan=%7.1fus  place_only=%7.1fus\n", count, full_us, place_us);
  }
}

} // namespace
