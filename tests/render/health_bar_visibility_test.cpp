#include <gtest/gtest.h>

#include "render/entity/health_bar_visibility.h"

namespace {

using Render::GL::health_bar_visible;
using Render::GL::HealthBarVisibilityInputs;

auto healthy_idle() -> HealthBarVisibilityInputs {
  HealthBarVisibilityInputs inputs;
  inputs.alive = true;
  inputs.full_health = true;
  inputs.camera_distance = 20.0F;
  return inputs;
}

TEST(HealthBarVisibilityTest, AFullHealthIdleUnitShowsNoBar) {
  EXPECT_FALSE(health_bar_visible(healthy_idle()));
}

TEST(HealthBarVisibilityTest, FiveHundredHealthyIdleSoldiersProduceNoBars) {
  int visible = 0;
  for (int soldier = 0; soldier < 500; ++soldier) {
    auto inputs = healthy_idle();
    inputs.camera_distance = 12.0F + static_cast<float>(soldier % 30);
    if (health_bar_visible(inputs)) {
      ++visible;
    }
  }
  EXPECT_EQ(visible, 0);
}

TEST(HealthBarVisibilityTest, SelectionAndHoverAlwaysShowTheBar) {
  auto selected = healthy_idle();
  selected.selected = true;
  EXPECT_TRUE(health_bar_visible(selected));

  auto hovered = healthy_idle();
  hovered.hovered = true;
  EXPECT_TRUE(health_bar_visible(hovered));
}

TEST(HealthBarVisibilityTest, TheCommanderTargetAlwaysShowsTheBar) {
  auto inputs = healthy_idle();
  inputs.commander_target = true;
  EXPECT_TRUE(health_bar_visible(inputs));
}

TEST(HealthBarVisibilityTest, RecentDamageShowsTheBar) {
  auto inputs = healthy_idle();
  inputs.recently_damaged = true;
  inputs.full_health = false;
  EXPECT_TRUE(health_bar_visible(inputs));

  inputs.recently_damaged = false;
  EXPECT_FALSE(health_bar_visible(inputs))
      << "a unit that stopped taking damage kept its bar";
}

TEST(HealthBarVisibilityTest, AnUntouchedStructureAtFullHealthStaysQuiet) {
  auto intact = healthy_idle();
  intact.recently_damaged = true;
  EXPECT_FALSE(health_bar_visible(intact))
      << "a structure at full health showed a bar it did not need";

  auto damaged = intact;
  damaged.full_health = false;
  EXPECT_TRUE(health_bar_visible(damaged));
}

TEST(HealthBarVisibilityTest, DeadEntitiesNeverShowABar) {
  auto inputs = healthy_idle();
  inputs.alive = false;
  inputs.selected = true;
  inputs.hovered = true;
  inputs.recently_damaged = true;
  inputs.commander_target = true;
  EXPECT_FALSE(health_bar_visible(inputs));
}

TEST(HealthBarVisibilityTest, DistantEntitiesFallBackToGroupInformation) {
  auto inputs = healthy_idle();
  inputs.selected = true;
  inputs.recently_damaged = true;
  inputs.camera_distance = Render::GL::k_health_bar_max_camera_distance + 1.0F;
  EXPECT_FALSE(health_bar_visible(inputs));

  inputs.camera_distance = Render::GL::k_health_bar_max_camera_distance - 1.0F;
  EXPECT_TRUE(health_bar_visible(inputs));
}

TEST(HealthBarVisibilityTest, TheRecentDamageWindowIsThreeSeconds) {
  EXPECT_FLOAT_EQ(Render::GL::k_health_bar_recent_damage_seconds, 3.0F);
}

} // namespace
