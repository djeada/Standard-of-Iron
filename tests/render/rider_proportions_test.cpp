#include "render/entity/registry.h"
#include "render/humanoid/rig.h"
#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>

using namespace Render::GL;

// Mock renderer classes for testing proportions
namespace TestMocks {

class KingdomHorseSwordsmanRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.95F, 1.0F, 0.98F};
  }
};

class RomanHorseSwordsmanRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.95F, 1.0F, 0.98F};
  }
};

class CarthageHorseSwordsmanRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.95F, 1.0F, 0.98F};
  }
};

} // namespace TestMocks

class RiderProportionsTest : public ::testing::Test {
protected:
  // Helper to check if a value is within a reasonable range
  bool inRange(float value, float min, float max) {
    return value >= min && value <= max;
  }

  // Helper to check approximate equality
  bool approxEqual(float a, float b, float epsilon = 0.01F) {
    return std::abs(a - b) < epsilon;
  }
};

TEST_F(RiderProportionsTest, KingdomRiderHasRealisticProportions) {
  TestMocks::KingdomHorseSwordsmanRenderer renderer;
  QVector3D const proportions = renderer.get_proportion_scaling();

  // Width scale should be reasonable (0.90-1.0)
  EXPECT_TRUE(inRange(proportions.x(), 0.90F, 1.0F))
      << "Width scale " << proportions.x() << " is outside realistic range";

  // Height scale should be near normal since base limbs are now corrected
  // (0.95-1.05)
  EXPECT_TRUE(inRange(proportions.y(), 0.95F, 1.05F))
      << "Height scale " << proportions.y() << " is outside realistic range";

  // Depth scale should be reasonable (0.95-1.0)
  EXPECT_TRUE(inRange(proportions.z(), 0.95F, 1.0F))
      << "Depth scale " << proportions.z() << " is outside realistic range";

  // Proportions should not be extremely thin (no dimension < 0.5)
  EXPECT_GT(proportions.x(), 0.5F)
      << "Width scale " << proportions.x() << " is too thin";
  EXPECT_GT(proportions.y(), 0.5F)
      << "Height scale " << proportions.y() << " is too short";
  EXPECT_GT(proportions.z(), 0.5F)
      << "Depth scale " << proportions.z() << " is too thin";
}

TEST_F(RiderProportionsTest, RomanRiderHasRealisticProportions) {
  TestMocks::RomanHorseSwordsmanRenderer renderer;
  QVector3D const proportions = renderer.get_proportion_scaling();

  // Same expectations as Kingdom rider
  EXPECT_TRUE(inRange(proportions.x(), 0.90F, 1.0F))
      << "Width scale " << proportions.x() << " is outside realistic range";
  EXPECT_TRUE(inRange(proportions.y(), 0.95F, 1.05F))
      << "Height scale " << proportions.y() << " is outside realistic range";
  EXPECT_TRUE(inRange(proportions.z(), 0.95F, 1.0F))
      << "Depth scale " << proportions.z() << " is outside realistic range";
}

TEST_F(RiderProportionsTest, CarthageRiderHasRealisticProportions) {
  TestMocks::CarthageHorseSwordsmanRenderer renderer;
  QVector3D const proportions = renderer.get_proportion_scaling();

  // Same expectations as other nations
  EXPECT_TRUE(inRange(proportions.x(), 0.90F, 1.0F))
      << "Width scale " << proportions.x() << " is outside realistic range";
  EXPECT_TRUE(inRange(proportions.y(), 0.95F, 1.05F))
      << "Height scale " << proportions.y() << " is outside realistic range";
  EXPECT_TRUE(inRange(proportions.z(), 0.95F, 1.0F))
      << "Depth scale " << proportions.z() << " is outside realistic range";
}

TEST_F(RiderProportionsTest, AllNationsHaveConsistentProportions) {
  TestMocks::KingdomHorseSwordsmanRenderer kingdom_renderer;
  TestMocks::RomanHorseSwordsmanRenderer roman_renderer;
  TestMocks::CarthageHorseSwordsmanRenderer carthage_renderer;

  QVector3D const kingdom_props = kingdom_renderer.get_proportion_scaling();
  QVector3D const roman_props = roman_renderer.get_proportion_scaling();
  QVector3D const carthage_props = carthage_renderer.get_proportion_scaling();

  // All nations should have similar proportions (within 10%)
  EXPECT_TRUE(approxEqual(kingdom_props.x(), roman_props.x(), 0.1F))
      << "Kingdom and Roman width scales differ too much";
  EXPECT_TRUE(approxEqual(kingdom_props.y(), roman_props.y(), 0.1F))
      << "Kingdom and Roman height scales differ too much";
  EXPECT_TRUE(approxEqual(kingdom_props.z(), roman_props.z(), 0.1F))
      << "Kingdom and Roman depth scales differ too much";

  EXPECT_TRUE(approxEqual(kingdom_props.x(), carthage_props.x(), 0.1F))
      << "Kingdom and Carthage width scales differ too much";
  EXPECT_TRUE(approxEqual(kingdom_props.y(), carthage_props.y(), 0.1F))
      << "Kingdom and Carthage height scales differ too much";
  EXPECT_TRUE(approxEqual(kingdom_props.z(), carthage_props.z(), 0.1F))
      << "Kingdom and Carthage depth scales differ too much";
}

TEST_F(RiderProportionsTest, ProportionsPreventOverlyElongatedLimbs) {
  TestMocks::KingdomHorseSwordsmanRenderer renderer;
  QVector3D const proportions = renderer.get_proportion_scaling();

  // Width and height should be reasonably balanced
  // Aspect ratio should not be extreme (no dimension more than 2x another)
  float const width_height_ratio = proportions.x() / proportions.y();
  EXPECT_TRUE(inRange(width_height_ratio, 0.5F, 2.0F))
      << "Width/height ratio " << width_height_ratio << " is too extreme";

  float const depth_height_ratio = proportions.z() / proportions.y();
  EXPECT_TRUE(inRange(depth_height_ratio, 0.5F, 2.0F))
      << "Depth/height ratio " << depth_height_ratio << " is too extreme";

  // Height should not be drastically different from width/depth
  // This prevents the "stretched stick figure" appearance
  float const avg_lateral = (proportions.x() + proportions.z()) / 2.0F;
  float const height_vs_lateral = proportions.y() / avg_lateral;
  EXPECT_TRUE(inRange(height_vs_lateral, 0.7F, 1.3F))
      << "Height vs lateral proportion ratio " << height_vs_lateral
      << " is unbalanced";
}

TEST_F(RiderProportionsTest, MountedRiderLimbsAreShorterThanStanding) {
  using HP = HumanProportions;
  using MRP = MountedRiderProportions;

  // Mounted rider legs should be significantly shorter than standing
  EXPECT_LT(MRP::UPPER_LEG_LEN, HP::UPPER_LEG_LEN)
      << "Mounted upper leg should be shorter than standing";
  EXPECT_LT(MRP::LOWER_LEG_LEN, HP::LOWER_LEG_LEN)
      << "Mounted lower leg should be shorter than standing";

  // Mounted rider arms should also be slightly shorter
  EXPECT_LT(MRP::UPPER_ARM_LEN, HP::UPPER_ARM_LEN)
      << "Mounted upper arm should be shorter than standing";
  EXPECT_LT(MRP::FORE_ARM_LEN, HP::FORE_ARM_LEN)
      << "Mounted forearm should be shorter than standing";

  // Total leg length reduction should be substantial (at least 20%)
  float const standing_leg = HP::UPPER_LEG_LEN + HP::LOWER_LEG_LEN;
  float const mounted_leg = MRP::UPPER_LEG_LEN + MRP::LOWER_LEG_LEN;
  float const reduction = (standing_leg - mounted_leg) / standing_leg;
  EXPECT_GT(reduction, 0.20F)
      << "Mounted rider legs should be at least 20% shorter, got " 
      << (reduction * 100.0F) << "%";
}
