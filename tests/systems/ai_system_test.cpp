#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <string_view>

#include "game/core/component.h"
#include "game/core/ownership_constants.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_base_manager.h"
#include "game/systems/ai_system/ai_behavior_registry.h"
#include "game/systems/ai_system/ai_command_applier.h"
#include "game/systems/ai_system/ai_command_filter.h"
#include "game/systems/ai_system/ai_executor.h"
#include "game/systems/ai_system/ai_reasoner.h"
#include "game/systems/ai_system/ai_snapshot_builder.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/ai_system/ai_utils.h"
#include "game/systems/ai_system/behaviors/assault_behavior.h"
#include "game/systems/ai_system/behaviors/attack_behavior.h"
#include "game/systems/ai_system/behaviors/builder_behavior.h"
#include "game/systems/ai_system/behaviors/defend_behavior.h"
#include "game/systems/ai_system/behaviors/economy_behavior.h"
#include "game/systems/ai_system/behaviors/expand_behavior.h"
#include "game/systems/ai_system/behaviors/gather_behavior.h"
#include "game/systems/ai_system/behaviors/harass_behavior.h"
#include "game/systems/ai_system/behaviors/local_engagement_behavior.h"
#include "game/systems/ai_system/behaviors/production_behavior.h"
#include "game/systems/ai_system/behaviors/squad_discipline_behavior.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"

namespace {

class AISystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
  }

  void TearDown() override {
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
  }

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

  static auto make_enemy_building(Engine::Core::EntityID id,
                                  float x,
                                  float z) -> Game::Systems::AI::ContactSnapshot {
    auto building = make_enemy(id, x, z);
    building.is_building = true;
    building.spawn_type = Game::Units::SpawnType::Barracks;
    return building;
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

  static auto
  add_world_unit(Engine::Core::World& world,
                 int owner_id,
                 float x,
                 float z,
                 float vision_range,
                 bool ai_controlled,
                 bool is_building = false,
                 Game::Units::SpawnType spawn_type = Game::Units::SpawnType::Spearman)
      -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    (void)entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>(
        100, 100, 1.0F, vision_range);
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

  static void register_sepulcher_nation(int owner_id = 3) {
    Game::Systems::Nation nation;
    nation.id = Game::Systems::NationID::IronSepulcher;
    nation.display_name = "The Iron Sepulcher";
    nation.playable = false;
    nation.has_economy = false;
    nation.ai_profile = "sepulcher_defense";
    nation.selectable_in_skirmish = false;
    Game::Systems::NationRegistry::instance().register_nation(std::move(nation));
    Game::Systems::NationRegistry::instance().set_player_nation(
        owner_id, Game::Systems::NationID::IronSepulcher);
  }

  static void register_economy_nation(int owner_id = 3) {
    Game::Systems::Nation nation;
    nation.id = Game::Systems::NationID::RomanRepublic;
    nation.display_name = "Rome";
    nation.playable = true;
    nation.has_economy = true;
    nation.ai_profile = "standard";

    Game::Systems::TroopType archer;
    archer.unit_type = Game::Units::TroopType::Archer;
    archer.display_name = "Archer";
    archer.is_melee = false;
    archer.priority = 1;
    nation.available_troops.push_back(archer);

    Game::Systems::TroopType spearman;
    spearman.unit_type = Game::Units::TroopType::Spearman;
    spearman.display_name = "Spearman";
    spearman.is_melee = true;
    spearman.priority = 1;
    nation.available_troops.push_back(spearman);

    Game::Systems::NationRegistry::instance().register_nation(std::move(nation));
    Game::Systems::NationRegistry::instance().set_player_nation(
        owner_id, Game::Systems::NationID::RomanRepublic);
  }

  static auto make_barracks(Engine::Core::EntityID id,
                            float x,
                            float z,
                            int queue_size = 0) -> Game::Systems::AI::EntitySnapshot {
    auto barracks = make_building(id, x, z, Game::Units::SpawnType::Barracks);
    barracks.production.has_component = true;
    barracks.production.max_units = 50;
    barracks.production.produced_count = 0;
    barracks.production.queue_size = queue_size;

    barracks.production.manpower_available = 500;
    return barracks;
  }

  static void initialize_world_props(const std::vector<Game::Map::WorldProp>& props) {
    Game::Map::TerrainService::instance().restore_from_serialized(
        8,
        8,
        1.0F,
        std::vector<float>(64, 0.0F),
        std::vector<Game::Map::TerrainType>(64, Game::Map::TerrainType::Flat),
        {},
        {},
        {},
        {},
        props,
        props);
  }
};

TEST_F(AISystemTest, ReinitializePicksUpOwnersRegisteredAfterConstruction) {
  Game::Systems::AISystem ai_system(Game::Systems::AISystem::Services{
      .owners = Game::Systems::OwnerRegistry::instance(),
      .nations = Game::Systems::NationRegistry::instance()});

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

  Game::Systems::AISystem ai_system(Game::Systems::AISystem::Services{
      .owners = Game::Systems::OwnerRegistry::instance(),
      .nations = Game::Systems::NationRegistry::instance()});
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

TEST_F(AISystemTest, SnapshotExcludesLocalGuardAndPatrolAssignments) {
  Engine::Core::World world;

  auto* strategic = add_world_unit(world, 3, 0.0F, 0.0F, 20.0F, true);
  auto* guard = add_world_unit(world, 3, 10.0F, 0.0F, 20.0F, true);
  auto* patrol = add_world_unit(world, 3, 20.0F, 0.0F, 20.0F, true);
  ASSERT_NE(strategic, nullptr);
  ASSERT_NE(guard, nullptr);
  ASSERT_NE(patrol, nullptr);

  auto* guard_mode = guard->add_component<Engine::Core::GuardModeComponent>();
  guard_mode->active = true;
  guard_mode->has_guard_target = true;
  guard_mode->guard_position_x = 10.0F;
  guard_mode->guard_position_z = 0.0F;

  auto* patrol_mode = patrol->add_component<Engine::Core::PatrolComponent>();
  patrol_mode->patrolling = true;
  patrol_mode->waypoints = {{20.0F, 0.0F}, {25.0F, 0.0F}};

  const auto snapshot = Game::Systems::AI::AISnapshotBuilder::build(world, 3);

  ASSERT_EQ(snapshot.friendly_units.size(), 1U);
  EXPECT_EQ(snapshot.friendly_units.front().id, strategic->get_id());
}

TEST_F(AISystemTest, PersonalityChangesArmyCommitmentAndOrganization) {
  auto aggressive = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Balanced);
  auto defensive = aggressive;

  Game::Systems::AI::AIStrategyFactory::apply_personality(
      aggressive, 0.85F, 0.25F, 0.25F);
  Game::Systems::AI::AIStrategyFactory::apply_personality(
      defensive, 0.25F, 0.85F, 0.25F);

  EXPECT_LT(aggressive.reactive_attack_size, defensive.reactive_attack_size);
  EXPECT_LT(aggressive.proactive_attack_size, defensive.proactive_attack_size);
  EXPECT_LT(aggressive.reserve_units, defensive.reserve_units);
  EXPECT_LT(aggressive.attack_formation_spacing, defensive.attack_formation_spacing);
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

TEST_F(
    AISystemTest,
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

  EXPECT_NEAR(average_target_x, 40.0F, 0.75F);
  EXPECT_NEAR(average_target_z, 90.0F, 0.75F);
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

  const auto claimed =
      Game::Systems::AI::claim_units({7},
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

  const auto alive =
      Game::Systems::AI::cleanup_dead_units(snapshot, context, snapshot.game_time);

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

TEST_F(AISystemTest, CommandFilterKeepsNonDuplicateUnitsInMultiUnitMoveCommands) {
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

TEST_F(AISystemTest, BuilderBehaviorHarvestsMissingStoneBeforeConstruction) {
  Game::Systems::AI::BuilderBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 5.0F;
  snapshot.friendly_units = {make_builder(11, 18.0F, 12.0F)};
  snapshot.has_resource_snapshot = true;
  snapshot.resources.set(Game::Systems::ResourceType::Wood, 100);
  snapshot.resources.set(Game::Systems::ResourceType::Stone, 10);
  snapshot.resource_nodes.push_back(
      {77, Game::Map::WorldProp::Type::Boulder, 22.0F, 14.0F, false});

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.builder_count = 1;
  context.home_count = 2;
  context.defense_tower_count = 1;
  context.barracks_count = 0;
  context.base_pos_x = 20.0F;
  context.base_pos_z = 15.0F;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 3.1F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type,
            Game::Systems::AI::AICommandType::StartBuilderHarvest);
  EXPECT_STREQ(commands.front().construction_type, "collect_stone");
  EXPECT_EQ(commands.front().resource_target_id, 77U);
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
  context.marketplace_count = 1;
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

  for (const auto& command : commands) {
    if (command.type != Game::Systems::AI::AICommandType::StartBuilderConstruction) {
      continue;
    }
    EXPECT_FALSE(command.construction_site_x == 80.0F &&
                 command.construction_site_z == 60.0F)
        << "a second outpost was ordered on top of the one already being raised";
  }
}

TEST_F(AISystemTest, SnapshotBuilderFiltersEnemiesByOwnedVision) {
  Engine::Core::World world;
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "AI");
  owners.register_owner_with_id(7, Game::Systems::OwnerType::Player, "Enemy");

  auto* visible_enemy = add_world_unit(world, 7, 8.0F, 0.0F, 12.0F, false, false);
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

  auto* visible_enemy = add_world_unit(world, 7, 15.0F, 0.0F, 12.0F, false, false);
  (void)add_world_unit(
      world, 3, 0.0F, 0.0F, 5.0F, true, true, Game::Units::SpawnType::Barracks);

  const auto snapshot = Game::Systems::AI::AISnapshotBuilder::build(world, 3);

  ASSERT_EQ(snapshot.visible_enemies.size(), 1U);
  EXPECT_EQ(snapshot.visible_enemies.front().id, visible_enemy->get_id());
}

TEST_F(AISystemTest, SepulcherStrategyParsesAliasesAndDisablesEconomyTargets) {
  EXPECT_EQ(Game::Systems::AI::AIStrategyFactory::parse_strategy("sepulcher_defense"),
            Game::Systems::AI::AIStrategy::SepulcherDefense);
  EXPECT_EQ(Game::Systems::AI::AIStrategyFactory::parse_strategy("undead_defense"),
            Game::Systems::AI::AIStrategy::SepulcherDefense);

  const auto config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::SepulcherDefense);
  EXPECT_EQ(config.target_builder_count, 0);
  EXPECT_EQ(config.base_home_target, 0);
  EXPECT_EQ(config.desired_barracks_count, 0);
  EXPECT_EQ(config.desired_defense_tower_count, 0);
  EXPECT_EQ(config.desired_outpost_barracks_count, 0);
  EXPECT_EQ(config.harass_units, 0);
  EXPECT_GT(config.reserve_units, 0);
}

TEST_F(AISystemTest, SnapshotBuilderAddsShrineAndRuinDefenseAnchorsForSepulcherAI) {
  register_sepulcher_nation();
  initialize_world_props({{1, Game::Map::WorldProp::Type::MagicShrine, 12.0F, 14.0F},
                          {2, Game::Map::WorldProp::Type::Ruins, 18.0F, 20.0F},
                          {3, Game::Map::WorldProp::Type::Tent, 2.0F, 3.0F}});

  Engine::Core::World world;
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "Sepulcher");
  owners.register_owner_with_id(7, Game::Systems::OwnerType::Player, "Enemy");
  (void)add_world_unit(world, 3, 10.0F, 10.0F, 12.0F, true, false);

  const auto snapshot = Game::Systems::AI::AISnapshotBuilder::build(world, 3);

  ASSERT_EQ(snapshot.defense_anchors.size(), 2U);
  EXPECT_FLOAT_EQ(snapshot.defense_anchors[0].pos_x, 12.0F);
  EXPECT_FLOAT_EQ(snapshot.defense_anchors[1].pos_x, 18.0F);
}

TEST_F(AISystemTest, SepulcherReasonerPrefersStructuralShrineAnchorWithoutBarracks) {
  register_sepulcher_nation();

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.friendly_units = {
      make_unit(1, 13.0F, 11.0F),
      make_unit(2, 12.0F, 9.0F),
      make_unit(3, 70.0F, 70.0F),
  };
  snapshot.defense_anchors = {
      make_enemy(901, 12.0F, 10.0F),
      make_enemy(902, 80.0F, 80.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;

  Game::Systems::AI::AISnapshotBuilder::attach_nation(
      snapshot, context.player_id, Game::Systems::NationRegistry::instance());
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  EXPECT_TRUE(context.has_base_anchor);
  EXPECT_TRUE(context.anchor_is_structural);
  EXPECT_EQ(context.primary_barracks, 0U);
  EXPECT_FLOAT_EQ(context.base_pos_x, 12.0F);
  EXPECT_FLOAT_EQ(context.base_pos_z, 10.0F);
  EXPECT_FLOAT_EQ(context.rally_x, 12.0F);
  EXPECT_FLOAT_EQ(context.rally_z, 10.0F);
}

TEST_F(AISystemTest, NoEconomyBehaviorsDoNotRequestProductionBuildersOrExpansion) {
  register_sepulcher_nation();

  Game::Systems::AI::ProductionBehavior production_behavior;
  Game::Systems::AI::BuilderBehavior builder_behavior;
  Game::Systems::AI::ExpandBehavior expand_behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_builder(11, 18.0F, 12.0F),
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 42.0F, 40.0F),
      make_unit(2, 43.0F, 41.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.nation = Game::Systems::NationRegistry::instance().get_nation_for_player(
      context.player_id);
  context.state = Game::Systems::AI::AIState::Expanding;
  context.builder_count = 1;
  context.total_units = 3;
  context.max_troops_per_player = 500;
  context.has_expansion_site = true;

  EXPECT_FALSE(production_behavior.should_execute(snapshot, context));
  EXPECT_FALSE(builder_behavior.should_execute(snapshot, context));
  EXPECT_FALSE(expand_behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  production_behavior.execute(snapshot, context, 2.0F, commands);
  builder_behavior.execute(snapshot, context, 4.0F, commands);
  expand_behavior.execute(snapshot, context, 2.0F, commands);
  EXPECT_TRUE(commands.empty());
}

TEST_F(AISystemTest, SepulcherStateMachineStaysLocalAndReturnsToGathering) {
  register_sepulcher_nation();

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 20.0F;
  snapshot.friendly_units = {
      make_unit(1, 12.0F, 10.0F),
      make_unit(2, 13.0F, 9.0F),
      make_unit(3, 14.0F, 10.0F),
  };
  snapshot.visible_enemies = {make_enemy(101, 60.0F, 60.0F)};
  snapshot.defense_anchors = {make_enemy(901, 12.0F, 10.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::SepulcherDefense);
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  context.state = Game::Systems::AI::AIState::Gathering;
  context.state_timer = 3.1F;
  context.decision_timer = 2.1F;

  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.0F);
  EXPECT_EQ(context.state, Game::Systems::AI::AIState::Gathering);

  context.state = Game::Systems::AI::AIState::Defending;
  context.state_timer = 3.1F;
  context.decision_timer = 2.1F;
  context.nearby_threat_count = 0;
  context.barracks_under_threat = false;
  context.buildings_under_attack.clear();
  context.last_local_threat_time = 5.0F;

  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.0F);
  EXPECT_EQ(context.state, Game::Systems::AI::AIState::Gathering);
}

TEST_F(AISystemTest, SnapshotBuilderKeepsHiddenStrategicObjectives) {
  Engine::Core::World world;
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "AI");
  owners.register_owner_with_id(7, Game::Systems::OwnerType::Player, "Enemy");

  auto* hidden_enemy_base = add_world_unit(
      world, 7, 60.0F, 0.0F, 12.0F, false, true, Game::Units::SpawnType::Barracks);
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
  EXPECT_TRUE(std::find(commands.front().units.begin(),
                        commands.front().units.end(),
                        1U) == commands.front().units.end());
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
  EXPECT_TRUE(std::find(commands.front().units.begin(),
                        commands.front().units.end(),
                        1U) == commands.front().units.end());
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

TEST_F(AISystemTest, AttackBehaviorPicksSoldierOverCloserBuilding) {
  Game::Systems::AI::AttackBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.friendly_units = {
      make_unit(1, 30.0F, 20.0F),
      make_unit(2, 32.0F, 22.0F),
  };
  auto damaged_building = make_enemy_building(101, 33.0F, 23.0F);
  damaged_building.health = 10;
  snapshot.visible_enemies = {
      damaged_building,
      make_enemy(102, 38.0F, 28.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Attacking;
  context.total_units = 2;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.6F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::AttackTarget);
  EXPECT_EQ(commands.front().target_id, 102U);
  EXPECT_TRUE(commands.front().should_chase);
}

TEST_F(AISystemTest, AssaultBehaviorPicksSoldierOverCloserBuilding) {
  Game::Systems::AI::AssaultBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  auto assault_unit = make_unit(1, 30.0F, 20.0F);
  assault_unit.is_assault = true;
  snapshot.friendly_units = {assault_unit};
  snapshot.visible_enemies = {
      make_enemy_building(101, 31.0F, 21.0F),
      make_enemy(102, 45.0F, 35.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.assault_unit_ids = {1U};
  context.assault_unit_count = 1;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.1F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::MoveUnits);
  ASSERT_EQ(commands.front().move_target_x.size(), 1U);
  EXPECT_NEAR(commands.front().move_target_x.front(), 45.0F, 8.0F);
  EXPECT_NEAR(commands.front().move_target_z.front(), 35.0F, 8.0F);
}

TEST_F(AISystemTest, AssaultBehaviorImmediatelyBreachesHostileGateAcrossAttackLane) {
  Game::Systems::AI::AssaultBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  auto assault_unit = make_unit(1, 0.0F, 0.0F);
  assault_unit.is_assault = true;
  snapshot.friendly_units = {assault_unit};

  auto gate = make_enemy_building(101, 10.0F, 0.0F);
  gate.spawn_type = Game::Units::SpawnType::WallGate;
  snapshot.visible_enemies = {gate, make_enemy(102, 30.0F, 0.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.assault_unit_ids = {1U};
  context.assault_unit_count = 1;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.1F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::AttackTarget);
  EXPECT_EQ(commands.front().target_id, 101U);
  EXPECT_TRUE(commands.front().should_chase);
}

TEST_F(AISystemTest, AssaultBehaviorDoesNotReplaceActiveAuthoredMarch) {
  Game::Systems::AI::AssaultBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  auto assault_unit = make_unit(1, 0.0F, 0.0F);
  assault_unit.is_assault = true;
  assault_unit.has_march_target = true;
  assault_unit.march_target_x = 40.0F;
  assault_unit.march_target_z = 0.0F;
  assault_unit.movement.has_target = true;
  snapshot.friendly_units = {assault_unit};
  snapshot.strategic_objectives = {make_enemy_building(101, 20.0F, 20.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.assault_unit_ids = {1U};
  context.assault_unit_count = 1;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 1.1F, commands);

  EXPECT_TRUE(commands.empty());
}

TEST_F(AISystemTest, HarassBehaviorPicksSoldierOverCloserBuilding) {
  Game::Systems::AI::HarassBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.game_time = 5.0F;
  snapshot.friendly_units = {make_unit(1, 45.0F, 20.0F)};
  snapshot.visible_enemies = {
      make_enemy_building(101, 47.0F, 22.0F),
      make_enemy(102, 55.0F, 30.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.state = Game::Systems::AI::AIState::Gathering;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Harasser);
  context.strategy_config.harassment_range = 50.0F;
  context.harass_unit_ids = {1U};
  context.effective_harass_units = 1;

  ASSERT_TRUE(behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.1F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::AttackTarget);
  EXPECT_EQ(commands.front().target_id, 102U);
}

TEST_F(AISystemTest, StandingNextToEnemyBuildingDoesNotCountAsEngaged) {
  auto unit = make_unit(7, 10.0F, 10.0F);

  EXPECT_FALSE(Game::Systems::AI::is_entity_engaged(
      unit, {make_enemy_building(9, 11.0F, 10.0F)}));
  EXPECT_TRUE(
      Game::Systems::AI::is_entity_engaged(unit, {make_enemy(9, 11.0F, 10.0F)}));
}

TEST_F(AISystemTest, HarassBehaviorStopsWhenBaseIsUnderThreat) {
  Game::Systems::AI::HarassBehavior const behavior;

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
  const float wide_span = *std::max_element(wide_commands.front().move_target_x.begin(),
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

  EXPECT_NEAR(average_target_x, 120.0F, 0.75F);
  EXPECT_NEAR(average_target_z, 80.0F, 0.75F);
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

TEST_F(AISystemTest, BaseManagerClustersBuildingsIntoDistinctBases) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_building(60, 36.0F, 42.0F, Game::Units::SpawnType::Home),
      make_building(61, 44.0F, 38.0F, Game::Units::SpawnType::Home),
      make_barracks(70, 100.0F, 40.0F),
      make_building(71, 104.0F, 42.0F, Game::Units::SpawnType::Home),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  ASSERT_EQ(context.bases.size(), 2U);
  EXPECT_NE(context.bases[0].id, context.bases[1].id);

  const auto* main = Game::Systems::AI::AIBaseManager::main_base(context);
  ASSERT_NE(main, nullptr);
  EXPECT_EQ(main->role, Game::Systems::AI::BaseRole::Main);
  EXPECT_EQ(main->primary_barracks, 50U);
  EXPECT_EQ(main->barracks_count, 1);
  EXPECT_EQ(main->home_count, 2);

  const auto* second =
      Game::Systems::AI::AIBaseManager::base_for_position(context, 100.0F, 40.0F);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(second->id, main->id);
  EXPECT_EQ(second->primary_barracks, 70U);
  EXPECT_NE(second->role, Game::Systems::AI::BaseRole::Main);
}

TEST_F(AISystemTest, BaseManagerSeparatesProductionAndDefensiveResponsibilities) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_building(60, 36.0F, 42.0F, Game::Units::SpawnType::Home),
      make_barracks(70, 100.0F, 40.0F),
      make_building(80, 40.0F, 100.0F, Game::Units::SpawnType::DefenseTower),
      make_building(81, 44.0F, 104.0F, Game::Units::SpawnType::Home),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  ASSERT_EQ(context.bases.size(), 3U);

  const auto* production =
      Game::Systems::AI::AIBaseManager::base_for_position(context, 100.0F, 40.0F);
  const auto* defensive =
      Game::Systems::AI::AIBaseManager::base_for_position(context, 40.0F, 100.0F);
  ASSERT_NE(production, nullptr);
  ASSERT_NE(defensive, nullptr);

  EXPECT_GT(production->barracks_count, 0);
  EXPECT_EQ(defensive->barracks_count, 0);
  EXPECT_EQ(defensive->role, Game::Systems::AI::BaseRole::Defensive);
  EXPECT_NE(production->role, Game::Systems::AI::BaseRole::Defensive);
}

TEST_F(AISystemTest, BaseManagerKeepsBaseIdentityAcrossUpdates) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_barracks(70, 100.0F, 40.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  ASSERT_EQ(context.bases.size(), 2U);
  const int main_id = context.main_base_id;
  const auto* second_before =
      Game::Systems::AI::AIBaseManager::base_for_position(context, 100.0F, 40.0F);
  ASSERT_NE(second_before, nullptr);
  const int second_id = second_before->id;

  snapshot.game_time = 20.0F;
  snapshot.friendly_units.push_back(
      make_building(71, 103.0F, 41.0F, Game::Units::SpawnType::Home));

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  ASSERT_EQ(context.bases.size(), 2U);
  EXPECT_EQ(context.main_base_id, main_id);
  const auto* second_after =
      Game::Systems::AI::AIBaseManager::base_for_position(context, 101.0F, 40.0F);
  ASSERT_NE(second_after, nullptr);
  EXPECT_EQ(second_after->id, second_id);
}

TEST_F(AISystemTest, ProductionContinuesFromSurvivingBaseWhenMainBaseIsDestroyed) {
  register_economy_nation();
  Game::Systems::AI::ProductionBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_building(60, 36.0F, 42.0F, Game::Units::SpawnType::Home),
      make_barracks(70, 100.0F, 40.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.max_troops_per_player = 500;

  Game::Systems::AI::AISnapshotBuilder::attach_nation(
      snapshot, context.player_id, Game::Systems::NationRegistry::instance());
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  const auto* main = Game::Systems::AI::AIBaseManager::main_base(context);
  ASSERT_NE(main, nullptr);
  ASSERT_EQ(main->primary_barracks, 50U);
  const int destroyed_base_id = main->id;

  Game::Systems::AI::AISnapshot after_loss;
  after_loss.player_id = 3;
  after_loss.game_time = 25.0F;
  after_loss.friendly_units = {make_barracks(70, 100.0F, 40.0F)};

  Game::Systems::AI::AISnapshotBuilder::attach_nation(
      after_loss, context.player_id, Game::Systems::NationRegistry::instance());
  Game::Systems::AI::AIReasoner::update_context(after_loss, context);

  ASSERT_EQ(context.bases.size(), 1U);
  EXPECT_NE(context.main_base_id, destroyed_base_id);
  EXPECT_EQ(context.primary_barracks, 70U);
  EXPECT_FLOAT_EQ(context.base_pos_x, 100.0F);

  std::vector<Game::Systems::AI::AICommand> commands;
  ASSERT_TRUE(behavior.should_execute(after_loss, context));
  behavior.execute(after_loss, context, 5.0F, commands);

  const auto production_commands = std::count_if(
      commands.begin(), commands.end(), [](const Game::Systems::AI::AICommand& cmd) {
        return cmd.type == Game::Systems::AI::AICommandType::StartProduction;
      });
  EXPECT_EQ(production_commands, 1);
  for (const auto& cmd : commands) {
    if (cmd.type == Game::Systems::AI::AICommandType::StartProduction) {
      EXPECT_EQ(cmd.building_id, 70U);
    }
  }
}

TEST_F(AISystemTest, BaseManagerReleasesAssignmentsOwnedByADestroyedBase) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_barracks(70, 100.0F, 40.0F),
      make_unit(1, 101.0F, 41.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  const auto* outpost =
      Game::Systems::AI::AIBaseManager::base_for_position(context, 100.0F, 40.0F);
  ASSERT_NE(outpost, nullptr);

  const auto claimed =
      Game::Systems::AI::claim_units({1},
                                     Game::Systems::AI::BehaviorPriority::High,
                                     "defending",
                                     context,
                                     snapshot.game_time,
                                     2.0F,
                                     outpost->id);
  ASSERT_EQ(claimed.size(), 1U);
  ASSERT_TRUE(context.assigned_units.contains(1));

  Game::Systems::AI::AISnapshot after_loss;
  after_loss.player_id = 3;
  after_loss.game_time = 12.0F;
  after_loss.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_unit(1, 101.0F, 41.0F),
  };

  Game::Systems::AI::AISnapshotBuilder::attach_nation(
      after_loss, context.player_id, Game::Systems::NationRegistry::instance());
  Game::Systems::AI::AIReasoner::update_context(after_loss, context);

  EXPECT_EQ(context.reassigned_units_last_update, 1);
  EXPECT_FALSE(context.assigned_units.contains(1));
}

TEST_F(AISystemTest, BaseManagerMigratesStrategicCenterToStrongerForwardBase) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_barracks(70, 100.0F, 40.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  const auto* main = Game::Systems::AI::AIBaseManager::main_base(context);
  ASSERT_NE(main, nullptr);
  ASSERT_EQ(main->primary_barracks, 50U);

  snapshot.game_time = 40.0F;
  snapshot.friendly_units.push_back(make_barracks(71, 104.0F, 42.0F));
  snapshot.friendly_units.push_back(
      make_building(72, 96.0F, 44.0F, Game::Units::SpawnType::Home));
  snapshot.friendly_units.push_back(
      make_building(73, 103.0F, 36.0F, Game::Units::SpawnType::Home));

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  const auto* migrated = Game::Systems::AI::AIBaseManager::main_base(context);
  ASSERT_NE(migrated, nullptr);
  EXPECT_EQ(migrated->primary_barracks, 70U);
  EXPECT_EQ(context.primary_barracks, 70U);
  EXPECT_FLOAT_EQ(context.base_pos_x, 100.0F);
}

TEST_F(AISystemTest, IsolatedBaseKeepsOwnRallyAndReportsItsOwnThreat) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_building(60, 36.0F, 42.0F, Game::Units::SpawnType::Home),
      make_barracks(70, 140.0F, 140.0F),
  };
  snapshot.visible_enemies = {make_enemy(200, 142.0F, 141.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  const auto* isolated =
      Game::Systems::AI::AIBaseManager::base_for_position(context, 140.0F, 140.0F);
  const auto* main = Game::Systems::AI::AIBaseManager::main_base(context);
  ASSERT_NE(isolated, nullptr);
  ASSERT_NE(main, nullptr);

  EXPECT_TRUE(isolated->under_threat);
  EXPECT_FALSE(main->under_threat);
  EXPECT_FLOAT_EQ(isolated->rally_x, 135.0F);
  EXPECT_FLOAT_EQ(isolated->rally_z, 140.0F);
  EXPECT_FLOAT_EQ(main->rally_x, 35.0F);
  EXPECT_FLOAT_EQ(isolated->last_threat_time, 10.0F);
}

TEST_F(AISystemTest, DefendBehaviorDefendsThreatenedSecondaryBase) {
  Game::Systems::AI::DefendBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_barracks(70, 140.0F, 140.0F),
      make_unit(1, 60.0F, 60.0F),
      make_unit(2, 62.0F, 62.0F),
  };
  snapshot.visible_enemies = {make_enemy(200, 145.0F, 142.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  context.state = Game::Systems::AI::AIState::Defending;

  const auto* threatened =
      Game::Systems::AI::AIBaseManager::base_for_position(context, 140.0F, 140.0F);
  ASSERT_NE(threatened, nullptr);
  ASSERT_TRUE(threatened->under_threat);

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);

  ASSERT_FALSE(commands.empty());
  const auto& response = commands.front();
  EXPECT_EQ(response.type, Game::Systems::AI::AICommandType::AttackTarget);
  EXPECT_EQ(response.target_id, 200U);
  ASSERT_FALSE(response.units.empty());

  for (const auto unit_id : response.units) {
    ASSERT_TRUE(context.assigned_units.contains(unit_id));
    EXPECT_EQ(context.assigned_units[unit_id].base_id, threatened->id);
  }
}

TEST_F(AISystemTest, ProductionBehaviorSetsRallyPointPerBase) {
  register_economy_nation();
  Game::Systems::AI::ProductionBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_barracks(70, 140.0F, 140.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.max_troops_per_player = 500;

  Game::Systems::AI::AISnapshotBuilder::attach_nation(
      snapshot, context.player_id, Game::Systems::NationRegistry::instance());
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 5.0F, commands);

  std::unordered_map<Engine::Core::EntityID, std::pair<float, float>> rally_by_building;
  for (const auto& cmd : commands) {
    if (cmd.type == Game::Systems::AI::AICommandType::SetRallyPoint) {
      rally_by_building[cmd.building_id] = {cmd.rally_x, cmd.rally_z};
    }
  }

  ASSERT_EQ(rally_by_building.size(), 2U);
  EXPECT_FLOAT_EQ(rally_by_building[50].first, 35.0F);
  EXPECT_FLOAT_EQ(rally_by_building[50].second, 40.0F);
  EXPECT_FLOAT_EQ(rally_by_building[70].first, 135.0F);
  EXPECT_FLOAT_EQ(rally_by_building[70].second, 140.0F);
}

TEST_F(AISystemTest, ProductionBehaviorRespectsPerBaseQueueLimit) {
  register_economy_nation();
  Game::Systems::AI::ProductionBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F, 4),
      make_barracks(51, 44.0F, 44.0F, 1),
      make_barracks(70, 140.0F, 140.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.max_troops_per_player = 500;

  Game::Systems::AI::AISnapshotBuilder::attach_nation(
      snapshot, context.player_id, Game::Systems::NationRegistry::instance());
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 5.0F, commands);

  std::vector<Engine::Core::EntityID> produced_at;
  for (const auto& cmd : commands) {
    if (cmd.type == Game::Systems::AI::AICommandType::StartProduction) {
      produced_at.push_back(cmd.building_id);
    }
  }

  EXPECT_EQ(std::count(produced_at.begin(), produced_at.end(), 70U), 1);
  EXPECT_LE(std::count(produced_at.begin(), produced_at.end(), 50U) +
                std::count(produced_at.begin(), produced_at.end(), 51U),
            1);
}

TEST_F(AISystemTest, BuilderBehaviorFortifiesThreatenedSecondaryBase) {
  Game::Systems::AI::BuilderBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_building(60, 36.0F, 42.0F, Game::Units::SpawnType::Home),
      make_building(61, 44.0F, 38.0F, Game::Units::SpawnType::Home),
      make_barracks(70, 140.0F, 140.0F),
      make_builder(11, 120.0F, 120.0F),
  };
  snapshot.visible_enemies = {make_enemy(200, 145.0F, 142.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  ASSERT_EQ(context.builder_count, 1);

  ASSERT_TRUE(behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 4.0F, commands);

  ASSERT_EQ(commands.size(), 1U);
  const auto& command = commands.front();
  EXPECT_EQ(command.type, Game::Systems::AI::AICommandType::StartBuilderConstruction);
  ASSERT_NE(command.construction_type, nullptr);
  EXPECT_STREQ(command.construction_type, "defense_tower");

  const float dx = command.construction_site_x - 140.0F;
  const float dz = command.construction_site_z - 140.0F;
  EXPECT_LE(std::sqrt((dx * dx) + (dz * dz)), 20.0F)
      << "the tower must be raised beside the threatened outpost";
  EXPECT_FALSE(command.construction_site_x == 140.0F &&
               command.construction_site_z == 140.0F)
      << "the tower must not be sited on top of the outpost barracks, which would "
         "have the order refused every tick";
}

TEST_F(AISystemTest, BuilderBehaviorDividesOneWorkSquadIntoSeveralParties) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;

  auto builders = make_builder(11, 40.0F, 40.0F);
  builders.squad_strength = 12;
  builders.squad_establishment = 12;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_building(60, 36.0F, 42.0F, Game::Units::SpawnType::Home),
      make_building(61, 44.0F, 38.0F, Game::Units::SpawnType::Home),
      builders,
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  Game::Systems::AI::BuilderBehavior behavior;
  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 4.0F, commands);

  const auto divide = std::find_if(
      commands.begin(), commands.end(), [](const Game::Systems::AI::AICommand& cmd) {
        return cmd.type == Game::Systems::AI::AICommandType::DivideSquads;
      });
  ASSERT_NE(divide, commands.end())
      << "one twelve-strong work squad cannot cover a town's jobs at once";
  ASSERT_EQ(divide->units.size(), 1U);
  EXPECT_EQ(divide->units.front(), 11U);
}

TEST_F(AISystemTest, SquadDisciplineFoldsTwoDecimatedSquadsBackTogether) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;

  auto left = make_unit(1, 40.0F, 40.0F);
  left.squad_strength = 6;
  left.squad_establishment = 24;
  auto right = make_unit(2, 42.0F, 40.0F);
  right.squad_strength = 7;
  right.squad_establishment = 24;
  auto whole = make_unit(3, 44.0F, 40.0F);
  whole.squad_strength = 24;
  whole.squad_establishment = 24;

  snapshot.friendly_units = {make_barracks(50, 40.0F, 44.0F), left, right, whole};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  Game::Systems::AI::SquadDisciplineBehavior behavior;
  ASSERT_TRUE(behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 10.0F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::MergeSquads);
  ASSERT_EQ(commands.front().units.size(), 2U);
  EXPECT_NE(commands.front().units[0], 3U)
      << "a squad already at full strength has nothing to take on";
  EXPECT_NE(commands.front().units[1], 3U);
}

TEST_F(AISystemTest, ACommanderIsNeverDraftedIntoARaidingParty) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;

  auto commander = make_unit(9, 90.0F, 40.0F);
  commander.is_commander = true;
  commander.spawn_type = Game::Units::SpawnType::RomanVeteranConsul;

  snapshot.friendly_units = {make_barracks(50, 40.0F, 40.0F),
                             commander,
                             make_unit(1, 41.0F, 40.0F),
                             make_unit(2, 42.0F, 40.0F)};
  snapshot.visible_enemies = {make_enemy(200, 120.0F, 40.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Harasser);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  EXPECT_EQ(std::find(context.harass_unit_ids.begin(),
                      context.harass_unit_ids.end(),
                      Engine::Core::EntityID{9}),
            context.harass_unit_ids.end())
      << "the lord is the one unit a nation cannot spend on a raid";
}

TEST_F(AISystemTest, AMarketTownBuysTheResourceItHasRunOutOf) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_building(70, 44.0F, 42.0F, Game::Units::SpawnType::Marketplace),
  };
  snapshot.has_resource_snapshot = true;
  snapshot.resources.set(Game::Systems::ResourceType::Gold, 900);
  snapshot.resources.set(Game::Systems::ResourceType::Wood, 300);
  snapshot.resources.set(Game::Systems::ResourceType::Food, 300);
  snapshot.resources.set(Game::Systems::ResourceType::Iron, 300);
  snapshot.resources.set(Game::Systems::ResourceType::Stone, 5);

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  ASSERT_EQ(context.marketplace_count, 1);

  Game::Systems::AI::EconomyBehavior behavior;
  ASSERT_TRUE(behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 10.0F, commands);

  ASSERT_FALSE(commands.empty());
  for (const auto& command : commands) {
    EXPECT_EQ(command.type, Game::Systems::AI::AICommandType::TradeResource);
    EXPECT_EQ(command.trade_resource, Game::Systems::ResourceType::Stone);
    EXPECT_TRUE(command.trade_is_purchase)
        << "gold the town cannot spend is worth less than the stone it is short of";
  }
}

TEST_F(AISystemTest, ATownWithAThinPurseKeepsItsGoldInsteadOfTrading) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_building(70, 44.0F, 42.0F, Game::Units::SpawnType::Marketplace),
  };
  snapshot.has_resource_snapshot = true;

  snapshot.resources.set(Game::Systems::ResourceType::Gold, 120);
  snapshot.resources.set(Game::Systems::ResourceType::Wood, 300);
  snapshot.resources.set(Game::Systems::ResourceType::Food, 300);
  snapshot.resources.set(Game::Systems::ResourceType::Iron, 300);
  snapshot.resources.set(Game::Systems::ResourceType::Stone, 5);

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  Game::Systems::AI::EconomyBehavior behavior;
  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 10.0F, commands);

  for (const auto& command : commands) {
    EXPECT_FALSE(command.trade_is_purchase)
        << "a town this poor should be selling its glut, never spending its last gold";
  }
}

TEST_F(AISystemTest, ATownWithoutAMarketNeverTrades) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {make_barracks(50, 40.0F, 40.0F)};
  snapshot.has_resource_snapshot = true;
  snapshot.resources.set(Game::Systems::ResourceType::Gold, 500);
  snapshot.resources.set(Game::Systems::ResourceType::Stone, 0);

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  Game::Systems::AI::EconomyBehavior behavior;
  EXPECT_FALSE(behavior.should_execute(snapshot, context));
}

TEST_F(AISystemTest, ForwardPlanAbandonsOutpostSiteAfterRepeatedFailures) {
  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Expansionist);

  auto build_snapshot = [](float game_time) {
    Game::Systems::AI::AISnapshot snapshot;
    snapshot.player_id = 3;
    snapshot.game_time = game_time;
    snapshot.friendly_units = {
        make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
        make_building(60, 35.0F, 36.0F, Game::Units::SpawnType::Home),
        make_building(61, 34.0F, 44.0F, Game::Units::SpawnType::Home),
        make_building(62, 45.0F, 37.0F, Game::Units::SpawnType::Home),
        make_builder(10, 38.0F, 38.0F),
    };
    auto enemy_base = make_enemy(301, 140.0F, 40.0F);
    enemy_base.is_building = true;
    enemy_base.spawn_type = Game::Units::SpawnType::Barracks;
    snapshot.strategic_objectives = {enemy_base};
    return snapshot;
  };

  auto snapshot = build_snapshot(10.0F);
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  ASSERT_TRUE(context.has_expansion_site);
  const float first_site_x = context.expansion_site_x;
  const float first_site_z = context.expansion_site_z;

  float now = 10.0F;
  for (int attempt = 0;
       attempt < Game::Systems::AI::AIBaseManager::k_max_outpost_failures;
       ++attempt) {
    Game::Systems::AI::AIBaseManager::note_expansion_order(
        context, now, context.expansion_site_x, context.expansion_site_z);
    now += Game::Systems::AI::AIBaseManager::k_outpost_attempt_timeout + 1.0F;
    auto later = build_snapshot(now);
    Game::Systems::AI::AIReasoner::update_context(later, context);
  }

  EXPECT_EQ(context.forward_plan.abandoned_count, 1);
  ASSERT_EQ(context.abandoned_expansion_sites.size(), 1U);
  EXPECT_FLOAT_EQ(context.abandoned_expansion_sites.front().x, first_site_x);
  EXPECT_TRUE(Game::Systems::AI::AIBaseManager::site_is_abandoned(
      context, first_site_x, first_site_z, now));

  auto next = build_snapshot(now + 1.0F);
  Game::Systems::AI::AIReasoner::update_context(next, context);

  ASSERT_TRUE(context.has_expansion_site);
  EXPECT_FALSE(Game::Systems::AI::AIBaseManager::site_is_abandoned(
      context, context.expansion_site_x, context.expansion_site_z, now + 1.0F));
  EXPECT_GT(std::abs(context.expansion_site_z - first_site_z), 1.0F);
}

TEST_F(AISystemTest, ForwardPlanKeepsSiteWhileConstructionIsStillPending) {
  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Expansionist);

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_building(60, 35.0F, 36.0F, Game::Units::SpawnType::Home),
      make_building(61, 34.0F, 44.0F, Game::Units::SpawnType::Home),
      make_building(62, 45.0F, 37.0F, Game::Units::SpawnType::Home),
      make_builder(10, 38.0F, 38.0F),
  };
  auto enemy_base = make_enemy(301, 140.0F, 40.0F);
  enemy_base.is_building = true;
  enemy_base.spawn_type = Game::Units::SpawnType::Barracks;
  snapshot.strategic_objectives = {enemy_base};

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  ASSERT_TRUE(context.has_expansion_site);

  Game::Systems::AI::AIBaseManager::note_expansion_order(
      context, 10.0F, context.expansion_site_x, context.expansion_site_z);

  snapshot.game_time =
      10.0F + Game::Systems::AI::AIBaseManager::k_outpost_attempt_timeout + 5.0F;
  snapshot.friendly_units[4].builder_production.has_construction_site = true;
  snapshot.friendly_units[4].builder_production.construction_site_x =
      context.expansion_site_x;
  snapshot.friendly_units[4].builder_production.construction_site_z =
      context.expansion_site_z;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  EXPECT_TRUE(context.expansion_construction_pending);
  EXPECT_EQ(context.forward_plan.failed_attempts, 0);
  EXPECT_TRUE(context.has_expansion_site);
  EXPECT_TRUE(context.abandoned_expansion_sites.empty());
}

TEST_F(AISystemTest, ForwardPlanClearsFailureCountOnceOutpostStructureExists) {
  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Expansionist);

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_building(60, 35.0F, 36.0F, Game::Units::SpawnType::Home),
      make_building(61, 34.0F, 44.0F, Game::Units::SpawnType::Home),
      make_building(62, 45.0F, 37.0F, Game::Units::SpawnType::Home),
      make_builder(10, 38.0F, 38.0F),
  };
  auto enemy_base = make_enemy(301, 140.0F, 40.0F);
  enemy_base.is_building = true;
  enemy_base.spawn_type = Game::Units::SpawnType::Barracks;
  snapshot.strategic_objectives = {enemy_base};

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  ASSERT_TRUE(context.has_expansion_site);

  Game::Systems::AI::AIBaseManager::note_expansion_order(
      context, 10.0F, context.expansion_site_x, context.expansion_site_z);
  context.forward_plan.failed_attempts = 2;

  snapshot.game_time = 100.0F;
  snapshot.friendly_units.push_back(make_building(90,
                                                  context.expansion_site_x,
                                                  context.expansion_site_z,
                                                  Game::Units::SpawnType::Barracks));

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  EXPECT_EQ(context.outpost_barracks_count, 1);
  EXPECT_EQ(context.forward_plan.failed_attempts, 0);
  EXPECT_FALSE(context.forward_plan.attempt_in_flight);
  EXPECT_TRUE(context.abandoned_expansion_sites.empty());
}

TEST_F(AISystemTest, ApplierWritesPerBaseRallyPointOntoBarracks) {
  Engine::Core::World world;
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "AI");

  auto* main_barracks = add_world_unit(
      world, 3, 40.0F, 40.0F, 10.0F, true, true, Game::Units::SpawnType::Barracks);
  auto* outpost_barracks = add_world_unit(
      world, 3, 140.0F, 140.0F, 10.0F, true, true, Game::Units::SpawnType::Barracks);

  auto& economy = Game::Session::session_for(world).economy();
  economy.ensure_owner(3);
  for (const auto type : Game::Systems::k_all_resource_types) {
    economy.set(3, type, 500);
  }
  for (auto* barracks : {main_barracks, outpost_barracks}) {
    auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
    ASSERT_NE(production, nullptr);
    production->max_units = 280;
    production->manpower_available = 280;
  }

  Game::Systems::AI::AICommand main_rally;
  main_rally.type = Game::Systems::AI::AICommandType::SetRallyPoint;
  main_rally.building_id = main_barracks->get_id();
  main_rally.rally_x = 35.0F;
  main_rally.rally_z = 40.0F;

  Game::Systems::AI::AICommand outpost_rally;
  outpost_rally.type = Game::Systems::AI::AICommandType::SetRallyPoint;
  outpost_rally.building_id = outpost_barracks->get_id();
  outpost_rally.rally_x = 135.0F;
  outpost_rally.rally_z = 140.0F;

  Game::Systems::AI::AICommandApplier::apply(world, 3, {main_rally, outpost_rally});

  const auto* main_production =
      main_barracks->get_component<Engine::Core::ProductionComponent>();
  const auto* outpost_production =
      outpost_barracks->get_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(main_production, nullptr);
  ASSERT_NE(outpost_production, nullptr);

  EXPECT_TRUE(main_production->rally_set);
  EXPECT_FLOAT_EQ(main_production->rally_x, 35.0F);
  EXPECT_FLOAT_EQ(main_production->rally_z, 40.0F);

  EXPECT_TRUE(outpost_production->rally_set);
  EXPECT_FLOAT_EQ(outpost_production->rally_x, 135.0F);
  EXPECT_FLOAT_EQ(outpost_production->rally_z, 140.0F);
}

TEST_F(AISystemTest, ApplierIgnoresRallyCommandsForForeignBuildings) {
  Engine::Core::World world;
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "AI");
  owners.register_owner_with_id(7, Game::Systems::OwnerType::Player, "Enemy");

  auto* enemy_barracks = add_world_unit(
      world, 7, 140.0F, 140.0F, 10.0F, false, true, Game::Units::SpawnType::Barracks);
  (void)enemy_barracks->add_component<Engine::Core::ProductionComponent>();

  Game::Systems::AI::AICommand rally;
  rally.type = Game::Systems::AI::AICommandType::SetRallyPoint;
  rally.building_id = enemy_barracks->get_id();
  rally.rally_x = 135.0F;
  rally.rally_z = 140.0F;

  Game::Systems::AI::AICommandApplier::apply(world, 3, {rally});

  const auto* production =
      enemy_barracks->get_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(production, nullptr);
  EXPECT_FALSE(production->rally_set);
}

TEST_F(AISystemTest, CommandFilterKeepsRallyCommandsThatCarryNoUnits) {
  Game::Systems::AI::AICommandFilter filter;

  Game::Systems::AI::AICommand rally;
  rally.type = Game::Systems::AI::AICommandType::SetRallyPoint;
  rally.building_id = 50;
  rally.rally_x = 35.0F;
  rally.rally_z = 40.0F;

  const auto filtered = filter.filter({rally}, 1.0F);

  ASSERT_EQ(filtered.size(), 1U);
  EXPECT_EQ(filtered.front().type, Game::Systems::AI::AICommandType::SetRallyPoint);
  EXPECT_EQ(filtered.front().building_id, 50U);
}

TEST_F(AISystemTest, PipelineKeepsSecondBaseProducingAfterMainBaseIsDestroyed) {
  register_economy_nation();

  Engine::Core::World world;
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "AI");

  auto* main_barracks = add_world_unit(
      world, 3, 40.0F, 40.0F, 20.0F, true, true, Game::Units::SpawnType::Barracks);
  auto* main_home = add_world_unit(
      world, 3, 36.0F, 42.0F, 20.0F, true, true, Game::Units::SpawnType::Home);
  auto* outpost_barracks = add_world_unit(
      world, 3, 100.0F, 40.0F, 20.0F, true, true, Game::Units::SpawnType::Barracks);

  auto& economy = Game::Session::session_for(world).economy();
  economy.ensure_owner(3);
  for (const auto type : Game::Systems::k_all_resource_types) {
    economy.set(3, type, 500);
  }
  for (auto* barracks : {main_barracks, outpost_barracks}) {
    auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
    ASSERT_NE(production, nullptr);
    production->max_units = 280;
    production->manpower_available = 280;
  }

  Game::Systems::AI::AIBehaviorRegistry registry;
  registry.register_behavior(std::make_unique<Game::Systems::AI::ProductionBehavior>());

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.nation = Game::Systems::NationRegistry::instance().get_nation_for_player(3);
  context.max_troops_per_player = 500;

  auto run_pipeline = [&](float delta_time) {
    const auto snapshot = Game::Systems::AI::AISnapshotBuilder::build(world, 3);
    Game::Systems::AI::AIReasoner::update_context(snapshot, context);
    std::vector<Game::Systems::AI::AICommand> commands;
    Game::Systems::AI::AIExecutor::run(
        snapshot, context, delta_time, registry, commands);
    Game::Systems::AI::AICommandApplier::apply(world, 3, commands);
    return commands;
  };

  const auto first_commands = run_pipeline(5.0F);

  ASSERT_EQ(context.bases.size(), 2U);
  EXPECT_EQ(context.primary_barracks, main_barracks->get_id());

  const auto* main_production =
      main_barracks->get_component<Engine::Core::ProductionComponent>();
  const auto* outpost_production =
      outpost_barracks->get_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(main_production, nullptr);
  ASSERT_NE(outpost_production, nullptr);
  EXPECT_TRUE(main_production->rally_set);
  EXPECT_TRUE(outpost_production->rally_set);
  EXPECT_FLOAT_EQ(main_production->rally_x, 35.0F);
  EXPECT_FLOAT_EQ(outpost_production->rally_x, 95.0F);

  const auto produced_for_outpost = std::count_if(
      first_commands.begin(),
      first_commands.end(),
      [&](const Game::Systems::AI::AICommand& cmd) {
        return cmd.type == Game::Systems::AI::AICommandType::StartProduction &&
               cmd.building_id == outpost_barracks->get_id();
      });
  EXPECT_EQ(produced_for_outpost, 1);

  const auto destroyed_base_id = context.main_base_id;
  world.destroy_entity(main_barracks->get_id());
  world.destroy_entity(main_home->get_id());

  const auto second_commands = run_pipeline(5.0F);

  ASSERT_EQ(context.bases.size(), 1U);
  EXPECT_NE(context.main_base_id, destroyed_base_id);
  EXPECT_EQ(context.primary_barracks, outpost_barracks->get_id());
  EXPECT_EQ(context.bases.front().role, Game::Systems::AI::BaseRole::Main);

  const auto still_producing = std::count_if(
      second_commands.begin(),
      second_commands.end(),
      [&](const Game::Systems::AI::AICommand& cmd) {
        return cmd.type == Game::Systems::AI::AICommandType::StartProduction &&
               cmd.building_id == outpost_barracks->get_id();
      });
  EXPECT_EQ(still_producing, 1);
}

TEST_F(AISystemTest, AssaultWaveUnitsAreTrackedSeparatelyFromTheStandingArmy) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;

  auto wave_unit = make_unit(20, 90.0F, 40.0F);
  wave_unit.is_assault = true;
  auto second_wave_unit = make_unit(21, 92.0F, 41.0F);
  second_wave_unit.is_assault = true;

  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 38.0F, 40.0F),
      make_unit(2, 36.0F, 41.0F),
      wave_unit,
      second_wave_unit,
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  EXPECT_EQ(context.assault_unit_count, 2);
  EXPECT_EQ(context.assault_unit_ids, (std::vector<Engine::Core::EntityID>{20U, 21U}));

  const auto attack_force =
      Game::Systems::AI::collect_attack_force_units(snapshot, context);
  for (const auto* unit : attack_force) {
    EXPECT_FALSE(unit->is_assault);
  }
}

TEST_F(AISystemTest, AssaultBehaviorAttacksWhileTheAIItselfStaysDefensive) {
  Game::Systems::AI::AssaultBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;

  auto wave_unit = make_unit(20, 90.0F, 40.0F);
  wave_unit.is_assault = true;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      wave_unit,
  };
  snapshot.visible_enemies = {make_enemy(200, 120.0F, 40.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  context.state = Game::Systems::AI::AIState::Defending;

  ASSERT_TRUE(behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);

  ASSERT_EQ(commands.size(), 1U);
  const auto& command = commands.front();
  EXPECT_EQ(command.type, Game::Systems::AI::AICommandType::MoveUnits);
  ASSERT_EQ(command.units.size(), 1U);
  EXPECT_EQ(command.units.front(), 20U);
  ASSERT_FALSE(command.move_target_x.empty());
  EXPECT_GT(command.move_target_x.front(), 100.0F);
}

TEST_F(AISystemTest, AssaultBehaviorEngagesTargetsItHasAlreadyReached) {
  Game::Systems::AI::AssaultBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;

  auto wave_unit = make_unit(20, 118.0F, 40.0F);
  wave_unit.is_assault = true;
  snapshot.friendly_units = {wave_unit};
  snapshot.visible_enemies = {make_enemy(200, 120.0F, 40.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.assault_unit_count = 1;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::AttackTarget);
  EXPECT_EQ(commands.front().target_id, 200U);
  EXPECT_TRUE(commands.front().should_chase);
}

TEST_F(AISystemTest, LocalEngagementRespondsWithNearbyUnitsOnly) {
  Game::Systems::AI::LocalEngagementBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 110.0F, 40.0F),
      make_unit(2, 111.0F, 41.0F),
      make_unit(3, 112.0F, 42.0F),
      make_unit(4, 40.0F, 45.0F),
      make_unit(5, 41.0F, 46.0F),
      make_unit(6, 42.0F, 47.0F),
      make_unit(7, 43.0F, 48.0F),
      make_unit(8, 44.0F, 49.0F),
      make_unit(9, 45.0F, 50.0F),
  };
  snapshot.visible_enemies = {make_enemy(200, 118.0F, 40.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  context.state = Game::Systems::AI::AIState::Gathering;

  ASSERT_TRUE(behavior.should_execute(snapshot, context));

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);

  ASSERT_EQ(commands.size(), 1U);
  const auto& command = commands.front();
  EXPECT_EQ(command.type, Game::Systems::AI::AICommandType::AttackTarget);
  EXPECT_EQ(command.target_id, 200U);
  EXPECT_TRUE(command.should_chase);
  EXPECT_LE(static_cast<int>(command.units.size()),
            context.strategy_config.max_local_responders);
  EXPECT_LT(static_cast<int>(command.units.size()), context.total_units);

  for (const auto unit_id : command.units) {
    EXPECT_TRUE(unit_id == 1U || unit_id == 2U || unit_id == 3U);
    EXPECT_EQ(std::string_view(context.assigned_units.at(unit_id).assigned_task),
              Game::Systems::AI::LocalEngagementBehavior::k_task_name);
  }
}

TEST_F(AISystemTest, LocalEngagementKeepsRespondingWhileAnotherBaseIsUnderAttack) {
  Game::Systems::AI::LocalEngagementBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 110.0F, 40.0F),
      make_unit(2, 111.0F, 41.0F),
      make_unit(3, 112.0F, 42.0F),
  };
  snapshot.visible_enemies = {
      make_enemy(200, 118.0F, 40.0F),
      make_enemy(201, 44.0F, 42.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  ASSERT_TRUE(context.any_base_under_threat);
  context.state = Game::Systems::AI::AIState::Defending;

  ASSERT_TRUE(behavior.should_execute(snapshot, context));
  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().target_id, 200U);
}

TEST_F(AISystemTest, LocalEngagementLeavesUnitsClaimedByHigherPriorityWork) {
  Game::Systems::AI::LocalEngagementBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_unit(1, 70.0F, 40.0F),
      make_unit(2, 71.0F, 41.0F),
  };
  snapshot.visible_enemies = {make_enemy(200, 78.0F, 40.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.total_units = 2;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);
  Game::Systems::AI::claim_units(
      {1U}, Game::Systems::AI::BehaviorPriority::Critical, "defending", context, 7.0F);
  Game::Systems::AI::claim_units(
      {2U}, Game::Systems::AI::BehaviorPriority::Low, "gathering", context, 7.0F);

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);

  ASSERT_EQ(commands.size(), 1U);
  ASSERT_EQ(commands.front().units.size(), 1U);
  EXPECT_EQ(commands.front().units.front(), 2U);
}

TEST_F(AISystemTest, LocalEngagementDoesNotChargeAnOverwhelmingForce) {
  Game::Systems::AI::LocalEngagementBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {make_unit(1, 70.0F, 40.0F), make_unit(2, 71.0F, 41.0F)};
  for (Engine::Core::EntityID id = 200; id < 212; ++id) {
    snapshot.visible_enemies.push_back(
        make_enemy(id, 84.0F + static_cast<float>(id - 200), 40.0F));
  }

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.total_units = 2;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);
  EXPECT_TRUE(commands.empty());
}

TEST_F(AISystemTest, LocalEngagementReleasesRespondersWhenThreatsLeave) {
  Game::Systems::AI::LocalEngagementBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {make_unit(1, 70.0F, 40.0F)};
  snapshot.visible_enemies = {make_enemy(200, 76.0F, 40.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.total_units = 1;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);
  ASSERT_EQ(commands.size(), 1U);
  ASSERT_TRUE(context.assigned_units.contains(1U));

  snapshot.visible_enemies = {make_enemy(200, 160.0F, 40.0F)};
  snapshot.game_time = 12.0F;
  commands.clear();
  behavior.execute(snapshot, context, 2.0F, commands);
  EXPECT_TRUE(commands.empty());
  EXPECT_FALSE(context.assigned_units.contains(1U));
}

TEST_F(AISystemTest, LocalEngagementIsDisabledWhenAStrategyDoesNotWantIt) {
  Game::Systems::AI::LocalEngagementBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {make_unit(1, 70.0F, 40.0F)};
  snapshot.visible_enemies = {make_enemy(200, 74.0F, 40.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.total_units = 1;
  context.strategy_config.local_response_radius = 0.0F;
  context.strategy_config.max_local_responders = 0;

  EXPECT_FALSE(behavior.should_execute(snapshot, context));
}

TEST_F(AISystemTest, GarrisonPostureNeverInitiatesAttacksOrExpansion) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 30.0F;
  snapshot.friendly_units = {make_barracks(50, 40.0F, 40.0F)};
  for (Engine::Core::EntityID id = 1; id <= 12; ++id) {
    snapshot.friendly_units.push_back(make_unit(
        id, 36.0F + static_cast<float>(id % 4), 40.0F + static_cast<float>(id / 4)));
  }
  snapshot.visible_enemies = {make_enemy(200, 140.0F, 40.0F)};

  Game::Systems::AI::AIPlayerProfile profile;
  profile.strategy = Game::Systems::AI::AIStrategy::Aggressive;
  profile.posture = Game::Systems::AI::AIPosture::Garrison;
  profile.personality.aggression = 0.9F;

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config =
      Game::Systems::AI::AIStrategyFactory::create_config(profile);
  context.state = Game::Systems::AI::AIState::Gathering;
  context.state_timer = 30.0F;
  context.decision_timer = 30.0F;
  context.last_meaningful_action_time = 30.0F;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.5F);
  Game::Systems::AI::AIReasoner::validate_state(context);

  EXPECT_NE(context.state, Game::Systems::AI::AIState::Attacking);
  EXPECT_NE(context.state, Game::Systems::AI::AIState::Expanding);
  EXPECT_EQ(context.effective_harass_units, 0);
  EXPECT_FALSE(context.has_expansion_site);

  context.state = Game::Systems::AI::AIState::Attacking;
  Game::Systems::AI::AIReasoner::validate_state(context);
  EXPECT_NE(context.state, Game::Systems::AI::AIState::Attacking);
}

TEST_F(AISystemTest, FieldPostureWithTheSameStrategyStillAttacks) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 30.0F;
  snapshot.friendly_units = {make_barracks(50, 40.0F, 40.0F)};
  for (Engine::Core::EntityID id = 1; id <= 12; ++id) {
    snapshot.friendly_units.push_back(make_unit(
        id, 36.0F + static_cast<float>(id % 4), 40.0F + static_cast<float>(id / 4)));
  }
  snapshot.visible_enemies = {make_enemy(200, 140.0F, 40.0F)};

  Game::Systems::AI::AIPlayerProfile profile;
  profile.strategy = Game::Systems::AI::AIStrategy::Aggressive;
  profile.posture = Game::Systems::AI::AIPosture::Field;
  profile.personality.aggression = 0.9F;

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config =
      Game::Systems::AI::AIStrategyFactory::create_config(profile);
  context.state = Game::Systems::AI::AIState::Gathering;
  context.state_timer = 30.0F;
  context.decision_timer = 30.0F;
  context.last_meaningful_action_time = 30.0F;

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  Game::Systems::AI::AIReasoner::update_state_machine(snapshot, context, 0.5F);

  EXPECT_EQ(context.state, Game::Systems::AI::AIState::Attacking);
}

TEST_F(AISystemTest, PostureParsingFallsBackToTheCallerDefault) {
  using Game::Systems::AI::AIPosture;
  namespace AIStrategyFactory = Game::Systems::AI::AIStrategyFactory;
  EXPECT_EQ(AIStrategyFactory::parse_posture("garrison"), AIPosture::Garrison);
  EXPECT_EQ(AIStrategyFactory::parse_posture("Field", AIPosture::Garrison),
            AIPosture::Field);
  EXPECT_EQ(AIStrategyFactory::parse_posture("", AIPosture::Garrison),
            AIPosture::Garrison);
  EXPECT_EQ(AIStrategyFactory::parse_posture("nonsense"), AIPosture::Field);
}

TEST_F(AISystemTest, GarrisonGathersEachUnitAtItsNearestBase) {
  Game::Systems::AI::GatherBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_barracks(50, 40.0F, 40.0F),
      make_building(51, 140.0F, 40.0F, Game::Units::SpawnType::Home),
      make_building(52, 146.0F, 44.0F, Game::Units::SpawnType::Home),
      make_unit(1, 60.0F, 40.0F),
      make_unit(2, 120.0F, 40.0F),
  };

  Game::Systems::AI::AIPlayerProfile profile;
  profile.strategy = Game::Systems::AI::AIStrategy::Defensive;
  profile.posture = Game::Systems::AI::AIPosture::Garrison;

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config =
      Game::Systems::AI::AIStrategyFactory::create_config(profile);
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  ASSERT_EQ(context.bases.size(), 2U);
  context.state = Game::Systems::AI::AIState::Gathering;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);

  bool unit_two_goes_east = false;
  for (const auto& command : commands) {
    if (command.type != Game::Systems::AI::AICommandType::MoveUnits) {
      continue;
    }
    for (std::size_t i = 0; i < command.units.size(); ++i) {
      if (command.units[i] == 2U) {
        unit_two_goes_east = command.move_target_x[i] > 100.0F;
      }
    }
  }
  EXPECT_TRUE(unit_two_goes_east);
}

TEST_F(AISystemTest, ExpansionistSkirmishAIRecallsEveryUnitWhenItsBaseIsAttacked) {
  Game::Systems::AI::DefendBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 120.0F, 120.0F),
      make_unit(2, 122.0F, 121.0F),
      make_unit(3, 124.0F, 122.0F),
      make_unit(4, 126.0F, 123.0F),
      make_unit(5, 128.0F, 124.0F),
      make_unit(6, 130.0F, 125.0F),
      make_unit(7, 132.0F, 126.0F),
      make_unit(8, 134.0F, 127.0F),
  };
  snapshot.visible_enemies = {make_enemy(200, 44.0F, 42.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Expansionist);
  ASSERT_TRUE(context.strategy_config.full_recall_on_base_threat);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  context.state = Game::Systems::AI::AIState::Defending;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);

  ASSERT_FALSE(commands.empty());
  std::size_t commanded_units = 0;
  for (const auto& command : commands) {
    commanded_units += command.units.size();
  }
  EXPECT_EQ(commanded_units, 8U);
}

TEST_F(AISystemTest, DefendBehaviorAnswersASingleIntruderWithAProportionalForce) {
  Game::Systems::AI::DefendBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {make_barracks(50, 40.0F, 40.0F)};
  for (Engine::Core::EntityID id = 1; id <= 8; ++id) {
    snapshot.friendly_units.push_back(
        make_unit(id, 30.0F + static_cast<float>(id), 40.0F));
  }
  snapshot.visible_enemies = {make_enemy(200, 60.0F, 40.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);
  ASSERT_FALSE(context.strategy_config.full_recall_on_base_threat);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  ASSERT_TRUE(context.barracks_under_threat);
  context.state = Game::Systems::AI::AIState::Defending;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, Game::Systems::AI::AICommandType::AttackTarget);
  EXPECT_EQ(commands.front().units.size(), 2U);
}

TEST_F(AISystemTest, DefendBehaviorDoesNotFeedMoreUnitsIntoAFightAlreadyCovered) {
  Game::Systems::AI::DefendBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {make_barracks(50, 40.0F, 40.0F),
                             make_unit(1, 58.0F, 40.0F),
                             make_unit(2, 59.0F, 41.0F),
                             make_unit(3, 30.0F, 40.0F),
                             make_unit(4, 31.0F, 40.0F)};
  snapshot.visible_enemies = {make_enemy(200, 60.0F, 40.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  context.state = Game::Systems::AI::AIState::Defending;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);

  for (const auto& command : commands) {
    EXPECT_NE(command.type, Game::Systems::AI::AICommandType::AttackTarget);
  }
}

TEST_F(AISystemTest, EnemyBuildingsNearABaseAreNotCountedAsThreats) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {make_barracks(50, 40.0F, 40.0F),
                             make_unit(1, 38.0F, 40.0F)};
  snapshot.visible_enemies = {make_enemy_building(200, 55.0F, 40.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  EXPECT_EQ(context.nearby_threat_count, 0);
  EXPECT_FALSE(context.barracks_under_threat);
  EXPECT_FALSE(context.any_base_under_threat);
}

TEST_F(AISystemTest, HomesWithoutBarracksStillAnchorTheBase) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Home),
      make_building(51, 46.0F, 40.0F, Game::Units::SpawnType::Home),
      make_unit(1, 80.0F, 80.0F),
      make_unit(2, 82.0F, 82.0F),
      make_unit(3, 84.0F, 84.0F),
  };

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  EXPECT_TRUE(context.has_base_anchor);
  EXPECT_TRUE(context.anchor_is_structural);
  EXPECT_EQ(context.primary_barracks, 0U);
  EXPECT_NEAR(context.base_pos_x, 43.0F, 0.01F);
  EXPECT_NEAR(context.base_pos_z, 40.0F, 0.01F);
}

TEST_F(AISystemTest, AssaultUnitsStayOnTheAttackWhileTheBaseIsRecalled) {
  Game::Systems::AI::DefendBehavior behavior;

  Game::Systems::AI::AISnapshot snapshot;
  snapshot.player_id = 3;
  snapshot.game_time = 10.0F;

  auto wave_unit = make_unit(20, 130.0F, 130.0F);
  wave_unit.is_assault = true;
  snapshot.friendly_units = {
      make_building(50, 40.0F, 40.0F, Game::Units::SpawnType::Barracks),
      make_unit(1, 120.0F, 120.0F),
      make_unit(2, 122.0F, 121.0F),
      wave_unit,
  };
  snapshot.visible_enemies = {make_enemy(200, 44.0F, 42.0F)};

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);

  Game::Systems::AI::AIReasoner::update_context(snapshot, context);
  context.state = Game::Systems::AI::AIState::Defending;

  std::vector<Game::Systems::AI::AICommand> commands;
  behavior.execute(snapshot, context, 2.0F, commands);

  for (const auto& command : commands) {
    for (const auto unit_id : command.units) {
      EXPECT_NE(unit_id, 20U);
    }
  }
}

TEST_F(AISystemTest, StrategyPresetsGiveDefendersLocalResponseAndSkirmishFullRecall) {
  const auto defensive = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);
  EXPECT_GT(defensive.local_response_radius, 0.0F);
  EXPECT_GT(defensive.max_local_responders, 0);
  EXPECT_FALSE(defensive.full_recall_on_base_threat);

  const auto expansionist = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Expansionist);
  EXPECT_TRUE(expansionist.full_recall_on_base_threat);
  EXPECT_GT(expansionist.expansion_priority, defensive.expansion_priority);
  EXPECT_GT(expansionist.desired_outpost_barracks_count,
            defensive.desired_outpost_barracks_count);
}

TEST_F(AISystemTest, SnapshotBuilderResolvesEngagementOncePerTick) {
  Engine::Core::World world;
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "AI");
  owners.register_owner_with_id(7, Game::Systems::OwnerType::Player, "Enemy");

  add_world_unit(world, 3, 40.0F, 40.0F, 30.0F, true);
  add_world_unit(world, 3, 90.0F, 90.0F, 30.0F, true);
  add_world_unit(world, 7, 42.0F, 41.0F, 30.0F, false);

  const auto snapshot = Game::Systems::AI::AISnapshotBuilder::build(world, 3);

  ASSERT_EQ(snapshot.friendly_units.size(), 2U);
  for (const auto& friendly : snapshot.friendly_units) {
    EXPECT_TRUE(friendly.engagement_resolved);
  }

  const auto* near_enemy = &snapshot.friendly_units.front();
  const auto* far_from_enemy = &snapshot.friendly_units.back();
  if (near_enemy->pos_x > 50.0F) {
    std::swap(near_enemy, far_from_enemy);
  }

  EXPECT_TRUE(near_enemy->engaged);
  EXPECT_FALSE(far_from_enemy->engaged);
  EXPECT_TRUE(
      Game::Systems::AI::is_entity_engaged(*near_enemy, snapshot.visible_enemies));
  EXPECT_FALSE(
      Game::Systems::AI::is_entity_engaged(*far_from_enemy, snapshot.visible_enemies));
}

TEST_F(AISystemTest, EngagementHelperStillScansForHandBuiltSnapshots) {
  Game::Systems::AI::AISnapshot snapshot;
  snapshot.friendly_units = {make_unit(1, 40.0F, 40.0F)};
  snapshot.visible_enemies = {make_enemy(200, 42.0F, 41.0F)};

  ASSERT_FALSE(snapshot.friendly_units.front().engagement_resolved);
  EXPECT_TRUE(Game::Systems::AI::is_entity_engaged(snapshot.friendly_units.front(),
                                                   snapshot.visible_enemies));
}

TEST_F(AISystemTest, SnapshotBuilderMarksAssaultWaveUnitsFromTheirComponent) {
  Engine::Core::World world;
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "AI");

  auto* garrison = add_world_unit(world, 3, 40.0F, 40.0F, 20.0F, true);
  auto* wave_unit = add_world_unit(world, 3, 90.0F, 90.0F, 20.0F, true);
  (void)wave_unit->add_component<Engine::Core::AssaultWaveComponent>();

  auto snapshot = Game::Systems::AI::AISnapshotBuilder::build(world, 3);
  ASSERT_EQ(snapshot.friendly_units.size(), 2U);

  for (const auto& entity : snapshot.friendly_units) {
    if (entity.id == wave_unit->get_id()) {
      EXPECT_TRUE(entity.is_assault);
    } else {
      EXPECT_EQ(entity.id, garrison->get_id());
      EXPECT_FALSE(entity.is_assault);
    }
  }

  Game::Systems::AI::AIContext context;
  context.player_id = 3;
  context.strategy_config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);
  Game::Systems::AI::AIReasoner::update_context(snapshot, context);

  EXPECT_EQ(context.assault_unit_count, 1);
  ASSERT_EQ(context.assault_unit_ids.size(), 1U);
  EXPECT_EQ(context.assault_unit_ids.front(), wave_unit->get_id());
}

TEST_F(AISystemTest, DeactivatedAssaultComponentReturnsAUnitToTheStandingArmy) {
  Engine::Core::World world;
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(3, Game::Systems::OwnerType::AI, "AI");

  auto* wave_unit = add_world_unit(world, 3, 90.0F, 90.0F, 20.0F, true);
  auto* assault = wave_unit->add_component<Engine::Core::AssaultWaveComponent>();
  assault->active = false;

  const auto snapshot = Game::Systems::AI::AISnapshotBuilder::build(world, 3);
  ASSERT_EQ(snapshot.friendly_units.size(), 1U);
  EXPECT_FALSE(snapshot.friendly_units.front().is_assault);
}

} // namespace
