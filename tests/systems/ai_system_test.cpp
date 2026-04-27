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

} // namespace
