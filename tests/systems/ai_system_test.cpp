#include <gtest/gtest.h>

#include <algorithm>

#include "game/core/component.h"
#include "game/core/ownership_constants.h"
#include "game/core/world.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_command_filter.h"
#include "game/systems/ai_system/ai_reasoner.h"
#include "game/systems/ai_system/ai_snapshot_builder.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/ai_system/ai_utils.h"
#include "game/systems/ai_system/behaviors/attack_behavior.h"
#include "game/systems/ai_system/behaviors/builder_behavior.h"
#include "game/systems/ai_system/behaviors/defend_behavior.h"
#include "game/systems/ai_system/behaviors/expand_behavior.h"
#include "game/systems/ai_system/behaviors/gather_behavior.h"
#include "game/systems/ai_system/behaviors/harass_behavior.h"
#include "game/systems/owner_registry.h"

namespace {

class AISystemTest : public ::testing::Test {
protected:
  void SetUp() override { Game::Systems::OwnerRegistry::instance().clear(); }

  void TearDown() override { Game::Systems::OwnerRegistry::instance().clear(); }

  static auto make_unit(Engine::Core::EntityID id,
                        float x,
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

  static auto make_enemy(Engine::Core::EntityID id,
                         float x,
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

  static auto make_builder(Engine::Core::EntityID id,
                           float x,
                           float z) -> Game::Systems::AI::EntitySnapshot {
    auto builder = make_unit(id, x, z);
    builder.spawn_type = Game::Units::SpawnType::Builder;
    builder.builder_production.has_component = true;
    return builder;
  }

  static auto make_building(Engine::Core::EntityID id,
                            float x,
                            float z,
                            Game::Units::SpawnType spawn_type)
      -> Game::Systems::AI::EntitySnapshot {
    auto building = make_unit(id, x, z);
    building.is_building = true;
    building.spawn_type = spawn_type;
    return building;
  }

  static auto add_world_unit(Engine::Core::World& world,
                             int owner_id,
                             float x,
                             float z,
                             float vision_range,
                             bool ai_controlled,
                             bool is_building = false,
                             Game::Units::SpawnType spawn_type =
                                 Game::Units::SpawnType::Spearman) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    (void)entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit =
        entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, vision_range);
    unit->owner_id = owner_id;
    unit->spawn_type = spawn_type;
    if (ai_controlled) {
      (void)entity->add_component<Engine::Core::AIControlledComponent>();
    }
    if (is_building) {
      (void)entity->add_component<Engine::Core::BuildingComponent>();
    }
    return entity;
  }
};

TEST_F(AISystemTest, ReinitializePicksUpOwnersRegisteredAfterConstruction) {
  Game::Systems::AISystem ai_system;

  EXPECT_EQ(ai_system.ai_player_count(), 0U);

  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "Player");
  owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "Opponent");

  EXPECT_EQ(ai_system.ai_player_count(), 0U);

  ai_system.reinitialize();

  EXPECT_EQ(ai_system.ai_player_count(), 1U);
}

TEST_F(AISystemTest, ReinitializeStaggersInitialUpdateTimersAcrossAIPlayers) {
  auto& owners = Game::Systems::OwnerRegistry::instance();
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

TEST_F(AISystemTest, DifficultyChangesExecutionKnobsWithoutChangingStyleIdentity) {
  auto easy = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Balanced);
  Game::Systems::AI::AIStrategyFactory::apply_personality(easy, 0.72F, 0.38F, 0.41F);

  auto hard = easy;
  Game::Systems::AI::AIStrategyFactory::apply_difficulty(easy, "easy");
  Game::Systems::AI::AIStrategyFactory::apply_difficulty(hard, "hard");

  EXPECT_EQ(easy.reactive_attack_size, hard.reactive_attack_size);
  EXPECT_EQ(easy.proactive_attack_size, hard.proactive_attack_size);
  EXPECT_EQ(easy.desired_barracks_count, hard.desired_barracks_count);
  EXPECT_EQ(easy.target_builder_count, hard.target_builder_count);
  EXPECT_FLOAT_EQ(easy.attack_formation_spacing, hard.attack_formation_spacing);
  EXPECT_FLOAT_EQ(easy.gather_spacing, hard.gather_spacing);

  EXPECT_GT(easy.difficulty.update_interval_multiplier,
            hard.difficulty.update_interval_multiplier);
  EXPECT_LT(easy.difficulty.production_rate_multiplier,
            hard.difficulty.production_rate_multiplier);
}

TEST_F(AISystemTest, PersonalityChangesArmyCommitmentAndOrganization) {
  auto aggressive = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Balanced);
  auto defensive = aggressive;

  Game::Systems::AI::AIStrategyFactory::apply_personality(aggressive, 0.85F, 0.25F, 0.25F);
  Game::Systems::AI::AIStrategyFactory::apply_personality(defensive, 0.25F, 0.85F, 0.25F);

  EXPECT_LT(aggressive.reactive_attack_size, defensive.reactive_attack_size);
  EXPECT_LT(aggressive.proactive_attack_size, defensive.proactive_attack_size);
  EXPECT_LT(aggressive.reserve_units, defensive.reserve_units);
  EXPECT_LT(aggressive.attack_formation_spacing,
            defensive.attack_formation_spacing);
  EXPECT_GT(defensive.desired_defense_tower_count,
            aggressive.desired_defense_tower_count);
}

TEST_F(AISystemTest, PersonalityHarassmentControlsHarassForceSizing) {
  auto passive = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Harasser);
  auto active = passive;

  Game::Systems::AI::AIStrategyFactory::apply_personality(passive, 0.5F, 0.5F, 0.2F);
  Game::Systems::AI::AIStrategyFactory::apply_personality(active, 0.5F, 0.5F, 0.9F);

  EXPECT_EQ(passive.harass_units, 0);
  EXPECT_GT(active.harass_units, passive.harass_units);
}

TEST_F(AISystemTest, AIReasonerBuildsBaseAnchorFromUnitClusterWithoutBarracks) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.friendly_units = {
      make_unit(1, 28.0F, 25.0F),
      make_unit(2, 30.0F, 27.0F),
      make_unit(3, 48.0F, 58.0F),
      make_unit(4, 50.0F, 56.0F),
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

TEST_F(AISystemTest, AIReasonerKeepsPrimaryBarracksAnchoredToOriginalBase) {
  Game::Systems::AI::AISnapshot initial_snapshot;
  initial_snapshot.player_id = 3;
  initial_snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 38.0F, 40.0F),
      make_unit(2, 36.0F, 40.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;

  Game::Systems::AI::AIReasoner::update_context(initial_snapshot, context);
  ASSERT_EQ(context.primary_barracks, 50U);

  Game::Systems::AI::AISnapshot expanded_snapshot;
  expanded_snapshot.player_id = 3;
  expanded_snapshot.friendly_units = {
      make_building(60, 90.0F, 80.0F, Game::Units::SpawnType::Barracks),
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 38.0F, 40.0F),
      make_unit(2, 36.0F, 40.0F),
  };

  Game::Systems::AI::AIReasoner::update_context(expanded_snapshot, context);

  EXPECT_EQ(context.primary_barracks, 50U);
  EXPECT_FLOAT_EQ(context.base_pos_x, 40.0F);
  EXPECT_FLOAT_EQ(context.base_pos_z, 40.0F);
}

TEST_F(AISystemTest,
       AIReasonerChoosesExpansionSiteFromEnemyStrategicObjectiveAndIgnoresNeutralBarracks) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_building(60, 35.0F, 36.0F, Game::Units::SpawnType::Home),
      make_building(61, 34.0F, 44.0F, Game::Units::SpawnType::Home),
      make_building(62, 45.0F, 37.0F, Game::Units::SpawnType::Home),
      make_builder(10, 38.0F, 38.0F),
      make_unit(1, 36.0F, 40.0F),
      make_unit(2, 34.0F, 39.0F),
      make_unit(3, 35.0F, 42.0F),
  };

  auto neutral_barracks = make_enemy(201, 52.0F, 40.0F);
  neutral_barracks.is_building = true;
  neutral_barracks.spawn_type = Game::Units::SpawnType::Barracks;
  neutral_barracks.owner_id = Game::Core::NEUTRAL_OWNER_ID;

  auto enemy_base = make_enemy(301, 100.0F, 40.0F);
  enemy_base.is_building = true;
  enemy_base.spawn_type = Game::Units::SpawnType::Barracks;
  enemy_base.owner_id = 7;

  snapshot.strategic_objectives = {neutral_barracks, enemy_base};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Expansionist);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  EXPECT_TRUE(context.has_expansion_site);
  EXPECT_GT(context.expansion_site_x, 60.0F);
  EXPECT_LT(context.expansion_site_x, 80.0F);
  EXPECT_FLOAT_EQ(context.expansion_site_z, 40.0F);
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
  for (std::size_t index = 0; index < commands.front().move_target_x.size(); ++index) {
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

TEST_F(AISystemTest, ClaimUnitsRefreshesExistingSameTaskAssignments) {
  Game::Systems::AI::AIContext context;
  context.assigned_units[7] = {
      Game::Systems::AI::BehaviorPriority::Normal, 1.0F, "gathering"};

  const auto claimed = Game::Systems::AI::claim_units({7},
                                                      Game::Systems::AI::BehaviorPriority::Normal,
                                                      "gathering",
                                                      context,
                                                      5.0F,
                                                      2.0F);

  ASSERT_EQ(claimed.size(), 1U);
  EXPECT_EQ(claimed.front(), 7U);
  ASSERT_TRUE(context.assigned_units.contains(7));
  EXPECT_FLOAT_EQ(context.assigned_units.at(7).assignment_time, 5.0F);
}

TEST_F(AISystemTest, CleanupDeadUnitsRemovesStaleAssignmentsForAliveUnits) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {make_unit(7, 12.0F, 8.0F)};

  Game::Systems::AI::AIContext context;
  context.assigned_units[7] = {
      Game::Systems::AI::BehaviorPriority::Normal, 1.0F, "attacking"};

  const auto alive = Game::Systems::AI::cleanup_dead_units(
      snapshot, context, snapshot.game_time);

  EXPECT_EQ(alive.size(), 1U);
  EXPECT_TRUE(context.assigned_units.empty());
}

TEST_F(AISystemTest, EntityEngagementRequiresNearbyEnemyNotOldDamage) {
  auto unit = make_unit(7, 10.0F, 10.0F);
  unit.health = 80;

  EXPECT_FALSE(Game::Systems::AI::is_entity_engaged(unit, {}));
  EXPECT_TRUE(
      Game::Systems::AI::is_entity_engaged(unit, {make_enemy(9, 12.0F, 10.0F)}));
}

TEST_F(AISystemTest, CommandFilterKeepsNonDuplicateUnitsInGroupMoveCommands) {
  Game::Systems::AI::AICommandFilter filter;

  Game::Systems::AI::AICommand initial;
  initial.type = Game::Systems::AI::AICommandType::MoveUnits;
  initial.units = {1U, 2U};
  initial.move_target_x = {10.0F, 20.0F};
  initial.move_target_y = {0.0F, 0.0F};
  initial.move_target_z = {10.0F, 20.0F};

  auto first_pass = filter.filter({initial}, 0.0F);
  ASSERT_EQ(first_pass.size(), 1U);

  Game::Systems::AI::AICommand follow_up = initial;
  follow_up.move_target_x = {10.0F, 35.0F};
  follow_up.move_target_z = {10.0F, 35.0F};

  auto second_pass = filter.filter({follow_up}, 1.0F);

  ASSERT_EQ(second_pass.size(), 1U);
  ASSERT_EQ(second_pass.front().units.size(), 1U);
  EXPECT_EQ(second_pass.front().units.front(), 2U);
  ASSERT_EQ(second_pass.front().move_target_x.size(), 1U);
  EXPECT_FLOAT_EQ(second_pass.front().move_target_x.front(), 35.0F);
  EXPECT_FLOAT_EQ(second_pass.front().move_target_z.front(), 35.0F);
}

TEST_F(AISystemTest, BuilderBehaviorRequestsBarracksWhenNoneExist) {
  Game::Systems::AI::BuilderBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 5.0F;
  snapshot.friendly_units = {make_builder(11, 18.0F, 12.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.builder_count = 1;
  context.home_count = 2;
  context.defense_tower_count = 1;
  context.barracks_count = 0;
  context.base_pos_x = 40.0F;
  context.base_pos_z = 55.0F;

  ASSERT_TRUE(behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 3.1F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type,
            Game::Systems::AI::AICommandType::StartBuilderConstruction);
  ASSERT_NE(commands.front().construction_type, nullptr);
  EXPECT_STREQ(commands.front().construction_type, "barracks");
}

TEST_F(AISystemTest, BuilderBehaviorUsesStrategyDrivenBarracksTargets) {
  Game::Systems::AI::BuilderBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 5.0F;
  snapshot.friendly_units = {make_builder(11, 18.0F, 12.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.builder_count = 1;
  context.home_count = 3;
  context.defense_tower_count = 1;
  context.barracks_count = 1;
  context.base_pos_x = 40.0F;
  context.base_pos_z = 55.0F;
  context.macro_targets.home_count = 3;
  context.macro_targets.defense_tower_count = 1;
  context.macro_targets.barracks_count = 3;

  ASSERT_TRUE(behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 3.1F, commands);

  ASSERT_EQ(commands.size(), 1U);
  ASSERT_NE(commands.front().construction_type, nullptr);
  EXPECT_STREQ(commands.front().construction_type, "barracks");
}

TEST_F(AISystemTest, BuilderBehaviorUsesExpansionSiteForOutpostBarracks) {
  Game::Systems::AI::BuilderBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 8.0F;
  snapshot.friendly_units = {make_builder(11, 78.0F, 58.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Expanding;
  context.builder_count = 1;
  context.home_count = 3;
  context.barracks_count = 1;
  context.has_expansion_site = true;
  context.expansion_site_x = 80.0F;
  context.expansion_site_z = 60.0F;
  context.strategy_config.base_home_target = 3;
  context.strategy_config.desired_outpost_barracks_count = 1;
  context.outpost_barracks_count = 0;

  ASSERT_TRUE(behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 3.1F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type,
            Game::Systems::AI::AICommandType::StartBuilderConstruction);
  ASSERT_NE(commands.front().construction_type, nullptr);
  EXPECT_STREQ(commands.front().construction_type, "barracks");
  EXPECT_FLOAT_EQ(commands.front().construction_site_x, 80.0F);
  EXPECT_FLOAT_EQ(commands.front().construction_site_z, 60.0F);
}

TEST_F(AISystemTest, BuilderBehaviorSkipsDuplicateExpansionConstructionWhilePending) {
  Game::Systems::AI::BuilderBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 8.0F;
  snapshot.friendly_units = {make_builder(11, 78.0F, 58.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Expanding;
  context.builder_count = 1;
  context.home_count = 3;
  context.barracks_count = 1;
  context.has_expansion_site = true;
  context.expansion_site_x = 80.0F;
  context.expansion_site_z = 60.0F;
  context.expansion_construction_pending = true;
  context.strategy_config.base_home_target = 3;
  context.strategy_config.desired_outpost_barracks_count = 1;
  context.outpost_barracks_count = 0;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 3.1F, commands);

  EXPECT_TRUE(commands.empty());
}

TEST_F(AISystemTest, SnapshotBuilderFiltersEnemiesByOwnedVision) {
  Engine::Core::World world;
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "AI");
  owners.register_owner_with_id(7, Game::Systems::OwnerType::Player, "Enemy");

  auto* visible_enemy =
      add_world_unit(world, 7, 8.0F, 0.0F, 12.0F, false, false);
  (void)add_world_unit(world, 7, 30.0F, 0.0F, 12.0F, false, false);
  (void)add_world_unit(world, 3, 0.0F, 0.0F, 12.0F, true, false);

  const auto snapshot = Game::Systems::AI::AISnapshotBuilder::build(world, 3);

  ASSERT_EQ(snapshot.visible_enemies.size(), 1U);
  EXPECT_EQ(snapshot.visible_enemies.front().id, visible_enemy->get_id());
}

TEST_F(AISystemTest, SnapshotBuilderUsesBuildingVisionForBaseDefense) {
  Engine::Core::World world;
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "AI");
  owners.register_owner_with_id(7, Game::Systems::OwnerType::Player, "Enemy");

  auto* visible_enemy =
      add_world_unit(world, 7, 15.0F, 0.0F, 12.0F, false, false);
  (void)add_world_unit(world,
                       3,
                       0.0F,
                       0.0F,
                       5.0F,
                       true,
                       true,
                       Game::Units::SpawnType::Barracks);

  const auto snapshot = Game::Systems::AI::AISnapshotBuilder::build(world, 3);

  ASSERT_EQ(snapshot.visible_enemies.size(), 1U);
  EXPECT_EQ(snapshot.visible_enemies.front().id, visible_enemy->get_id());
}

TEST_F(AISystemTest, SnapshotBuilderKeepsHiddenStrategicObjectives) {
  Engine::Core::World world;
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "AI");
  owners.register_owner_with_id(7, Game::Systems::OwnerType::Player, "Enemy");

  auto* hidden_enemy_base =
      add_world_unit(world, 7, 60.0F, 0.0F, 12.0F, false, true, Game::Units::SpawnType::Barracks);
  (void)add_world_unit(world, 3, 0.0F, 0.0F, 12.0F, true, false);

  const auto snapshot = Game::Systems::AI::AISnapshotBuilder::build(world, 3);

  EXPECT_TRUE(snapshot.visible_enemies.empty());
  ASSERT_EQ(snapshot.strategic_objectives.size(), 1U);
  EXPECT_EQ(snapshot.strategic_objectives.front().id, hidden_enemy_base->get_id());
}

TEST_F(AISystemTest, DefensiveAILeavesDefendingWhenOnlyDistantEnemyRemainsVisible) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 20.0F;
  snapshot.friendly_units = {
      make_unit(1, 30.0F, 20.0F),
      make_unit(2, 32.0F, 22.0F),
      make_unit(3, 34.0F, 24.0F),
   };
  snapshot.visible_enemies = {make_enemy(101, 120.0F, 120.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Defending;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  context.state = Game::Systems::AI::AIState::Defending;
  context.state_timer = 3.1F;
  context.decision_timer = 2.1F;
  context.last_local_threat_time = 10.0F;

  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.0F);

  EXPECT_EQ(context.state, Game::Systems::AI::AIState::Gathering);
}

TEST_F(AISystemTest, BuildingAttackMemoryKeepsAIInDefendingState) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 20.0F;
  auto barracks = make_unit(44, 32.0F, 22.0F);
  barracks.is_building = true;
  barracks.spawn_type = Game::Units::SpawnType::Barracks;
  snapshot.friendly_units = {
      make_unit(1, 30.0F, 20.0F),
      make_unit(2, 32.0F, 22.0F),
      make_unit(3, 34.0F, 24.0F),
      barracks,
  };
  snapshot.visible_enemies = {make_enemy(101, 120.0F, 120.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Defending;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);
  context.last_local_threat_time = 5.0F;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  context.state = Game::Systems::AI::AIState::Defending;
  context.state_timer = 3.1F;
  context.decision_timer = 2.1F;
  context.buildings_under_attack[44] = 19.5F;

  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.0F);

  EXPECT_EQ(context.state, Game::Systems::AI::AIState::Defending);
}

TEST_F(AISystemTest, StructuralBaseRequiresArmyAssemblyBeforeProactiveAttack) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 70.0F, 70.0F),
      make_unit(2, 72.0F, 70.0F),
      make_unit(3, 74.0F, 70.0F),
      make_unit(4, 76.0F, 70.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Gathering;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Aggressive);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  EXPECT_TRUE(context.anchor_is_structural);
  EXPECT_LT(context.assembled_unit_count, context.macro_targets.assembly_size);

  context.state = Game::Systems::AI::AIState::Gathering;
  context.state_timer = 3.1F;
  context.decision_timer = 2.1F;
  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.0F);
  EXPECT_EQ(context.state, Game::Systems::AI::AIState::Gathering);

  snapshot.game_time = 12.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 35.0F, 40.0F),
      make_unit(2, 36.0F, 40.0F),
      make_unit(3, 34.0F, 39.0F),
      make_unit(4, 35.0F, 42.0F),
  };

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  EXPECT_GE(context.assembled_unit_count, context.macro_targets.assembly_size);

  context.state = Game::Systems::AI::AIState::Gathering;
  context.state_timer = 3.1F;
  context.decision_timer = 2.1F;
  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.0F);
  EXPECT_EQ(context.state, Game::Systems::AI::AIState::Attacking);
}

TEST_F(AISystemTest, AIReasonerTransitionsToExpandingForOutpostPlan) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_building(60, 35.0F, 36.0F, Game::Units::SpawnType::Home),
      make_building(61, 34.0F, 44.0F, Game::Units::SpawnType::Home),
      make_building(62, 45.0F, 37.0F, Game::Units::SpawnType::Home),
      make_builder(10, 38.0F, 38.0F),
      make_unit(1, 36.0F, 40.0F),
      make_unit(2, 34.0F, 39.0F),
      make_unit(3, 35.0F, 42.0F),
  };
  auto enemy_base = make_enemy(301, 100.0F, 40.0F);
  enemy_base.is_building = true;
  enemy_base.spawn_type = Game::Units::SpawnType::Barracks;
  snapshot.strategic_objectives = {enemy_base};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Gathering;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Expansionist);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  EXPECT_TRUE(context.has_expansion_site);
  EXPECT_EQ(context.builder_count, 1);
  EXPECT_EQ(context.outpost_barracks_count, 0);
  EXPECT_EQ(context.outpost_home_count, 0);
  EXPECT_EQ(context.effective_reserve_units, 1);
  EXPECT_GE(context.assembled_unit_count, 3);
  context.state = Game::Systems::AI::AIState::Gathering;
  context.state_timer = 3.1F;
  context.decision_timer = 2.1F;

  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.0F);

  EXPECT_EQ(context.state, Game::Systems::AI::AIState::Expanding);
}

TEST_F(AISystemTest, AIReasonerKeepsStableReserveAssignmentsAcrossUpdates) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 40.5F, 40.0F),
      make_unit(2, 41.0F, 40.0F),
      make_unit(3, 52.0F, 40.0F),
      make_unit(4, 54.0F, 40.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Balanced);
  context.strategy_config.reserve_units = 2;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  ASSERT_EQ(context.reserve_unit_ids.size(), 2U);
  const auto first_reserve_ids = context.reserve_unit_ids;

  snapshot.game_time = 12.0F;
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  EXPECT_EQ(context.reserve_unit_ids, first_reserve_ids);
}

TEST_F(AISystemTest, AIReasonerKeepsStableHarassAssignmentsAcrossUpdates) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 40.5F, 40.0F),
      make_unit(2, 41.0F, 40.0F),
      make_unit(3, 58.0F, 42.0F),
      make_unit(4, 60.0F, 44.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Harasser);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  ASSERT_EQ(context.effective_harass_units, 2);
  ASSERT_EQ(context.harass_unit_ids.size(), 2U);
  const auto first_harass_ids = context.harass_unit_ids;

  snapshot.game_time = 12.0F;
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  EXPECT_EQ(context.harass_unit_ids, first_harass_ids);
}

TEST_F(AISystemTest, AIReasonerDoesNotCountHarassUnitsTowardProactiveAttackThreshold) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 15.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 35.0F, 40.0F),
      make_unit(2, 36.0F, 40.0F),
      make_unit(3, 37.0F, 40.0F),
      make_unit(4, 52.0F, 42.0F),
      make_unit(5, 56.0F, 44.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Gathering;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Aggressive);
  context.strategy_config.harass_units = 3;
  context.strategy_config.harassment_range = 60.0F;
  context.strategy_config.reactive_attack_size = 2;
  context.strategy_config.proactive_attack_size = 5;
  context.strategy_config.desired_assembly_size = 5;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  context.state = Game::Systems::AI::AIState::Gathering;
  context.state_timer = 3.1F;
  context.decision_timer = 2.1F;

  ASSERT_EQ(context.effective_harass_units, 3);

  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.0F);

  EXPECT_EQ(context.state, Game::Systems::AI::AIState::Gathering);
}

TEST_F(AISystemTest, AIReasonerUsesExplicitProactiveAttackThreshold) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 35.0F, 40.0F),
      make_unit(2, 36.0F, 40.0F),
      make_unit(3, 34.0F, 39.0F),
      make_unit(4, 35.0F, 42.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Gathering;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Balanced);
  context.strategy_config.desired_assembly_size = 4;
  context.strategy_config.proactive_attack_size = 6;
  context.strategy_config.reserve_units = 0;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  context.state = Game::Systems::AI::AIState::Gathering;
  context.state_timer = 3.1F;
  context.decision_timer = 2.1F;
  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.0F);
  EXPECT_EQ(context.state, Game::Systems::AI::AIState::Gathering);

  context.strategy_config.proactive_attack_size = 4;
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  EXPECT_EQ(context.assembled_unit_count, 4);
  EXPECT_EQ(context.macro_targets.assembly_size, 4);
  context.state = Game::Systems::AI::AIState::Gathering;
  context.state_timer = 3.1F;
  context.decision_timer = 2.1F;
  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.0F);
  EXPECT_EQ(context.state, Game::Systems::AI::AIState::Attacking);
}

TEST_F(AISystemTest, GatherBehaviorDoesNotPullReserveUnitsToRally) {
  Game::Systems::AI::GatherBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 6.0F;
  snapshot.friendly_units = {
      make_unit(1, 40.5F, 40.0F),
      make_unit(2, 10.0F, 10.0F),
      make_unit(3, 12.0F, 10.0F),
      make_unit(4, 14.0F, 10.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Gathering;
  context.has_base_anchor = true;
  context.anchor_is_structural = true;
  context.base_pos_x = 40.0F;
  context.base_pos_z = 40.0F;
  context.rally_x = 55.0F;
  context.rally_z = 40.0F;
  context.reserve_unit_ids = {1U};
  context.effective_reserve_units = 1;
  context.macro_targets.gather_spacing = 1.5F;
  context.macro_targets.assembly_radius = 10.0F;
  context.strategy_config.reserve_hold_radius = 6.0F;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.1F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::MoveUnits);
  EXPECT_TRUE(std::find(commands.front().units.begin(), commands.front().units.end(), 1U) ==
              commands.front().units.end());
}

TEST_F(AISystemTest, ExpandBehaviorMovesAttackForceToExpansionSite) {
  Game::Systems::AI::ExpandBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 6.0F;
  snapshot.friendly_units = {
      make_unit(1, 20.0F, 20.0F),
      make_unit(2, 22.0F, 20.0F),
      make_unit(3, 24.0F, 20.0F),
      make_unit(4, 26.0F, 20.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Expanding;
  context.has_expansion_site = true;
  context.expansion_site_x = 80.0F;
  context.expansion_site_z = 60.0F;
  context.reserve_unit_ids = {1U};
  context.effective_reserve_units = 1;
  context.harass_unit_ids = {2U};
  context.effective_harass_units = 1;

  ASSERT_TRUE(behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.1F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::MoveUnits);
  EXPECT_EQ(commands.front().units, (std::vector<Engine::Core::EntityID>{3U, 4U}));
}

TEST_F(AISystemTest, GatherBehaviorDoesNotPullHarassUnitsToRally) {
  Game::Systems::AI::GatherBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 6.0F;
  snapshot.friendly_units = {
      make_unit(1, 10.0F, 10.0F),
      make_unit(2, 12.0F, 10.0F),
      make_unit(3, 14.0F, 10.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Gathering;
  context.has_base_anchor = true;
  context.rally_x = 40.0F;
  context.rally_z = 40.0F;
  context.harass_unit_ids = {3U};
  context.effective_harass_units = 1;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.1F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::MoveUnits);
  EXPECT_EQ(commands.front().units, (std::vector<Engine::Core::EntityID>{1U, 2U}));
}

TEST_F(AISystemTest, AttackBehaviorLeavesReserveUnitsAtHome) {
  Game::Systems::AI::AttackBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 12.0F;
  snapshot.friendly_units = {
      make_unit(1, 40.5F, 40.0F),
      make_unit(2, 32.0F, 22.0F),
      make_unit(3, 34.0F, 24.0F),
      make_unit(4, 36.0F, 26.0F),
  };
  snapshot.visible_enemies = {make_enemy(101, 42.0F, 24.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Attacking;
  context.total_units = 4;
  context.reserve_unit_ids = {1U};
  context.effective_reserve_units = 1;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.6F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::AttackTarget);
  EXPECT_TRUE(std::find(commands.front().units.begin(), commands.front().units.end(), 1U) ==
              commands.front().units.end());
}

TEST_F(AISystemTest, AttackBehaviorLeavesHarassUnitsOutOfMainAttack) {
  Game::Systems::AI::AttackBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 12.0F;
  snapshot.friendly_units = {
      make_unit(1, 30.0F, 20.0F),
      make_unit(2, 32.0F, 22.0F),
      make_unit(3, 34.0F, 24.0F),
  };
  snapshot.visible_enemies = {make_enemy(101, 42.0F, 24.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Attacking;
  context.total_units = 3;
  context.harass_unit_ids = {3U};
  context.effective_harass_units = 1;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.6F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::AttackTarget);
  EXPECT_EQ(commands.front().units, (std::vector<Engine::Core::EntityID>{1U, 2U}));
}

TEST_F(AISystemTest, DefendBehaviorPrefersReserveUnitsForSmallThreats) {
  Game::Systems::AI::DefendBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 40.5F, 40.0F),
      make_unit(2, 41.0F, 40.0F),
      make_unit(3, 65.0F, 40.0F),
      make_unit(4, 67.0F, 40.0F),
  };
  snapshot.visible_enemies = {make_enemy(101, 52.0F, 40.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Defending;
  context.primary_barracks = 50;
  context.base_pos_x = 40.0F;
  context.base_pos_z = 40.0F;
  context.reserve_unit_ids = {1U, 2U};
  context.effective_reserve_units = 2;
  context.buildings_under_attack[50] = 9.5F;
  context.idle_units = 4;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.6F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::AttackTarget);
  EXPECT_EQ(commands.front().units, (std::vector<Engine::Core::EntityID>{1U, 2U}));
}

TEST_F(AISystemTest, HarassBehaviorUsesDedicatedRaidersTowardStrategicObjective) {
  Game::Systems::AI::HarassBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 5.0F;
  snapshot.friendly_units = {
      make_unit(1, 28.0F, 20.0F),
      make_unit(2, 45.0F, 20.0F),
      make_unit(3, 47.0F, 22.0F),
  };
  auto objective = make_enemy(201, 90.0F, 70.0F);
  objective.is_building = true;
  objective.spawn_type = Game::Units::SpawnType::Barracks;
  snapshot.strategic_objectives = {objective};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Gathering;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Harasser);
  context.harass_unit_ids = {2U, 3U};
  context.effective_harass_units = 2;

  ASSERT_TRUE(behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.1F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::MoveUnits);
  EXPECT_EQ(commands.front().units, (std::vector<Engine::Core::EntityID>{2U, 3U}));
}

TEST_F(AISystemTest, HarassBehaviorStopsWhenBaseIsUnderThreat) {
  Game::Systems::AI::HarassBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.friendly_units = {
      make_unit(1, 28.0F, 20.0F),
      make_unit(2, 45.0F, 20.0F),
  };
  snapshot.visible_enemies = {make_enemy(101, 42.0F, 20.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Gathering;
  context.barracks_under_threat = true;
  context.harass_unit_ids = {1U};
  context.effective_harass_units = 1;
  context.strategy_config.harassment_range = 50.0F;

  EXPECT_FALSE(behavior.should_execute(snapshot, context));
}

TEST_F(AISystemTest, AttackBehaviorUsesConfiguredFormationSpacing) {
  Game::Systems::AI::AttackBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 12.0F;
  snapshot.friendly_units = {
      make_unit(1, 30.0F, 20.0F),
      make_unit(2, 32.0F, 22.0F),
      make_unit(3, 34.0F, 24.0F),
  };
  snapshot.visible_enemies = {make_enemy(101, 80.0F, 80.0F)};

  Game::Systems::AI::AIContext compact_context;
  compact_context.player_id = 3;
  compact_context.state = Game::Systems::AI::AIState::Attacking;
  compact_context.total_units = 3;
  compact_context.strategy_config.attack_formation_spacing = 1.5F;

  std::vector<Game::Systems::AI::AICommand> compact_commands;
  behavior.execute(snapshot, compact_context, 1.6F, compact_commands);
  ASSERT_EQ(compact_commands.size(), 1U);

  Game::Systems::AI::AttackBehavior wide_behavior;
  Game::Systems::AI::AIContext wide_context = compact_context;
  wide_context.strategy_config.attack_formation_spacing = 3.5F;

  std::vector<Game::Systems::AI::AICommand> wide_commands;
  wide_behavior.execute(snapshot, wide_context, 1.6F, wide_commands);
  ASSERT_EQ(wide_commands.size(), 1U);

  const float compact_span =
      *std::max_element(compact_commands.front().move_target_x.begin(),
                        compact_commands.front().move_target_x.end()) -
      *std::min_element(compact_commands.front().move_target_x.begin(),
                        compact_commands.front().move_target_x.end());
  const float wide_span =
      *std::max_element(wide_commands.front().move_target_x.begin(),
                        wide_commands.front().move_target_x.end()) -
      *std::min_element(wide_commands.front().move_target_x.begin(),
                        wide_commands.front().move_target_x.end());

  EXPECT_LT(compact_span, wide_span);
}

TEST_F(AISystemTest, AttackBehaviorMarchesTowardStrategicObjectiveWithoutVision) {
  Game::Systems::AI::AttackBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.friendly_units = {
      make_unit(1, 30.0F, 20.0F),
      make_unit(2, 32.0F, 22.0F),
      make_unit(3, 34.0F, 24.0F),
  };
  snapshot.strategic_objectives = {
      make_enemy(101, 120.0F, 80.0F),
  };
  snapshot.strategic_objectives.front().is_building = true;
  snapshot.strategic_objectives.front().spawn_type = Game::Units::SpawnType::Barracks;

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

  float average_target_x = 0.0F;
  float average_target_z = 0.0F;
  for (std::size_t index = 0; index < commands.front().move_target_x.size(); ++index) {
    average_target_x += commands.front().move_target_x[index];
    average_target_z += commands.front().move_target_z[index];
  }
  average_target_x /= static_cast<float>(commands.front().move_target_x.size());
  average_target_z /= static_cast<float>(commands.front().move_target_z.size());

  EXPECT_FLOAT_EQ(average_target_x, 120.0F);
  EXPECT_FLOAT_EQ(average_target_z, 80.0F);
}

TEST_F(AISystemTest, AttackBehaviorUsesChaseForUnitTargets) {
  Game::Systems::AI::AttackBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 12.0F;
  snapshot.friendly_units = {
      make_unit(1, 30.0F, 20.0F),
      make_unit(2, 32.0F, 22.0F),
      make_unit(3, 34.0F, 24.0F),
  };
  snapshot.visible_enemies = {make_enemy(101, 42.0F, 24.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Attacking;
  context.total_units = 3;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.6F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::AttackTarget);
  EXPECT_TRUE(commands.front().should_chase);
}

} // namespace
