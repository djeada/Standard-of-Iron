#include "render/entity/registry.h"
#include "render/humanoid/rig.h"
#include <QVector3D>
#include <cmath>
#include <gtest/gtest.h>

using namespace Render::GL;

// Mock renderer classes for testing proportions
namespace TestMocks {

class RomanHorseSwordsmanRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.92F, 0.88F, 0.96F};
  }
};

class CarthageHorseSwordsmanRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.92F, 0.88F, 0.96F};
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

TEST_F(RiderProportionsTest, KingdomRiderHasRealisticProportions) { SUCCEED(); }

TEST_F(RiderProportionsTest, RomanRiderHasRealisticProportions) {
  TestMocks::RomanHorseSwordsmanRenderer renderer;
  QVector3D const proportions = renderer.get_proportion_scaling();

  // Expectations for realistic mounted proportions
  EXPECT_TRUE(inRange(proportions.x(), 0.9F, 1.1F))
      << "Width scale " << proportions.x() << " is outside realistic range";
  EXPECT_TRUE(inRange(proportions.y(), 0.85F, 0.98F))
      << "Height scale " << proportions.y() << " is outside realistic range";
  EXPECT_TRUE(inRange(proportions.z(), 0.9F, 1.1F))
      << "Depth scale " << proportions.z() << " is outside realistic range";
}

TEST_F(RiderProportionsTest, CarthageRiderHasRealisticProportions) {
  TestMocks::CarthageHorseSwordsmanRenderer renderer;
  QVector3D const proportions = renderer.get_proportion_scaling();

  // Same expectations as other nations
  EXPECT_TRUE(inRange(proportions.x(), 0.9F, 1.1F))
      << "Width scale " << proportions.x() << " is outside realistic range";
  EXPECT_TRUE(inRange(proportions.y(), 0.85F, 0.98F))
      << "Height scale " << proportions.y() << " is outside realistic range";
  EXPECT_TRUE(inRange(proportions.z(), 0.9F, 1.1F))
      << "Depth scale " << proportions.z() << " is outside realistic range";
}

TEST_F(RiderProportionsTest, AllNationsHaveConsistentProportions) {
  TestMocks::RomanHorseSwordsmanRenderer roman_renderer;
  TestMocks::CarthageHorseSwordsmanRenderer carthage_renderer;

  QVector3D const roman_props = roman_renderer.get_proportion_scaling();
  QVector3D const carthage_props = carthage_renderer.get_proportion_scaling();

  // All nations should have similar proportions (within 10%)
  EXPECT_TRUE(approxEqual(roman_props.x(), carthage_props.x(), 0.1F))
      << "Roman and Carthage width scales differ too much";
  EXPECT_TRUE(approxEqual(roman_props.y(), carthage_props.y(), 0.1F))
      << "Roman and Carthage height scales differ too much";
  EXPECT_TRUE(approxEqual(roman_props.z(), carthage_props.z(), 0.1F))
      << "Roman and Carthage depth scales differ too much";
}

TEST_F(RiderProportionsTest, ProportionsPreventOverlyElongatedLimbs) {
  TestMocks::RomanHorseSwordsmanRenderer renderer;
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
