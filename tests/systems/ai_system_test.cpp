#include "game/systems/ai_system.h"
#include "game/systems/owner_registry.h"

#include <gtest/gtest.h>

namespace {

class AISystemTest : public ::testing::Test {
protected:
  void SetUp() override { Game::Systems::OwnerRegistry::instance().clear(); }

  void TearDown() override { Game::Systems::OwnerRegistry::instance().clear(); }
};

TEST_F(AISystemTest, ReinitializePicksUpOwnersRegisteredAfterConstruction) {
  Game::Systems::AISystem ai_system;

  EXPECT_EQ(ai_system.ai_player_count(), 0U);

  auto &owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "Player");
  owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "Opponent");

  EXPECT_EQ(ai_system.ai_player_count(), 0U);

  ai_system.reinitialize();

  EXPECT_EQ(ai_system.ai_player_count(), 1U);
}

TEST_F(AISystemTest, ReinitializeStaggersInitialUpdateTimersAcrossAIPlayers) {
  auto &owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "Player");
  owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "AI-1");
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "AI-2");
  owners.register_owner_with_id(4, Game::Systems::OwnerType::AI, "AI-3");

  Game::Systems::AISystem ai_system;
  ai_system.set_update_interval(0.6F);
  ai_system.reinitialize();

  ASSERT_EQ(ai_system.ai_player_count(), 3U);
  EXPECT_FLOAT_EQ(ai_system.ai_update_timer(0), 0.0F);
  EXPECT_FLOAT_EQ(ai_system.ai_update_timer(1), 0.2F);
  EXPECT_FLOAT_EQ(ai_system.ai_update_timer(2), 0.4F);
}

} // namespace
