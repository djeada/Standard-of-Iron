#include <QVector3D>

#include <gtest/gtest.h>

#include "render/entity/horse_archer_renderer_base.h"
#include "render/entity/horse_spearman_renderer_base.h"
#include "render/entity/mounted_knight_renderer_base.h"
#include "render/humanoid/humanoid_proportion_profiles.h"

using namespace Render::GL;

namespace {

constexpr auto k_mounted_base_profile =
    Render::GL::Humanoid::k_mounted_rider_proportion_profile;
constexpr Render::GL::Humanoid::ProportionOffset k_spearman_profile_tolerance{
    .x = 0.03F, .y = 0.02F, .z = 0.03F};

} // namespace

class RiderProportionsTest : public ::testing::Test {
protected:
  static auto make_mounted_knight_renderer() -> MountedKnightRendererBase {
    MountedKnightRendererConfig config;
    config.rider_debug_name = "tests/mounted_knight/rider";
    config.mount_debug_name = "tests/mounted_knight/horse";
    return MountedKnightRendererBase(std::move(config));
  }

  static auto make_horse_archer_renderer() -> HorseArcherRendererBase {
    HorseArcherRendererConfig config;
    config.rider_debug_name = "tests/horse_archer/rider";
    config.mount_debug_name = "tests/horse_archer/horse";
    return HorseArcherRendererBase(std::move(config));
  }

  static auto make_horse_spearman_renderer() -> HorseSpearmanRendererBase {
    HorseSpearmanRendererConfig config;
    config.rider_debug_name = "tests/horse_spearman/rider";
    config.mount_debug_name = "tests/horse_spearman/horse";
    return HorseSpearmanRendererBase(std::move(config));
  }

  template <typename Renderer>
  void expect_visual_spec_matches_runtime_scaling(Renderer& renderer) {
    QVector3D const proportions = renderer.get_proportion_scaling();
    auto const spec = renderer.visual_spec();
    EXPECT_FLOAT_EQ(spec.scaling.x, proportions.x());
    EXPECT_FLOAT_EQ(spec.scaling.y, proportions.y());
    EXPECT_FLOAT_EQ(spec.scaling.z, proportions.z());
  }

  template <typename Renderer>
  void expect_profile_equals(Renderer& renderer,
                             const Render::GL::Humanoid::ProportionProfile& expected) {
    QVector3D const proportions = renderer.get_proportion_scaling();
    EXPECT_FLOAT_EQ(proportions.x(), expected.x);
    EXPECT_FLOAT_EQ(proportions.y(), expected.y);
    EXPECT_FLOAT_EQ(proportions.z(), expected.z);
    expect_visual_spec_matches_runtime_scaling(renderer);
  }

  template <typename Renderer>
  void expect_profile_near(Renderer& renderer,
                           const Render::GL::Humanoid::ProportionProfile& expected,
                           const Render::GL::Humanoid::ProportionOffset& tolerance) {
    QVector3D const proportions = renderer.get_proportion_scaling();
    EXPECT_NEAR(proportions.x(), expected.x, tolerance.x);
    EXPECT_NEAR(proportions.y(), expected.y, tolerance.y);
    EXPECT_NEAR(proportions.z(), expected.z, tolerance.z);
    expect_visual_spec_matches_runtime_scaling(renderer);
  }

  void expect_balanced_profile(const QVector3D& proportions) {
    float const width_height_ratio = proportions.x() / proportions.y();
    EXPECT_GE(width_height_ratio, 0.5F);
    EXPECT_LE(width_height_ratio, 2.0F);

    float const depth_height_ratio = proportions.z() / proportions.y();
    EXPECT_GE(depth_height_ratio, 0.5F);
    EXPECT_LE(depth_height_ratio, 2.0F);

    float const avg_lateral = (proportions.x() + proportions.z()) / 2.0F;
    float const height_vs_lateral = proportions.y() / avg_lateral;
    EXPECT_GE(height_vs_lateral, 0.7F);
    EXPECT_LE(height_vs_lateral, 1.3F);
  }
};

TEST_F(RiderProportionsTest, MountedKnightBaseHasRealisticProportions) {
  auto renderer = make_mounted_knight_renderer();
  expect_profile_equals(renderer, k_mounted_base_profile);
  expect_balanced_profile(renderer.get_proportion_scaling());
}

TEST_F(RiderProportionsTest, MountedArcherBaseHasRealisticProportions) {
  auto renderer = make_horse_archer_renderer();
  expect_profile_equals(renderer, k_mounted_base_profile);
  expect_balanced_profile(renderer.get_proportion_scaling());
}

TEST_F(RiderProportionsTest, MountedSpearmanBaseHasRealisticProportions) {
  auto renderer = make_horse_spearman_renderer();
  expect_profile_near(renderer, k_mounted_base_profile, k_spearman_profile_tolerance);
  expect_balanced_profile(renderer.get_proportion_scaling());
}

TEST_F(RiderProportionsTest, MountedRiderBasesStayConsistentAcrossRoles) {
  auto mounted_knight = make_mounted_knight_renderer();
  auto horse_archer = make_horse_archer_renderer();
  auto horse_spearman = make_horse_spearman_renderer();

  QVector3D const knight_props = mounted_knight.get_proportion_scaling();
  QVector3D const archer_props = horse_archer.get_proportion_scaling();
  QVector3D const spearman_props = horse_spearman.get_proportion_scaling();

  EXPECT_FLOAT_EQ(knight_props.x(), archer_props.x());
  EXPECT_FLOAT_EQ(knight_props.y(), archer_props.y());
  EXPECT_FLOAT_EQ(knight_props.z(), archer_props.z());

  EXPECT_NEAR(knight_props.x(), spearman_props.x(), k_spearman_profile_tolerance.x);
  EXPECT_NEAR(knight_props.y(), spearman_props.y(), k_spearman_profile_tolerance.y);
  EXPECT_NEAR(knight_props.z(), spearman_props.z(), k_spearman_profile_tolerance.z);
}

TEST_F(RiderProportionsTest, MountedRiderBasesAvoidExtremeRatios) {
  auto renderer = make_mounted_knight_renderer();
  expect_balanced_profile(renderer.get_proportion_scaling());
}
