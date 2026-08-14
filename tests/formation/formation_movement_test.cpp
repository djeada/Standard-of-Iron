#include <QVector3D>

#include <cmath>
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
using Game::Formation::MovementPolicy;
using Game::Systems::NationID;

auto add_unit(Engine::Core::World& world, float x, float z) -> Engine::Core::EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  transform->position = {x, 0.0F, z};
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->nation_id = NationID::RomanRepublic;
  unit->health = 100;
  unit->max_health = 100;
  unit->speed = 2.0F;
  return entity->get_id();
}

auto commit(Engine::Core::World& world,
            const std::vector<Engine::Core::EntityID>& units,
            const QVector3D& anchor,
            MovementPolicy policy) {
  ArmyFormationRequest request;
  request.members = units;
  request.anchor = anchor;
  request.intent = ArmyFormationIntent::Line;
  request.spacing = 1.5F;
  request.options.movement_policy = policy;
  return ArmyFormationService::commit(world, request);
}

class FormationMovementTest : public ::testing::Test {
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
    ArmyFormationRegistry::instance().clear();
  }

  void TearDown() override {
    ArmyFormationRegistry::instance().clear();
    Game::Systems::TroopProfileService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  static auto
  make_squad(Engine::Core::World& world) -> std::vector<Engine::Core::EntityID> {
    std::vector<Engine::Core::EntityID> units;
    for (int i = 0; i < 6; ++i) {
      units.push_back(add_unit(world, static_cast<float>(i) - 3.0F, 0.0F));
    }
    return units;
  }
};

} // namespace

TEST_F(FormationMovementTest, ReformAtDestinationAnchorsStraightAtTheTarget) {
  Engine::Core::World world;
  auto const units = make_squad(world);
  QVector3D const target(0.0F, 0.0F, 60.0F);

  auto const result = commit(world, units, target, MovementPolicy::ReformAtDestination);
  ASSERT_TRUE(result.valid) << result.rejection_reason;

  const auto* formation = ArmyFormationRegistry::instance().find(result.group_id);
  ASSERT_NE(formation, nullptr);
  EXPECT_FALSE(formation->maintains_formation());
  EXPECT_FALSE(formation->has_destination);
  EXPECT_NEAR(formation->anchor.z(), target.z(), 2.0F);
}

TEST_F(FormationMovementTest, MaintainFormationStartsFromTheCurrentCentroid) {
  Engine::Core::World world;
  auto const units = make_squad(world);
  QVector3D const target(0.0F, 0.0F, 60.0F);

  auto const result = commit(world, units, target, MovementPolicy::MaintainFormation);
  ASSERT_TRUE(result.valid) << result.rejection_reason;

  const auto* formation = ArmyFormationRegistry::instance().find(result.group_id);
  ASSERT_NE(formation, nullptr);
  EXPECT_TRUE(formation->maintains_formation());
  EXPECT_TRUE(formation->has_destination);
  EXPECT_NEAR(formation->destination.z(), target.z(), 2.0F);
  EXPECT_LT(formation->anchor.z(), target.z() * 0.5F);
}

TEST_F(FormationMovementTest, MaintainFormationAdvancesTheAnchorInStages) {
  Engine::Core::World world;
  auto const units = make_squad(world);
  QVector3D const target(0.0F, 0.0F, 60.0F);

  auto const result = commit(world, units, target, MovementPolicy::MaintainFormation);
  ASSERT_TRUE(result.valid);

  auto& registry = ArmyFormationRegistry::instance();
  float const first_anchor = registry.find(result.group_id)->anchor.z();

  ArmyFormationRuntime runtime;
  runtime.update(&world, 0.3F);

  float const second_anchor = registry.find(result.group_id)->anchor.z();
  EXPECT_GT(second_anchor, first_anchor);
  EXPECT_LT(second_anchor, target.z());
}

TEST_F(FormationMovementTest, MaintainFormationDoesNotOvershootTheDestination) {
  Engine::Core::World world;
  auto const units = make_squad(world);
  QVector3D const target(0.0F, 0.0F, 12.0F);

  auto const result = commit(world, units, target, MovementPolicy::MaintainFormation);
  ASSERT_TRUE(result.valid);

  auto& registry = ArmyFormationRegistry::instance();
  ArmyFormationRuntime runtime;
  for (int tick = 0; tick < 60; ++tick) {
    const auto* formation = registry.find(result.group_id);
    if (formation == nullptr) {
      break;
    }
    for (auto const member : formation->members) {
      auto* entity = world.get_entity(member);
      const auto* slot = formation->find_slot_for(member);
      if (entity == nullptr || slot == nullptr) {
        continue;
      }
      auto* transform = entity->get_component<Engine::Core::TransformComponent>();
      transform->position.x = slot->world_position.x();
      transform->position.z = slot->world_position.z();
    }
    runtime.update(&world, 0.3F);
  }

  const auto* formation = registry.find(result.group_id);
  ASSERT_NE(formation, nullptr);
  EXPECT_LE(formation->anchor.z(), target.z() + 1.0F);
  EXPECT_FALSE(formation->has_destination);
}

TEST_F(FormationMovementTest, MaintainFormationSlowsItsMembersDown) {
  Engine::Core::World world;
  auto const units = make_squad(world);

  auto const result = commit(
      world, units, QVector3D(0.0F, 0.0F, 40.0F), MovementPolicy::MaintainFormation);
  ASSERT_TRUE(result.valid);

  auto* entity = world.get_entity(units.front());
  ASSERT_NE(entity, nullptr);
  EXPECT_LT(ArmyFormationRuntime::move_speed_multiplier(*entity), 1.0F);
}

TEST_F(FormationMovementTest, ReformAtDestinationRunsAtFullSpeed) {
  Engine::Core::World world;
  auto const units = make_squad(world);

  auto const result = commit(
      world, units, QVector3D(0.0F, 0.0F, 40.0F), MovementPolicy::ReformAtDestination);
  ASSERT_TRUE(result.valid);

  auto* entity = world.get_entity(units.front());
  ASSERT_NE(entity, nullptr);
  EXPECT_FLOAT_EQ(ArmyFormationRuntime::move_speed_multiplier(*entity), 1.0F);
}

TEST_F(FormationMovementTest, UnitsOutsideAnyGroupAreUnaffected) {
  Engine::Core::World world;
  auto const loner = add_unit(world, 5.0F, 5.0F);
  auto* entity = world.get_entity(loner);
  ASSERT_NE(entity, nullptr);
  EXPECT_FLOAT_EQ(ArmyFormationRuntime::move_speed_multiplier(*entity), 1.0F);
}

TEST_F(FormationMovementTest, TerrainFittingKeepsSlotsOnWalkableGround) {
  Game::Systems::NavGrid::initialize(64, 64);
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  Engine::Core::World world;
  auto const units = make_squad(world);

  QVector3D const target(24.0F, 0.0F, 24.0F);
  auto const centre = Game::Systems::NavGrid::world_to_grid(target.x(), target.z());
  for (int dx = -1; dx <= 1; ++dx) {
    pathfinder->set_obstacle(centre.x + dx, centre.y, true);
  }

  auto const result = commit(world, units, target, MovementPolicy::ReformAtDestination);
  ASSERT_TRUE(result.valid) << result.rejection_reason;

  int placed = 0;
  for (std::size_t i = 0; i < result.positions.size(); ++i) {
    if (result.slot_status[i] == Game::Formation::SlotStatus::Blocked) {
      continue;
    }
    ++placed;
    EXPECT_TRUE(Game::Systems::NavGrid::is_world_position_walkable(result.positions[i]))
        << "unit index " << i;
  }
  EXPECT_GT(placed, 0);
}

TEST_F(FormationMovementTest, NarrowGroundCompressesRatherThanStackingUnits) {
  Game::Systems::NavGrid::initialize(64, 64);
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  Engine::Core::World world;
  auto const units = make_squad(world);

  QVector3D const target(32.0F, 0.0F, 32.0F);
  auto const centre = Game::Systems::NavGrid::world_to_grid(target.x(), target.z());
  for (int dz = -6; dz <= 6; ++dz) {
    pathfinder->set_obstacle(centre.x - 3, centre.y + dz, true);
    pathfinder->set_obstacle(centre.x + 3, centre.y + dz, true);
  }

  auto const result = commit(world, units, target, MovementPolicy::ReformAtDestination);

  std::vector<QVector3D> placed;
  for (std::size_t i = 0; i < result.positions.size(); ++i) {
    if (result.slot_status[i] == Game::Formation::SlotStatus::Blocked) {
      continue;
    }
    for (const auto& other : placed) {
      float const dx = other.x() - result.positions[i].x();
      float const dz = other.z() - result.positions[i].z();
      EXPECT_GT(std::sqrt(dx * dx + dz * dz), 0.4F)
          << "units " << i << " share a position";
    }
    placed.push_back(result.positions[i]);
  }
}
