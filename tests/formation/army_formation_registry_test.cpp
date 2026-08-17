#include <QJsonObject>
#include <QVector3D>

#include <gtest/gtest.h>
#include <vector>

#include "core/component.h"
#include "core/world.h"
#include "formation/army_formation_registry.h"
#include "formation/army_formation_service.h"
#include "systems/nation_registry.h"
#include "systems/nav_grid.h"
#include "systems/pathfinding.h"
#include "systems/troop_profile_service.h"

namespace {

using Game::Formation::ArmyFormationIntent;
using Game::Formation::ArmyFormationRegistry;
using Game::Formation::ArmyFormationRequest;
using Game::Formation::ArmyFormationRuntime;
using Game::Formation::ArmyFormationService;
using Game::Formation::k_invalid_group;
using Game::Systems::NationID;

auto add_unit(Engine::Core::World& world,
              Game::Units::SpawnType spawn_type,
              float x) -> Engine::Core::EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  transform->position = {x, 0.0F, 0.0F};
  unit->spawn_type = spawn_type;
  unit->nation_id = NationID::RomanRepublic;
  unit->health = 100;
  unit->max_health = 100;
  return entity->get_id();
}

auto commit(Engine::Core::World& world,
            const std::vector<Engine::Core::EntityID>& units,
            ArmyFormationIntent intent = ArmyFormationIntent::Line) {
  ArmyFormationRequest request;
  request.members = units;
  request.anchor = QVector3D(0.0F, 0.0F, 12.0F);
  request.intent = intent;
  request.spacing = 1.5F;
  return ArmyFormationService::commit(world, request);
}

class ArmyFormationRegistryTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NavGrid::initialize(128, 128);
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
    ArmyFormationRegistry::instance().clear();
  }

  void TearDown() override {
    ArmyFormationRegistry::instance().clear();
    Game::Systems::TroopProfileService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }
};

} // namespace

TEST_F(ArmyFormationRegistryTest, CommitCreatesOneGroupOwningEveryMember) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 5; ++i) {
    units.push_back(
        add_unit(world, Game::Units::SpawnType::Knight, static_cast<float>(i) - 2.0F));
  }

  auto const result = commit(world, units);
  ASSERT_TRUE(result.valid) << result.rejection_reason;
  ASSERT_NE(result.group_id, k_invalid_group);

  auto& registry = ArmyFormationRegistry::instance();
  EXPECT_EQ(registry.group_count(), 1U);

  const auto* formation = registry.find(result.group_id);
  ASSERT_NE(formation, nullptr);
  EXPECT_EQ(formation->members.size(), units.size());
  EXPECT_EQ(formation->doctrine, "rome");
  for (auto const unit : units) {
    EXPECT_EQ(registry.group_of(unit), result.group_id);
  }
}

TEST_F(ArmyFormationRegistryTest, MembershipComponentsMirrorTheGroupRecord) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 4; ++i) {
    units.push_back(
        add_unit(world, Game::Units::SpawnType::Knight, static_cast<float>(i)));
  }

  auto const result = commit(world, units);
  ASSERT_TRUE(result.valid);

  for (auto const id : units) {
    auto* entity = world.get_entity(id);
    ASSERT_NE(entity, nullptr);
    const auto* membership =
        entity->get_component<Engine::Core::ArmyFormationMembershipComponent>();
    ASSERT_NE(membership, nullptr) << "entity " << id;
    EXPECT_EQ(membership->group_id, result.group_id);
    EXPECT_GE(membership->slot_id, 0);
  }
}

TEST_F(ArmyFormationRegistryTest, RemovingAMemberFreesItsSlotAndSchedulesAReplan) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 5; ++i) {
    units.push_back(
        add_unit(world, Game::Units::SpawnType::Knight, static_cast<float>(i)));
  }
  auto const result = commit(world, units);
  ASSERT_TRUE(result.valid);

  auto& registry = ArmyFormationRegistry::instance();
  ASSERT_TRUE(registry.remove_member(units[2]));

  const auto* formation = registry.find(result.group_id);
  ASSERT_NE(formation, nullptr);
  EXPECT_EQ(formation->members.size(), units.size() - 1U);
  EXPECT_TRUE(formation->needs_replan);
  EXPECT_EQ(registry.group_of(units[2]), k_invalid_group);
  EXPECT_EQ(formation->find_slot_for(units[2]), nullptr);
}

TEST_F(ArmyFormationRegistryTest, EmptyingAGroupRemovesIt) {

  Engine::Core::World world;
  auto const first = add_unit(world, Game::Units::SpawnType::Knight, 0.0F);
  auto const second = add_unit(world, Game::Units::SpawnType::Knight, 1.0F);
  auto const result = commit(world, {first, second});
  ASSERT_TRUE(result.valid);

  auto& registry = ArmyFormationRegistry::instance();
  ASSERT_TRUE(registry.remove_member(first));
  ASSERT_NE(registry.find(result.group_id), nullptr);

  ASSERT_TRUE(registry.remove_member(second));
  EXPECT_EQ(registry.find(result.group_id), nullptr);
  EXPECT_EQ(registry.group_count(), 0U);
}

TEST_F(ArmyFormationRegistryTest, ASingleUnitOrderCommitsNoGroup) {

  Engine::Core::World world;
  auto const only = add_unit(world, Game::Units::SpawnType::Knight, 0.0F);
  auto const result = commit(world, {only});
  EXPECT_TRUE(result.valid);

  auto& registry = ArmyFormationRegistry::instance();
  EXPECT_EQ(registry.group_count(), 0U);
  EXPECT_EQ(registry.group_of(only), k_invalid_group);
  EXPECT_FALSE(registry.remove_member(only));
}

TEST_F(ArmyFormationRegistryTest, RuntimePrunesDestroyedMembers) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 4; ++i) {
    units.push_back(
        add_unit(world, Game::Units::SpawnType::Knight, static_cast<float>(i)));
  }
  auto const result = commit(world, units);
  ASSERT_TRUE(result.valid);

  auto* casualty = world.get_entity(units.back());
  ASSERT_NE(casualty, nullptr);
  casualty->get_component<Engine::Core::UnitComponent>()->health = 0;

  ArmyFormationRuntime runtime;
  runtime.update(&world, 0.1F);

  const auto* formation = ArmyFormationRegistry::instance().find(result.group_id);
  ASSERT_NE(formation, nullptr);
  EXPECT_EQ(formation->members.size(), units.size() - 1U);
  EXPECT_FALSE(formation->has_member(units.back()));
}

TEST_F(ArmyFormationRegistryTest, DetachClearsBothRegistryAndComponent) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 3; ++i) {
    units.push_back(
        add_unit(world, Game::Units::SpawnType::Knight, static_cast<float>(i)));
  }
  auto const result = commit(world, units);
  ASSERT_TRUE(result.valid);

  ArmyFormationRuntime::detach(world, units.front());

  EXPECT_EQ(ArmyFormationRegistry::instance().group_of(units.front()), k_invalid_group);
  const auto* membership =
      world.get_entity(units.front())
          ->get_component<Engine::Core::ArmyFormationMembershipComponent>();
  ASSERT_NE(membership, nullptr);
  EXPECT_FALSE(membership->is_valid());
}

TEST_F(ArmyFormationRegistryTest, GroupStateSurvivesASaveLoadRoundTrip) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 6; ++i) {
    units.push_back(
        add_unit(world, Game::Units::SpawnType::Knight, static_cast<float>(i) - 3.0F));
  }
  auto const result = commit(world, units, ArmyFormationIntent::Defensive);
  ASSERT_TRUE(result.valid);

  auto& registry = ArmyFormationRegistry::instance();
  const auto* before = registry.find(result.group_id);
  ASSERT_NE(before, nullptr);
  auto const expected_members = before->members;
  auto const expected_slots = before->slot_list.size();
  auto const expected_doctrine = before->doctrine;
  auto const expected_intent = before->intent;
  auto const expected_anchor = before->anchor;

  auto const json = registry.to_json();
  registry.clear();
  ASSERT_EQ(registry.group_count(), 0U);

  registry.from_json(json);
  const auto* after = registry.find(result.group_id);
  ASSERT_NE(after, nullptr);
  EXPECT_EQ(after->members, expected_members);
  EXPECT_EQ(after->slot_list.size(), expected_slots);
  EXPECT_EQ(after->doctrine, expected_doctrine);
  EXPECT_EQ(after->intent, expected_intent);
  EXPECT_FLOAT_EQ(after->anchor.x(), expected_anchor.x());
  EXPECT_FLOAT_EQ(after->anchor.z(), expected_anchor.z());
  for (auto const unit : expected_members) {
    EXPECT_EQ(registry.group_of(unit), result.group_id);
  }
}

TEST_F(ArmyFormationRegistryTest, ReplanningKeepsUnitsInTheirExistingSlots) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> units;
  for (int i = 0; i < 8; ++i) {
    units.push_back(
        add_unit(world, Game::Units::SpawnType::Knight, static_cast<float>(i) - 4.0F));
  }

  auto const first = commit(world, units);
  ASSERT_TRUE(first.valid);

  ArmyFormationRequest request;
  request.members = units;
  request.anchor = QVector3D(0.0F, 0.0F, 12.0F);
  request.intent = ArmyFormationIntent::Line;
  request.spacing = 1.5F;
  request.group_id = first.group_id;
  request.preserve_previous_slots = true;
  auto const second = ArmyFormationService::commit(world, request);
  ASSERT_TRUE(second.valid);

  ASSERT_EQ(first.stable_slot_ids.size(), second.stable_slot_ids.size());
  for (std::size_t i = 0; i < first.stable_slot_ids.size(); ++i) {
    EXPECT_EQ(first.stable_slot_ids[i], second.stable_slot_ids[i])
        << "unit index " << i;
  }
}

TEST_F(ArmyFormationRegistryTest, CommittingASecondSelectionMovesUnitsBetweenGroups) {
  Engine::Core::World world;
  std::vector<Engine::Core::EntityID> first_units;
  for (int i = 0; i < 4; ++i) {
    first_units.push_back(
        add_unit(world, Game::Units::SpawnType::Knight, static_cast<float>(i)));
  }
  auto const first = commit(world, first_units);
  ASSERT_TRUE(first.valid);

  std::vector<Engine::Core::EntityID> second_units = {first_units[0], first_units[1]};
  for (int i = 0; i < 2; ++i) {
    second_units.push_back(
        add_unit(world, Game::Units::SpawnType::Archer, static_cast<float>(i) + 10.0F));
  }

  ArmyFormationRequest request;
  request.members = second_units;
  request.anchor = QVector3D(20.0F, 0.0F, 20.0F);
  request.intent = ArmyFormationIntent::Line;
  request.spacing = 1.5F;
  request.group_id = k_invalid_group;
  request.preserve_previous_slots = false;
  auto const second = ArmyFormationService::commit(world, request);
  ASSERT_TRUE(second.valid);

  auto& registry = ArmyFormationRegistry::instance();
  for (auto const unit : second_units) {
    EXPECT_EQ(registry.group_of(unit), second.group_id);
  }
}
