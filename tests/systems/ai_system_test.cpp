#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_reasoner.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/ai_system/behaviors/attack_behavior.h"
#include "game/systems/ai_system/behaviors/gather_behavior.h"
#include "game/systems/owner_registry.h"

#include <gtest/gtest.h>

namespace {

class AISystemTest : public ::testing::Test {
protected:
  void SetUp() override { Game::Systems::OwnerRegistry::instance().clear(); }

  void TearDown() override { Game::Systems::OwnerRegistry::instance().clear(); }

  static auto make_unit(Engine::Core::EntityID id, float x,
                        float z) -> Game::Systems::AI::EntitySnapshot {
    Game::Systems::AI::EntitySnapshot unit;
    unit.id = id;
    unit.spawn_type = Game::Units::SpawnType::Spearman;
    unit.owner_id = 3;
    unit.health = 100;
    unit.max_health = 100;
    unit.pos_x = x;
    unit.pos_z = z;
    unit.movement.has_component = true;
    unit.movement.has_target = false;
    return unit;
  }

  static auto make_enemy(Engine::Core::EntityID id, float x,
                         float z) -> Game::Systems::AI::ContactSnapshot {
    Game::Systems::AI::ContactSnapshot enemy;
    enemy.id = id;
    enemy.owner_id = 7;
    enemy.health = 100;
    enemy.max_health = 100;
    enemy.pos_x = x;
    enemy.pos_z = z;
    enemy.spawn_type = Game::Units::SpawnType::Spearman;
    return enemy;
  }
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

TEST_F(AISystemTest, AIReasonerBuildsBaseAnchorFromUnitClusterWithoutBarracks) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.friendly_units = {
      make_unit(1, 28.0F, 25.0F), make_unit(2, 30.0F, 27.0F),
      make_unit(3, 48.0F, 58.0F), make_unit(4, 50.0F, 56.0F),
      make_unit(5, 58.0F, 60.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  EXPECT_TRUE(context.has_base_anchor);
  EXPECT_EQ(context.primary_barracks, 0U);
  EXPECT_FLOAT_EQ(context.base_pos_x, 52.0F);
  EXPECT_FLOAT_EQ(context.base_pos_z, 58.0F);
  EXPECT_FLOAT_EQ(context.rally_x, 47.0F);
  EXPECT_FLOAT_EQ(context.rally_z, 58.0F);
}

TEST_F(AISystemTest, GatherBehaviorUsesUnitAnchorWithoutBarracks) {
  Game::Systems::AI::GatherBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.friendly_units = {make_unit(1, 10.0F, 10.0F)};

  Game::Systems::AI::AIContext context;
  context.state = Game::Systems::AI::AIState::Gathering;
  context.has_base_anchor = true;
  context.rally_x = 40.0F;
  context.rally_z = 50.0F;

  ASSERT_TRUE(behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.1F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::MoveUnits);
  ASSERT_EQ(commands.front().units.size(), 1U);
  EXPECT_EQ(commands.front().units.front(), 1U);
}

TEST_F(AISystemTest, AttackBehaviorScoutsFromUnitAnchorWithoutBarracks) {
  Game::Systems::AI::AttackBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.friendly_units = {
      make_unit(1, 30.0F, 20.0F),
      make_unit(2, 32.0F, 22.0F),
      make_unit(3, 34.0F, 24.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Attacking;
  context.total_units = 3;
  context.has_base_anchor = true;
  context.base_pos_x = 40.0F;
  context.base_pos_z = 50.0F;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.6F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::MoveUnits);
  ASSERT_EQ(commands.front().move_target_x.size(), 3U);
  ASSERT_EQ(commands.front().move_target_z.size(), 3U);

  float average_target_x = 0.0F;
  float average_target_z = 0.0F;
  for (std::size_t index = 0; index < commands.front().move_target_x.size();
       ++index) {
    average_target_x += commands.front().move_target_x[index];
    average_target_z += commands.front().move_target_z[index];
  }
  average_target_x /= static_cast<float>(commands.front().move_target_x.size());
  average_target_z /= static_cast<float>(commands.front().move_target_z.size());

  EXPECT_FLOAT_EQ(average_target_x, 40.0F);
  EXPECT_FLOAT_EQ(average_target_z, 90.0F);
}

TEST_F(AISystemTest, AIReasonerKeepsDefensiveAIInGatheringWhenEnemyIsDistant) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_unit(1, 30.0F, 20.0F),
      make_unit(2, 32.0F, 22.0F),
      make_unit(3, 34.0F, 24.0F),
  };
  snapshot.visible_enemies = {make_enemy(101, 140.0F, 140.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Gathering;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  context.state = Game::Systems::AI::AIState::Gathering;
  context.state_timer = 3.1F;
  context.decision_timer = 2.1F;

  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.0F);

  EXPECT_EQ(context.state, Game::Systems::AI::AIState::Gathering);
}

TEST_F(AISystemTest,
       AttackBehaviorDoesNotAdvanceDefensiveGatheringForceTowardDistantEnemy) {
  Game::Systems::AI::AttackBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.friendly_units = {
      make_unit(1, 30.0F, 20.0F),
      make_unit(2, 32.0F, 22.0F),
      make_unit(3, 34.0F, 24.0F),
  };
  snapshot.visible_enemies = {make_enemy(101, 90.0F, 90.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Gathering;
  context.total_units = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.6F, commands);

  EXPECT_TRUE(commands.empty());
}

} // namespace
