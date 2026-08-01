#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "game/map/visibility_service.h"

namespace {

using Game::Map::VisibilityService;
using Game::Map::VisibilityState;

auto state_at(int x, int z) -> VisibilityState {
  return VisibilityService::instance().state_at(x, z);
}

} // namespace

TEST(VisibilityRestore, SavedExplorationComesBackAsExplored) {
  auto& visibility = VisibilityService::instance();
  visibility.initialize(8, 8, 1.0F);

  std::vector<std::uint8_t> explored(64, 0U);
  explored[3 * 8 + 2] = 1U;

  ASSERT_TRUE(visibility.restore_explored(explored, 8, 8));

  EXPECT_EQ(state_at(2, 3), VisibilityState::Explored);
  EXPECT_EQ(state_at(0, 0), VisibilityState::Unseen);

  visibility.reset();
}

TEST(VisibilityRestore, DoesNotDowngradeWhatUnitsAlreadySee) {
  auto& visibility = VisibilityService::instance();
  visibility.initialize(4, 4, 1.0F);
  visibility.reveal_all();
  ASSERT_EQ(state_at(1, 1), VisibilityState::Visible);

  const std::vector<std::uint8_t> explored(16, 0U);
  ASSERT_TRUE(visibility.restore_explored(explored, 4, 4));

  EXPECT_EQ(state_at(1, 1), VisibilityState::Visible);

  visibility.reset();
}

TEST(VisibilityRestore, RejectsAMaskFromADifferentlySizedMap) {
  auto& visibility = VisibilityService::instance();
  visibility.initialize(4, 4, 1.0F);

  const std::vector<std::uint8_t> explored(64, 1U);

  EXPECT_FALSE(visibility.restore_explored(explored, 8, 8));
  EXPECT_EQ(state_at(1, 1), VisibilityState::Unseen);

  visibility.reset();
}

TEST(VisibilityRestore, PublishesANewSnapshotSoTheFogRedraws) {
  auto& visibility = VisibilityService::instance();
  visibility.initialize(8, 8, 1.0F);
  const auto before = visibility.snapshot().version;

  std::vector<std::uint8_t> explored(64, 0U);
  explored[10] = 1U;
  ASSERT_TRUE(visibility.restore_explored(explored, 8, 8));

  const auto after = visibility.snapshot();
  EXPECT_GT(after.version, before);
  EXPECT_EQ(static_cast<VisibilityState>(after.cells[10]), VisibilityState::Explored);

  visibility.reset();
}
