#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <unordered_set>
#include <vector>

#include "core/component.h"
#include "core/world.h"
#include "systems/formation_planner.h"
#include "systems/formation_system.h"
#include "systems/nation_registry.h"

using namespace Game::Systems;

namespace {

auto position_for(const std::vector<FormationPosition>& positions,
                  Engine::Core::EntityID entity_id) -> QVector3D {
  for (const auto& position : positions) {
    if (position.entity_id == entity_id) {
      return position.position;
    }
  }
  return {};
}

auto bounds_center(const std::vector<FormationPosition>& positions) -> QVector3D {
  if (positions.empty()) {
    return {};
  }

  float min_x = positions.front().position.x();
  float max_x = min_x;
  float min_z = positions.front().position.z();
  float max_z = min_z;
  for (const auto& pos : positions) {
    min_x = std::min(min_x, pos.position.x());
    max_x = std::max(max_x, pos.position.x());
    min_z = std::min(min_z, pos.position.z());
    max_z = std::max(max_z, pos.position.z());
  }
  return {(min_x + max_x) * 0.5F, 0.0F, (min_z + max_z) * 0.5F};
}

auto formation_right_vector(float formation_facing_degrees) -> QVector3D {
  constexpr float k_pi = std::numbers::pi_v<float>;
  float const yaw_radians = formation_facing_degrees * (k_pi / 180.0F);
  return {std::cos(yaw_radians), 0.0F, -std::sin(yaw_radians)};
}

auto lateral_projection(const QVector3D& position,
                        const QVector3D& right_vector) -> float {
  return QVector3D::dotProduct(position, right_vector);
}

auto span_x(const std::vector<FormationPosition>& positions) -> float {
  if (positions.empty()) {
    return 0.0F;
  }

  float min_x = positions.front().position.x();
  float max_x = min_x;
  for (const auto& pos : positions) {
    min_x = std::min(min_x, pos.position.x());
    max_x = std::max(max_x, pos.position.x());
  }
  return max_x - min_x;
}

auto add_unit(Engine::Core::World& world,
              Game::Units::SpawnType spawn_type,
              NationID nation_id,
              bool formation_active,
              float x) -> Engine::Core::EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  auto* formation = entity->add_component<Engine::Core::FormationModeComponent>();
  transform->position = {x, 0.0F, 0.0F};
  unit->spawn_type = spawn_type;
  unit->nation_id = nation_id;
  formation->active = formation_active;
  return entity->get_id();
}

} // namespace

class FormationSystemTest : public ::testing::Test {
protected:
  void SetUp() override {}
};

TEST_F(FormationSystemTest, RomanFormationCreatesRectangularGrid) {
  QVector3D const center(0.0F, 0.0F, 0.0F);
  float const spacing = 2.0F;
  int const unit_count = 9;

  auto positions = FormationSystem::instance().get_formation_positions(
      FormationType::Roman, unit_count, center, spacing);

  EXPECT_EQ(positions.size(), 9);

  for (const auto& pos : positions) {
    EXPECT_FLOAT_EQ(pos.y(), center.y());
  }
}

TEST_F(FormationSystemTest, CarthageFormationHasJitter) {
  QVector3D const center(0.0F, 0.0F, 0.0F);
  float const spacing = 2.0F;
  int const unit_count = 9;

  auto positions1 = FormationSystem::instance().get_formation_positions(
      FormationType::Carthage, unit_count, center, spacing);

  EXPECT_EQ(positions1.size(), 9);

  for (const auto& pos : positions1) {
    EXPECT_LT(std::abs(pos.x() - center.x()), spacing * 5.0F);
    EXPECT_LT(std::abs(pos.z() - center.z()), spacing * 5.0F);
    EXPECT_FLOAT_EQ(pos.y(), center.y());
  }
}

TEST_F(FormationSystemTest, BarbarianFormationIsLooser) {
  QVector3D const center(0.0F, 0.0F, 0.0F);
  float const spacing = 2.0F;
  int const unit_count = 9;

  auto barbarian_positions = FormationSystem::instance().get_formation_positions(
      FormationType::Barbarian, unit_count, center, spacing);
  auto roman_positions = FormationSystem::instance().get_formation_positions(
      FormationType::Roman, unit_count, center, spacing);

  EXPECT_EQ(barbarian_positions.size(), 9);
  EXPECT_EQ(roman_positions.size(), 9);

  float barbarian_avg_dist = 0.0F;
  float roman_avg_dist = 0.0F;

  for (size_t i = 0; i < 9; ++i) {
    QVector3D const barbarian_vec = barbarian_positions[i] - center;
    QVector3D const roman_vec = roman_positions[i] - center;
    barbarian_avg_dist += barbarian_vec.length();
    roman_avg_dist += roman_vec.length();
  }

  barbarian_avg_dist /= 9.0F;
  roman_avg_dist /= 9.0F;

  EXPECT_GT(barbarian_avg_dist, roman_avg_dist);
}

TEST_F(FormationSystemTest, HandlesZeroUnits) {
  QVector3D const center(0.0F, 0.0F, 0.0F);

  auto positions = FormationSystem::instance().get_formation_positions(
      FormationType::Roman, 0, center, 1.0F);

  EXPECT_TRUE(positions.empty());
}

TEST_F(FormationSystemTest, HandlesSingleUnit) {
  QVector3D const center(5.0F, 0.0F, 10.0F);

  auto positions = FormationSystem::instance().get_formation_positions(
      FormationType::Roman, 1, center, 1.0F);

  EXPECT_EQ(positions.size(), 1);
  EXPECT_FLOAT_EQ(positions[0].x(), center.x());
  EXPECT_FLOAT_EQ(positions[0].y(), center.y());
  EXPECT_FLOAT_EQ(positions[0].z(), center.z());
}

TEST_F(FormationSystemTest, FormationsScaleWithUnitCount) {
  QVector3D const center(0.0F, 0.0F, 0.0F);
  float const spacing = 2.0F;

  auto small_formation = FormationSystem::instance().get_formation_positions(
      FormationType::Roman, 4, center, spacing);
  auto large_formation = FormationSystem::instance().get_formation_positions(
      FormationType::Roman, 16, center, spacing);

  EXPECT_EQ(small_formation.size(), 4);
  EXPECT_EQ(large_formation.size(), 16);

  float small_max_dist = 0.0F;
  float large_max_dist = 0.0F;

  for (const auto& pos : small_formation) {
    QVector3D const vec = pos - center;
    small_max_dist = std::max(small_max_dist, vec.length());
  }

  for (const auto& pos : large_formation) {
    QVector3D const vec = pos - center;
    large_max_dist = std::max(large_max_dist, vec.length());
  }

  EXPECT_GT(large_max_dist, small_max_dist);
}

TEST_F(FormationSystemTest, CentralCavalryOffsetsUseStaggerAndRankYaw) {
  float const spacing = 1.5F;

  auto const rear =
      FormationSystem::instance().get_local_offset(FormationType::Roman,
                                                   FormationUnitCategory::Cavalry,
                                                   0,
                                                   0,
                                                   0,
                                                   3,
                                                   3,
                                                   spacing,
                                                   0x12345678U);
  auto const middle =
      FormationSystem::instance().get_local_offset(FormationType::Roman,
                                                   FormationUnitCategory::Cavalry,
                                                   3,
                                                   1,
                                                   0,
                                                   3,
                                                   3,
                                                   spacing,
                                                   0x12345678U);
  auto const front =
      FormationSystem::instance().get_local_offset(FormationType::Roman,
                                                   FormationUnitCategory::Cavalry,
                                                   6,
                                                   2,
                                                   0,
                                                   3,
                                                   3,
                                                   spacing,
                                                   0x12345678U);

  EXPECT_GT(middle.offset_x, rear.offset_x);
  EXPECT_GT(front.offset_z, middle.offset_z);
  EXPECT_GT(middle.offset_z, rear.offset_z);
  EXPECT_FLOAT_EQ(front.yaw_offset, 0.0F);
  EXPECT_FLOAT_EQ(middle.yaw_offset, -5.0F);
  EXPECT_FLOAT_EQ(rear.yaw_offset, 5.0F);
}

TEST_F(FormationSystemTest, RomanInfantryLocalOffsetsUseWiderSpacing) {
  float const spacing = 1.0F;

  auto const left =
      FormationSystem::instance().get_local_offset(FormationType::Roman,
                                                   FormationUnitCategory::Infantry,
                                                   0,
                                                   0,
                                                   0,
                                                   2,
                                                   2,
                                                   spacing,
                                                   0x12345678U);
  auto const right =
      FormationSystem::instance().get_local_offset(FormationType::Roman,
                                                   FormationUnitCategory::Infantry,
                                                   1,
                                                   0,
                                                   1,
                                                   2,
                                                   2,
                                                   spacing,
                                                   0x12345678U);
  auto const rear_left =
      FormationSystem::instance().get_local_offset(FormationType::Roman,
                                                   FormationUnitCategory::Infantry,
                                                   2,
                                                   1,
                                                   0,
                                                   2,
                                                   2,
                                                   spacing,
                                                   0x12345678U);

  EXPECT_GT(right.offset_x - left.offset_x, 1.05F);
  EXPECT_GT(rear_left.offset_z - left.offset_z, 1.05F);
}

TEST_F(FormationSystemTest, TacticalFormationsPlaceEveryTroopType) {
  const std::vector<Game::Units::TroopType> troop_types = {
      Game::Units::TroopType::Archer,
      Game::Units::TroopType::Swordsman,
      Game::Units::TroopType::Spearman,
      Game::Units::TroopType::MountedKnight,
      Game::Units::TroopType::HorseArcher,
      Game::Units::TroopType::HorseSpearman,
      Game::Units::TroopType::Healer,
      Game::Units::TroopType::Catapult,
      Game::Units::TroopType::Ballista,
      Game::Units::TroopType::Elephant,
      Game::Units::TroopType::RomanLegionOrganizer,
      Game::Units::TroopType::RomanVeteranConsul,
      Game::Units::TroopType::RomanFieldCommander,
      Game::Units::TroopType::CarthageMercenaryBroker,
      Game::Units::TroopType::CarthageCavalryPatron,
      Game::Units::TroopType::CarthageElephantMaster,
      Game::Units::TroopType::Civilian,
      Game::Units::TroopType::Builder};

  std::vector<UnitFormationInfo> units;
  units.reserve(troop_types.size());
  for (size_t i = 0; i < troop_types.size(); ++i) {
    units.push_back({static_cast<Engine::Core::EntityID>(i + 1),
                     troop_types[i],
                     QVector3D(static_cast<float>(i), 0.0F, 0.0F)});
  }

  for (FormationType const formation_type :
       {FormationType::Roman, FormationType::Carthage}) {
    auto positions = FormationSystem::instance().get_formation_positions_with_facing(
        formation_type, units, QVector3D(0.0F, 0.0F, 0.0F), 1.0F);

    EXPECT_EQ(positions.size(), units.size());

    std::unordered_set<Engine::Core::EntityID> placed_ids;
    for (const auto& position : positions) {
      placed_ids.insert(position.entity_id);
    }
    for (const auto& unit : units) {
      EXPECT_TRUE(placed_ids.contains(unit.entity_id));
    }
  }
}

TEST_F(FormationSystemTest, RangedUnitsStayBehindFrontlineInTacticalFormations) {
  std::vector<UnitFormationInfo> const units = {
      {1, Game::Units::TroopType::Swordsman, QVector3D(-1.0F, 0.0F, 0.0F)},
      {2, Game::Units::TroopType::Archer, QVector3D(1.0F, 0.0F, 0.0F)}};

  for (FormationType const formation_type :
       {FormationType::Roman, FormationType::Carthage}) {
    auto positions = FormationSystem::instance().get_formation_positions_with_facing(
        formation_type, units, QVector3D(0.0F, 0.0F, 0.0F), 1.0F);

    EXPECT_LT(position_for(positions, 2).z(), position_for(positions, 1).z());
  }
}

TEST_F(FormationSystemTest, RomanFormationKeepsSpearmenAheadOfSwordsmenAndArchers) {
  std::vector<UnitFormationInfo> const units = {
      {1, Game::Units::TroopType::Spearman, QVector3D(-2.0F, 0.0F, 0.0F)},
      {2, Game::Units::TroopType::Swordsman, QVector3D(0.0F, 0.0F, 0.0F)},
      {3, Game::Units::TroopType::Archer, QVector3D(2.0F, 0.0F, 0.0F)}};

  auto positions = FormationSystem::instance().get_formation_positions_with_facing(
      FormationType::Roman, units, QVector3D(0.0F, 0.0F, 0.0F), 1.0F);

  ASSERT_EQ(positions.size(), units.size());
  EXPECT_GT(position_for(positions, 1).z(), position_for(positions, 2).z());
  EXPECT_GT(position_for(positions, 2).z(), position_for(positions, 3).z());
}

TEST_F(FormationSystemTest,
       RomanFormationKeepsSpearmenAheadWhenSwordsmenAreOnlySupport) {
  std::vector<UnitFormationInfo> const units = {
      {1, Game::Units::TroopType::Spearman, QVector3D(-1.0F, 0.0F, 0.0F)},
      {2, Game::Units::TroopType::Spearman, QVector3D(0.0F, 0.0F, 0.0F)},
      {3, Game::Units::TroopType::Swordsman, QVector3D(1.0F, 0.0F, 0.0F)}};

  auto positions = FormationSystem::instance().get_formation_positions_with_facing(
      FormationType::Roman, units, QVector3D(4.0F, 0.0F, -3.0F), 1.0F);

  ASSERT_EQ(positions.size(), units.size());
  EXPECT_GT(position_for(positions, 1).z(), position_for(positions, 3).z());
  EXPECT_GT(position_for(positions, 2).z(), position_for(positions, 3).z());
}

TEST_F(FormationSystemTest, SingleLineSubsetsStayCenteredOnOrderedPoint) {
  std::vector<UnitFormationInfo> const units = {
      {1, Game::Units::TroopType::Spearman, QVector3D(-2.0F, 0.0F, 0.0F)},
      {2, Game::Units::TroopType::Spearman, QVector3D(0.0F, 0.0F, 0.0F)},
      {3, Game::Units::TroopType::Spearman, QVector3D(2.0F, 0.0F, 0.0F)}};

  QVector3D const target(11.0F, 0.0F, -7.0F);
  auto positions = FormationSystem::instance().get_formation_positions_with_facing(
      FormationType::Roman, units, target, 1.0F);

  ASSERT_EQ(positions.size(), units.size());
  auto const center = bounds_center(positions);
  EXPECT_NEAR(center.x(), target.x(), 1.0e-4F);
  EXPECT_NEAR(center.z(), target.z(), 1.0e-4F);
}

TEST_F(FormationSystemTest, CarthageFormationPlacesElephantsAheadOfSpearsAndArchers) {
  std::vector<UnitFormationInfo> const units = {
      {1, Game::Units::TroopType::Elephant, QVector3D(-2.0F, 0.0F, 0.0F)},
      {2, Game::Units::TroopType::Spearman, QVector3D(0.0F, 0.0F, 0.0F)},
      {3, Game::Units::TroopType::Archer, QVector3D(2.0F, 0.0F, 0.0F)}};

  auto positions = FormationSystem::instance().get_formation_positions_with_facing(
      FormationType::Carthage, units, QVector3D(0.0F, 0.0F, 0.0F), 1.0F);

  ASSERT_EQ(positions.size(), units.size());
  EXPECT_GT(position_for(positions, 1).z(), position_for(positions, 2).z());
  EXPECT_GT(position_for(positions, 2).z(), position_for(positions, 3).z());
}

TEST_F(FormationSystemTest, RomanAndCarthageFormationsStayDistinctForSameArmy) {
  std::vector<UnitFormationInfo> const units = {
      {1, Game::Units::TroopType::Spearman, QVector3D(-2.0F, 0.0F, 0.0F)},
      {2, Game::Units::TroopType::Swordsman, QVector3D(-1.0F, 0.0F, 0.0F)},
      {3, Game::Units::TroopType::Archer, QVector3D(1.0F, 0.0F, 0.0F)},
      {4, Game::Units::TroopType::MountedKnight, QVector3D(3.0F, 0.0F, 0.0F)}};

  auto roman = FormationSystem::instance().get_formation_positions_with_facing(
      FormationType::Roman, units, QVector3D(0.0F, 0.0F, 0.0F), 1.0F);
  auto carthage = FormationSystem::instance().get_formation_positions_with_facing(
      FormationType::Carthage, units, QVector3D(0.0F, 0.0F, 0.0F), 1.0F);

  ASSERT_EQ(roman.size(), units.size());
  ASSERT_EQ(carthage.size(), units.size());
  EXPECT_NE(position_for(roman, 4), position_for(carthage, 4));
  EXPECT_NE(position_for(roman, 3), position_for(carthage, 3));
}

TEST_F(FormationSystemTest, CavalryOnlySelectionsStayCompactWithoutCenterBody) {
  std::vector<UnitFormationInfo> const units = {
      {1, Game::Units::TroopType::MountedKnight, QVector3D(-3.0F, 0.0F, 0.0F)},
      {2, Game::Units::TroopType::MountedKnight, QVector3D(-1.0F, 0.0F, 0.0F)},
      {3, Game::Units::TroopType::MountedKnight, QVector3D(1.0F, 0.0F, 0.0F)},
      {4, Game::Units::TroopType::MountedKnight, QVector3D(3.0F, 0.0F, 0.0F)}};

  QVector3D const target(6.0F, 0.0F, -4.0F);
  for (FormationType const formation_type :
       {FormationType::Roman, FormationType::Carthage}) {
    auto positions = FormationSystem::instance().get_formation_positions_with_facing(
        formation_type, units, target, 1.0F);

    ASSERT_EQ(positions.size(), units.size());
    auto const center = bounds_center(positions);
    EXPECT_NEAR(center.x(), target.x(), 1.0e-4F);
    EXPECT_NEAR(center.z(), target.z(), 1.0e-4F);
    EXPECT_LT(span_x(positions), 12.0F);
  }
}

TEST_F(FormationSystemTest, PlannerSeparatesRomanAndCarthageFormationGroups) {
  NationRegistry::instance().initialize_defaults();
  Engine::Core::World world;

  std::vector<Engine::Core::EntityID> units;
  auto const roman_swordsman = add_unit(
      world, Game::Units::SpawnType::Knight, NationID::RomanRepublic, true, -3.0F);
  auto const roman_archer = add_unit(
      world, Game::Units::SpawnType::Archer, NationID::RomanRepublic, true, -2.0F);
  auto const carthage_swordsman =
      add_unit(world, Game::Units::SpawnType::Knight, NationID::Carthage, true, 2.0F);
  auto const carthage_archer =
      add_unit(world, Game::Units::SpawnType::Archer, NationID::Carthage, true, 3.0F);
  units = {roman_swordsman, roman_archer, carthage_swordsman, carthage_archer};

  auto result = FormationPlanner::get_formation_with_facing(
      world, units, QVector3D(10.0F, 0.0F, 20.0F), 1.0F);

  ASSERT_TRUE(result.used_tactical_formation);
  ASSERT_EQ(result.positions.size(), units.size());

  auto const right = formation_right_vector(result.formation_facing);
  auto const roman_center_x = (lateral_projection(result.positions[0], right) +
                               lateral_projection(result.positions[1], right)) *
                              0.5F;
  auto const carthage_center_x = (lateral_projection(result.positions[2], right) +
                                  lateral_projection(result.positions[3], right)) *
                                 0.5F;
  EXPECT_LT(roman_center_x, carthage_center_x);
}

TEST_F(FormationSystemTest, PlannerUsesActualGroupWidthsForMixedFactionCavalry) {
  NationRegistry::instance().initialize_defaults();
  Engine::Core::World world;

  std::vector<Engine::Core::EntityID> units;
  std::vector<size_t> roman_indices;
  std::vector<size_t> carthage_indices;

  for (int i = 0; i < 6; ++i) {
    auto const carthage_id = add_unit(world,
                                      Game::Units::SpawnType::MountedKnight,
                                      NationID::Carthage,
                                      true,
                                      6.0F + static_cast<float>(i));
    carthage_indices.push_back(units.size());
    units.push_back(carthage_id);

    auto const roman_id = add_unit(world,
                                   Game::Units::SpawnType::MountedKnight,
                                   NationID::RomanRepublic,
                                   true,
                                   -12.0F + static_cast<float>(i));
    roman_indices.push_back(units.size());
    units.push_back(roman_id);
  }

  auto result = FormationPlanner::get_formation_with_facing(
      world, units, QVector3D(0.0F, 0.0F, 0.0F), 1.0F);

  ASSERT_TRUE(result.used_tactical_formation);
  ASSERT_EQ(result.positions.size(), units.size());

  auto const right = formation_right_vector(result.formation_facing);
  float roman_max_x =
      lateral_projection(result.positions[roman_indices.front()], right);
  for (size_t const index : roman_indices) {
    roman_max_x =
        std::max(roman_max_x, lateral_projection(result.positions[index], right));
  }

  float carthage_min_x =
      lateral_projection(result.positions[carthage_indices.front()], right);
  for (size_t const index : carthage_indices) {
    carthage_min_x =
        std::min(carthage_min_x, lateral_projection(result.positions[index], right));
  }

  EXPECT_LT(roman_max_x, carthage_min_x);
}

TEST_F(FormationSystemTest, PlannerRotatesFrontlineTowardOrderDirection) {
  NationRegistry::instance().initialize_defaults();
  Engine::Core::World world;

  auto const spearman = add_unit(
      world, Game::Units::SpawnType::Spearman, NationID::RomanRepublic, true, -1.0F);
  auto const archer = add_unit(
      world, Game::Units::SpawnType::Archer, NationID::RomanRepublic, true, 1.0F);
  std::vector<Engine::Core::EntityID> const units = {spearman, archer};

  auto result = FormationPlanner::get_formation_with_facing(
      world, units, QVector3D(12.0F, 0.0F, 0.0F), 1.0F);

  ASSERT_TRUE(result.used_tactical_formation);
  ASSERT_EQ(result.positions.size(), units.size());
  EXPECT_GT(result.positions[0].x(), result.positions[1].x());
  EXPECT_NEAR(result.formation_facing, 90.0F, 5.0F);
  EXPECT_NEAR(result.facing_angles[0], result.formation_facing, 5.0F);
}

TEST_F(FormationSystemTest, PlannerCollapsesInvalidFormationSlotsToCenterTarget) {
  NationRegistry::instance().initialize_defaults();
  Game::Systems::CommandService::initialize(32, 32);
  Engine::Core::World world;

  auto const left = add_unit(
      world, Game::Units::SpawnType::Archer, NationID::RomanRepublic, true, -1.0F);
  auto const right = add_unit(
      world, Game::Units::SpawnType::Archer, NationID::RomanRepublic, true, 1.0F);
  std::vector<Engine::Core::EntityID> const units = {left, right};

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  QVector3D const target(0.0F, 0.0F, 10.0F);
  auto const baseline =
      FormationPlanner::get_formation_with_facing(world, units, target, 1.0F);
  ASSERT_TRUE(baseline.used_tactical_formation);
  ASSERT_EQ(baseline.positions.size(), units.size());

  Game::Systems::Point const blocked = Game::Systems::CommandService::world_to_grid(
      baseline.positions.front().x(), baseline.positions.front().z());
  Game::Systems::Point const center =
      Game::Systems::CommandService::world_to_grid(target.x(), target.z());
  ASSERT_FALSE(blocked == center);
  pathfinder->set_obstacle(blocked.x, blocked.y, true);

  auto result = FormationPlanner::get_formation_with_facing(world, units, target, 1.0F);

  ASSERT_TRUE(result.used_tactical_formation);
  ASSERT_EQ(result.positions.size(), units.size());

  auto const collapsed = Game::Systems::CommandService::world_to_grid(
      result.positions.front().x(), result.positions.front().z());
  EXPECT_EQ(collapsed, center);
  EXPECT_TRUE(pathfinder->is_walkable(collapsed.x, collapsed.y));
}

TEST_F(FormationSystemTest, PlannerCollapsesTightFormationAreaToCenterTarget) {
  NationRegistry::instance().initialize_defaults();
  Game::Systems::CommandService::initialize(32, 32);
  Engine::Core::World world;

  std::vector<Engine::Core::EntityID> const units = {
      add_unit(
          world, Game::Units::SpawnType::Archer, NationID::RomanRepublic, true, -3.0F),
      add_unit(
          world, Game::Units::SpawnType::Archer, NationID::RomanRepublic, true, -1.0F),
      add_unit(
          world, Game::Units::SpawnType::Archer, NationID::RomanRepublic, true, 1.0F),
      add_unit(
          world, Game::Units::SpawnType::Archer, NationID::RomanRepublic, true, 3.0F)};

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  QVector3D const target(10.0F, 0.0F, 10.0F);
  auto const center =
      Game::Systems::CommandService::world_to_grid(target.x(), target.z());
  for (int dz = -3; dz <= 3; ++dz) {
    for (int dx = -3; dx <= 3; ++dx) {
      if (dx == 0 && dz == 0) {
        continue;
      }
      pathfinder->set_obstacle(center.x + dx, center.y + dz, true);
    }
  }

  auto const result =
      FormationPlanner::get_formation_with_facing(world, units, target, 1.0F);
  ASSERT_TRUE(result.used_tactical_formation);
  ASSERT_EQ(result.positions.size(), units.size());

  for (auto const& position : result.positions) {
    auto const grid =
        Game::Systems::CommandService::world_to_grid(position.x(), position.z());
    EXPECT_EQ(grid, center);
    EXPECT_TRUE(pathfinder->is_walkable(grid.x, grid.y));
  }
}
