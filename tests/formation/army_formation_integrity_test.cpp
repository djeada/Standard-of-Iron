#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <map>
#include <vector>

#include "core/component_core.h"
#include "core/world.h"
#include "formation/army_formation_planner.h"
#include "formation/army_formation_registry.h"
#include "formation/army_formation_service.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "systems/building_collision_registry.h"
#include "systems/nation_registry.h"
#include "systems/nav_grid.h"
#include "systems/pathfinding.h"
#include "systems/troop_profile_service.h"

namespace {

using Game::Formation::ArmyFormationIntent;
using Game::Formation::ArmyFormationMember;
using Game::Formation::ArmyFormationPlan;
using Game::Formation::ArmyFormationPlanner;
using Game::Formation::ArmyFormationRegistry;
using Game::Formation::ArmyFormationRequest;
using Game::Formation::ArmyFormationRuntime;
using Game::Formation::ArmyFormationService;
using Game::Formation::EntityID;
using Game::Formation::FormationSlot;
using Game::Formation::MovementPolicy;
using Game::Formation::SlotStatus;
using Game::Systems::NationID;
using Game::Systems::NavGrid;
using Game::Units::TroopType;

constexpr float k_pi = 3.14159265358979F;

auto add_unit(Engine::Core::World& world,
              Game::Units::SpawnType spawn_type,
              float x,
              float z) -> Engine::Core::EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  transform->position = {x, 0.0F, z};
  unit->spawn_type = spawn_type;
  unit->nation_id = NationID::RomanRepublic;
  unit->health = 100;
  unit->max_health = 100;
  unit->speed = 2.0F;
  return entity->get_id();
}

auto infantry(Engine::Core::World& world,
              int count,
              float z = -20.0F) -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < count; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Swordsman,
                             static_cast<float>(i) * 2.0F - static_cast<float>(count),
                             z));
  }
  return units;
}

auto mixed_members(int count) -> std::vector<ArmyFormationMember> {
  struct Kind {
    TroopType type;
    float half_width;
    float half_depth;
  };
  std::vector<Kind> const kinds{{TroopType::Swordsman, 1.6F, 2.8F},
                                {TroopType::Spearman, 2.2F, 1.4F},
                                {TroopType::Archer, 1.2F, 1.2F},
                                {TroopType::MountedSwordsman, 1.4F, 3.6F}};
  std::vector<ArmyFormationMember> members;
  for (int i = 0; i < count; ++i) {
    const auto& kind = kinds[static_cast<std::size_t>(i) % kinds.size()];
    auto member = ArmyFormationPlanner::make_member(
        static_cast<EntityID>(100 + i),
        kind.type,
        QVector3D(static_cast<float>(i) * 3.0F - 30.0F, 0.0F, -40.0F),
        "rome");
    member.half_width = kind.half_width;
    member.half_depth = kind.half_depth;
    members.push_back(member);
  }
  return members;
}

auto column_request(const std::vector<ArmyFormationMember>& members,
                    float facing) -> ArmyFormationRequest {
  ArmyFormationRequest request;
  for (const auto& member : members) {
    request.members.push_back(member.entity_id);
  }
  request.anchor = QVector3D(0.0F, 0.0F, 0.0F);
  request.facing = facing;
  request.intent = ArmyFormationIntent::Column;
  request.doctrine = "rome";
  request.spacing = 1.5F;
  return request;
}

void expect_disjoint(const std::vector<FormationSlot>& slot_list,
                     float gap,
                     const std::string& context) {
  auto const [first, second] = ArmyFormationPlanner::first_overlap(slot_list, gap);
  EXPECT_EQ(first, -1) << context << ": troops " << first << " and " << second
                       << " overlap";
}

auto sorted_offsets(const ArmyFormationPlan& plan) -> std::vector<std::pair<int, int>> {
  std::vector<std::pair<int, int>> offsets;
  for (const auto& slot : plan.slot_list) {
    offsets.emplace_back(static_cast<int>(std::lround(slot.local_offset.x() * 100.0F)),
                         static_cast<int>(std::lround(slot.local_offset.z() * 100.0F)));
  }
  std::sort(offsets.begin(), offsets.end());
  return offsets;
}

class ArmyFormationIntegrityTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    NavGrid::initialize(256, 256);
    if (auto* pathfinder = NavGrid::get_pathfinder()) {
      pathfinder->update_navigation_grid();
    }
    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    nations.register_nation({.id = NationID::RomanRepublic,
                             .display_name = "Roman Republic",
                             .doctrine = "rome"});
    Game::Systems::TroopProfileService::instance().clear();
    ArmyFormationRegistry::instance().clear();
  }

  void TearDown() override {
    ArmyFormationRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::TroopProfileService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  static void river_with_bridge() {
    Game::Map::MapDefinition map_def;
    map_def.grid.width = 121;
    map_def.grid.height = 121;
    map_def.grid.tile_size = 1.0F;
    map_def.coordSystem = Game::Map::CoordSystem::World;
    map_def.rivers.push_back(
        {QVector3D(-58.0F, 0.0F, 0.0F), QVector3D(58.0F, 0.0F, 0.0F), 8.0F});
    map_def.bridges.push_back(
        {QVector3D(0.0F, 0.0F, -6.0F), QVector3D(0.0F, 0.0F, 6.0F), 6.0F, 0.6F});
    Game::Map::TerrainService::instance().initialize(map_def);
    NavGrid::initialize(map_def.grid.width, map_def.grid.height);
    ASSERT_NE(NavGrid::get_pathfinder(), nullptr);
    NavGrid::get_pathfinder()->update_navigation_grid();
  }
};

} // namespace

TEST_F(ArmyFormationIntegrityTest, ChoosingColumnAppliesTheDoctrineMarchPolicy) {
  Engine::Core::World world;
  auto const units = infantry(world, 6);

  ArmyFormationRequest request;
  request.members = units;
  request.anchor = QVector3D(0.0F, 0.0F, 20.0F);
  request.intent = ArmyFormationIntent::Column;
  request.spacing = 1.5F;
  ASSERT_EQ(request.options.movement_policy, MovementPolicy::DoctrineDefault);

  auto const result = ArmyFormationService::commit(world, request);
  ASSERT_TRUE(result.valid) << result.rejection_reason;
  EXPECT_EQ(result.movement_policy, MovementPolicy::MaintainFormation)
      << "the column template declares maintain_formation";
  const auto* formation = ArmyFormationRegistry::instance().find(result.group_id);
  ASSERT_NE(formation, nullptr);
  EXPECT_TRUE(formation->maintains_formation());
}

TEST_F(ArmyFormationIntegrityTest, APlayerMarchChoiceOverridesTheDoctrine) {
  Engine::Core::World world;
  auto const units = infantry(world, 6);

  ArmyFormationRequest request;
  request.members = units;
  request.anchor = QVector3D(0.0F, 0.0F, 20.0F);
  request.intent = ArmyFormationIntent::Column;
  request.spacing = 1.5F;
  request.options.movement_policy = MovementPolicy::ReformAtDestination;

  auto const result = ArmyFormationService::commit(world, request);
  ASSERT_TRUE(result.valid) << result.rejection_reason;
  EXPECT_EQ(result.movement_policy, MovementPolicy::ReformAtDestination);
  const auto* formation = ArmyFormationRegistry::instance().find(result.group_id);
  ASSERT_NE(formation, nullptr);
  EXPECT_FALSE(formation->maintains_formation());
}

TEST_F(ArmyFormationIntegrityTest, OpenGroundColumnHoldsOneSilhouetteAtEveryHeading) {
  for (int const count : {4, 9, 16}) {
    auto const members = mixed_members(count);
    auto const reference =
        ArmyFormationPlanner::plan(members, column_request(members, 0.0F));
    ASSERT_TRUE(reference.valid) << reference.rejection_reason;
    auto const reference_shape = sorted_offsets(reference);

    for (float const facing : {0.0F, 37.0F, 90.0F, 180.0F, -135.0F}) {
      std::string const context =
          std::to_string(count) + " troops facing " + std::to_string(facing);
      auto const request = column_request(members, facing);
      auto const plan = ArmyFormationPlanner::plan(members, request);
      ASSERT_TRUE(plan.valid) << context << ": " << plan.rejection_reason;

      EXPECT_EQ(plan.blocked_count, 0) << context;
      EXPECT_EQ(plan.adjusted_count, 0) << context << ": open ground needs no fitting";
      EXPECT_FALSE(plan.narrowed) << context;
      EXPECT_GT(plan.footprint_gap, 0.0F);
      expect_disjoint(plan.slot_list, plan.footprint_gap, context);

      EXPECT_EQ(sorted_offsets(plan), reference_shape)
          << context << ": the column shape must not depend on its heading";

      float const yaw = facing * k_pi / 180.0F;
      for (const auto& slot : plan.slot_list) {
        EXPECT_FLOAT_EQ(slot.facing, facing) << context;
        QVector3D const expected(slot.local_offset.x() * std::cos(yaw) +
                                     slot.local_offset.z() * std::sin(yaw),
                                 0.0F,
                                 -slot.local_offset.x() * std::sin(yaw) +
                                     slot.local_offset.z() * std::cos(yaw));
        EXPECT_NEAR(slot.world_position.x(), plan.anchor.x() + expected.x(), 1.0e-3F)
            << context;
        EXPECT_NEAR(slot.world_position.z(), plan.anchor.z() + expected.z(), 1.0e-3F)
            << context;
      }

      auto const again = ArmyFormationPlanner::plan(members, request);
      ASSERT_EQ(again.slot_list.size(), plan.slot_list.size());
      for (std::size_t i = 0; i < plan.slot_list.size(); ++i) {
        EXPECT_EQ(again.slot_list[i].occupant, plan.slot_list[i].occupant) << context;
        EXPECT_EQ(again.slot_list[i].world_position, plan.slot_list[i].world_position)
            << context << ": planning must be deterministic";
      }
    }
  }
}

TEST_F(ArmyFormationIntegrityTest, AnInfantryColumnIsDeeperThanItIsWide) {
  Engine::Core::World world;
  auto const units = infantry(world, 12);
  ArmyFormationRequest request;
  request.members = units;
  request.anchor = QVector3D(0.0F, 0.0F, 30.0F);
  request.intent = ArmyFormationIntent::Column;
  request.spacing = 1.5F;

  auto const plan = ArmyFormationPlanner::plan(world, request);
  ASSERT_TRUE(plan.valid) << plan.rejection_reason;
  EXPECT_GT(plan.depth, plan.frontage);
  EXPECT_GE(plan.rank_count(), 3);
  EXPECT_LE(plan.file_count(), 4);
  expect_disjoint(plan.slot_list, plan.footprint_gap, "column of 12");
}

TEST_F(ArmyFormationIntegrityTest, KeepingPreviousSlotsNeverReopensAnOverlap) {
  auto const members = mixed_members(12);
  auto request = column_request(members, 0.0F);
  auto const first = ArmyFormationPlanner::plan(members, request);
  ASSERT_TRUE(first.valid);

  Game::Formation::ArmyFormation previous;
  previous.id = 7;
  previous.slot_list = first.slot_list;
  for (std::size_t i = 0; i < previous.slot_list.size(); ++i) {
    previous.slot_list[i].occupant =
        first.slot_list[(i + 1) % first.slot_list.size()].occupant;
  }
  request.group_id = previous.id;
  request.preserve_previous_slots = true;

  auto const replanned = ArmyFormationPlanner::plan(members, request, &previous);
  ASSERT_TRUE(replanned.valid);
  expect_disjoint(replanned.slot_list, replanned.footprint_gap, "after reassignment");

  std::map<EntityID, const ArmyFormationMember*> by_id;
  for (const auto& member : members) {
    by_id[member.entity_id] = &member;
  }
  for (const auto& slot : replanned.slot_list) {
    ASSERT_NE(by_id.count(slot.occupant), 0U);
    EXPECT_FLOAT_EQ(slot.half_width, by_id[slot.occupant]->half_width)
        << "a slot must be sized for the troop standing in it";
    EXPECT_FLOAT_EQ(slot.half_depth, by_id[slot.occupant]->half_depth);
  }
}

TEST_F(ArmyFormationIntegrityTest, AnUnfieldableFormationIsRejectedNotScattered) {
  Engine::Core::World world;
  auto const units = infantry(world, 5);

  ArmyFormationRequest request;
  request.members = units;
  request.anchor = QVector3D(4.0F, 0.0F, 12.0F);
  request.intent = ArmyFormationIntent::Encirclement;
  request.spacing = 1.5F;

  auto const preview = ArmyFormationService::preview(world, request);
  EXPECT_FALSE(preview.valid);
  EXPECT_FALSE(preview.rejection_reason.empty());
  for (std::size_t i = 0; i < units.size(); ++i) {
    EXPECT_EQ(preview.slot_status[i], SlotStatus::Blocked);
    EXPECT_EQ(preview.positions[i], request.anchor)
        << "no improvised positions may leak out of a rejected plan";
  }

  auto const members = ArmyFormationPlanner::collect_members(world, units);
  auto const placements = ArmyFormationService::placements_for(members, request);
  ASSERT_EQ(placements.size(), members.size());
  auto const fallback_request = [&] {
    auto copy = request;
    copy.intent = ArmyFormationIntent::FactionDefault;
    return copy;
  }();
  auto const fallback = ArmyFormationPlanner::plan(members, fallback_request);
  ASSERT_TRUE(fallback.valid);
  for (std::size_t i = 0; i < members.size(); ++i) {
    const auto* slot = fallback.slot_for(members[i].entity_id);
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(placements[i].position, slot->world_position)
        << "the fallback is the doctrine's own line, not a scatter";
  }
}

TEST_F(ArmyFormationIntegrityTest, AMarchingGroupKeepsItsSlotsThroughATurn) {
  Engine::Core::World world;
  auto const units = infantry(world, 8, 0.0F);

  ArmyFormationRequest request;
  request.members = units;
  request.anchor = QVector3D(0.0F, 0.0F, 40.0F);
  request.intent = ArmyFormationIntent::Column;
  request.spacing = 1.5F;
  auto const result = ArmyFormationService::commit(world, request);
  ASSERT_TRUE(result.valid) << result.rejection_reason;

  auto& registry = ArmyFormationRegistry::instance();
  auto* formation = registry.find(result.group_id);
  ASSERT_NE(formation, nullptr);
  ASSERT_TRUE(formation->maintains_formation());
  ASSERT_FALSE(formation->reference_slots.empty());

  std::map<EntityID, int> slot_of;
  std::map<EntityID, QVector3D> local_of;
  for (const auto& slot : formation->slot_list) {
    slot_of[slot.occupant] = slot.id;
    local_of[slot.occupant] = slot.local_offset;
  }

  for (float const facing : {0.0F, 0.0F, 30.0F, 60.0F, 90.0F, 90.0F}) {
    formation = registry.find(result.group_id);
    ASSERT_NE(formation, nullptr);
    formation->facing = facing;
    formation->anchor += QVector3D(0.0F, 0.0F, 2.0F);
    auto const revision = formation->plan_revision;
    ASSERT_TRUE(ArmyFormationRuntime::replan(world, result.group_id));
    formation = registry.find(result.group_id);
    ASSERT_NE(formation, nullptr);
    EXPECT_GT(formation->plan_revision, revision);
    EXPECT_FALSE(formation->compressed);
    for (const auto& slot : formation->slot_list) {
      EXPECT_EQ(slot.id, slot_of[slot.occupant])
          << "rank or file swapped at " << facing;
      EXPECT_EQ(slot.local_offset, local_of[slot.occupant])
          << "the shape drifted at " << facing;
      EXPECT_FLOAT_EQ(slot.facing, facing);
    }
    expect_disjoint(formation->slot_list, 0.0F, "while wheeling");
  }
}

TEST_F(ArmyFormationIntegrityTest, LosingATroopReplansWithoutOverlap) {
  Engine::Core::World world;
  auto const units = infantry(world, 9, 0.0F);
  ArmyFormationRequest request;
  request.members = units;
  request.anchor = QVector3D(0.0F, 0.0F, 30.0F);
  request.intent = ArmyFormationIntent::Column;
  request.spacing = 1.5F;
  auto const result = ArmyFormationService::commit(world, request);
  ASSERT_TRUE(result.valid);

  auto& registry = ArmyFormationRegistry::instance();
  ASSERT_TRUE(registry.remove_member(units[4]));
  ASSERT_TRUE(ArmyFormationRuntime::replan(world, result.group_id));
  const auto* formation = registry.find(result.group_id);
  ASSERT_NE(formation, nullptr);
  EXPECT_EQ(formation->members.size(), units.size() - 1U);
  EXPECT_TRUE(ArmyFormationRuntime::reference_matches_members(*formation))
      << "the survivors form a new reference shape";
  for (const auto& slot : formation->slot_list) {
    EXPECT_NE(slot.occupant, units[4]);
  }
  expect_disjoint(formation->slot_list, 0.0F, "after a loss");
}

TEST_F(ArmyFormationIntegrityTest,
       ABridgeCompressesTheFormationAndOpenGroundReformsIt) {
  river_with_bridge();

  Engine::Core::World world;
  auto const units = infantry(world, 6, -24.0F);
  ArmyFormationRequest request;
  request.members = units;
  request.anchor = QVector3D(0.0F, 0.0F, -24.0F);
  request.intent = ArmyFormationIntent::Line;
  request.spacing = 1.5F;
  request.options.movement_policy = MovementPolicy::MaintainFormation;
  auto const result = ArmyFormationService::commit(world, request);
  ASSERT_TRUE(result.valid) << result.rejection_reason;

  auto& registry = ArmyFormationRegistry::instance();
  auto* formation = registry.find(result.group_id);
  ASSERT_NE(formation, nullptr);
  ASSERT_FALSE(formation->compressed);
  auto const reference = formation->reference_slots;
  ASSERT_FALSE(reference.empty());

  auto* pathfinder = NavGrid::get_pathfinder();
  formation->anchor = QVector3D(0.0F, 0.0F, 0.0F);
  formation->facing = 0.0F;
  ASSERT_TRUE(ArmyFormationRuntime::replan(world, result.group_id));
  formation = registry.find(result.group_id);
  ASSERT_NE(formation, nullptr);
  EXPECT_TRUE(formation->compressed) << "a line cannot cross a 6 m bridge abreast";
  expect_disjoint(formation->slot_list, 0.0F, "on the bridge");
  for (const auto& slot : formation->slot_list) {
    if (slot.status == SlotStatus::Blocked) {
      continue;
    }
    auto const cell =
        pathfinder->world_to_grid(slot.world_position.x(), slot.world_position.z());
    EXPECT_TRUE(pathfinder->is_walkable(cell.x, cell.y))
        << "slot in the river at " << slot.world_position.x() << ", "
        << slot.world_position.z();
  }

  formation->anchor = QVector3D(0.0F, 0.0F, 24.0F);
  ASSERT_TRUE(ArmyFormationRuntime::replan(world, result.group_id));
  formation = registry.find(result.group_id);
  ASSERT_NE(formation, nullptr);
  EXPECT_FALSE(formation->compressed) << "open ground restores the chosen shape";
  ASSERT_EQ(formation->slot_list.size(), reference.size());
  for (std::size_t i = 0; i < reference.size(); ++i) {
    EXPECT_EQ(formation->slot_list[i].occupant, reference[i].occupant);
    EXPECT_EQ(formation->slot_list[i].local_offset, reference[i].local_offset);
  }
}
