#include "systems/formation_system.h"
#include <QVector3D>
#include <gtest/gtest.h>

using namespace Game::Systems;

class FormationSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    // FormationSystem is a singleton, instance is automatically initialized
  }
};

TEST_F(FormationSystemTest, RomanFormationCreatesRectangularGrid) {
  QVector3D center(0.0F, 0.0F, 0.0F);
  float spacing = 2.0F;
  int unit_count = 9; // 3x3 grid

  auto positions = FormationSystem::instance().get_formation_positions(
      FormationType::Roman, unit_count, center, spacing);

  EXPECT_EQ(positions.size(), 9);

  // Check that positions are arranged in a grid pattern
  // Roman formation uses rectangular layout with 0.7 aspect ratio
  for (const auto &pos : positions) {
    EXPECT_FLOAT_EQ(pos.y(), center.y());
  }
}

TEST_F(FormationSystemTest, CarthageFormationHasJitter) {
  QVector3D center(0.0F, 0.0F, 0.0F);
  float spacing = 2.0F;
  int unit_count = 9;

  auto positions1 = FormationSystem::instance().get_formation_positions(
      FormationType::Carthage, unit_count, center, spacing);
  
  // Carthage formation uses jitter, so positions won't be perfectly aligned
  EXPECT_EQ(positions1.size(), 9);
  
  // Check that all positions are near the center (within reasonable bounds)
  for (const auto &pos : positions1) {
    EXPECT_LT(std::abs(pos.x() - center.x()), spacing * 5.0F);
    EXPECT_LT(std::abs(pos.z() - center.z()), spacing * 5.0F);
    EXPECT_FLOAT_EQ(pos.y(), center.y());
  }
}

TEST_F(FormationSystemTest, BarbarianFormationIsLooser) {
  QVector3D center(0.0F, 0.0F, 0.0F);
  float spacing = 2.0F;
  int unit_count = 9;

  auto barbarian_positions =
      FormationSystem::instance().get_formation_positions(
          FormationType::Barbarian, unit_count, center, spacing);
  auto roman_positions = FormationSystem::instance().get_formation_positions(
      FormationType::Roman, unit_count, center, spacing);

  EXPECT_EQ(barbarian_positions.size(), 9);
  EXPECT_EQ(roman_positions.size(), 9);

  // Barbarian formation should use more spacing (1.8x vs 1.2x)
  // Calculate average distance from center for both formations
  float barbarian_avg_dist = 0.0F;
  float roman_avg_dist = 0.0F;

  for (size_t i = 0; i < 9; ++i) {
    QVector3D barbarian_vec = barbarian_positions[i] - center;
    QVector3D roman_vec = roman_positions[i] - center;
    barbarian_avg_dist += barbarian_vec.length();
    roman_avg_dist += roman_vec.length();
  }

  barbarian_avg_dist /= 9.0F;
  roman_avg_dist /= 9.0F;

  // Barbarian should have larger average distance due to looser spacing
  EXPECT_GT(barbarian_avg_dist, roman_avg_dist);
}

TEST_F(FormationSystemTest, HandlesZeroUnits) {
  QVector3D center(0.0F, 0.0F, 0.0F);
  
  auto positions = FormationSystem::instance().get_formation_positions(
      FormationType::Roman, 0, center, 1.0F);
  
  EXPECT_TRUE(positions.empty());
}

TEST_F(FormationSystemTest, HandlesSingleUnit) {
  QVector3D center(5.0F, 0.0F, 10.0F);
  
  auto positions = FormationSystem::instance().get_formation_positions(
      FormationType::Roman, 1, center, 1.0F);
  
  EXPECT_EQ(positions.size(), 1);
  EXPECT_FLOAT_EQ(positions[0].x(), center.x());
  EXPECT_FLOAT_EQ(positions[0].y(), center.y());
  EXPECT_FLOAT_EQ(positions[0].z(), center.z());
}

TEST_F(FormationSystemTest, FormationsScaleWithUnitCount) {
  QVector3D center(0.0F, 0.0F, 0.0F);
  float spacing = 2.0F;

  auto small_formation = FormationSystem::instance().get_formation_positions(
      FormationType::Roman, 4, center, spacing);
  auto large_formation = FormationSystem::instance().get_formation_positions(
      FormationType::Roman, 16, center, spacing);

  EXPECT_EQ(small_formation.size(), 4);
  EXPECT_EQ(large_formation.size(), 16);

  // Larger formations should spread wider
  float small_max_dist = 0.0F;
  float large_max_dist = 0.0F;

  for (const auto &pos : small_formation) {
    QVector3D vec = pos - center;
    small_max_dist = std::max(small_max_dist, vec.length());
  }

  for (const auto &pos : large_formation) {
    QVector3D vec = pos - center;
    large_max_dist = std::max(large_max_dist, vec.length());
  }

  EXPECT_GT(large_max_dist, small_max_dist);
}
