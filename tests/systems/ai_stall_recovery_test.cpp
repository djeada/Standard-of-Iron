

#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "game/systems/ai_system/ai_stall_recovery.h"
#include "game/systems/ai_system/ai_types.h"

namespace {

using Game::Systems::AI::AICommand;
using Game::Systems::AI::AICommandType;
using Game::Systems::AI::AIContext;
using Game::Systems::AI::AISnapshot;
using Game::Systems::AI::BehaviorPriority;
using Game::Systems::AI::EntitySnapshot;
using Game::Systems::AI::is_stood_down;
using Game::Systems::AI::k_max_stall_nudges;
using Game::Systems::AI::k_stall_nudge_interval_seconds;
using Game::Systems::AI::update_stall_recovery;

constexpr Engine::Core::EntityID k_unit = 7;

auto wedged_unit() -> EntitySnapshot {
  EntitySnapshot entity;
  entity.id = k_unit;
  entity.owner_id = 1;
  entity.health = 100;
  entity.max_health = 100;
  entity.pos_x = 0.0F;
  entity.pos_z = 0.0F;
  entity.movement.has_component = true;
  entity.movement.has_target = true;
  entity.movement.has_objective = true;
  entity.movement.objective_x = 40.0F;
  entity.movement.objective_z = 0.0F;
  entity.movement.stalled = true;
  entity.movement.stalled_seconds = 5.0F;
  return entity;
}

auto snapshot_with(const EntitySnapshot& entity, float game_time) -> AISnapshot {
  AISnapshot snapshot;
  snapshot.player_id = 1;
  snapshot.game_time = game_time;
  snapshot.friendly_units = {entity};
  return snapshot;
}

auto claimed_context() -> AIContext {
  AIContext context;
  context.player_id = 1;
  AIContext::UnitAssignment assignment;
  assignment.owner_priority = BehaviorPriority::Normal;
  assignment.assigned_task = "attack";
  context.assigned_units[k_unit] = assignment;
  context.wave.committed = true;
  context.wave.members = {k_unit};
  return context;
}

} // namespace

TEST(AIStallRecoveryTest, AWedgedUnitIsSentAtItsObjectiveFromASide) {
  auto context = claimed_context();
  const auto entity = wedged_unit();
  std::vector<AICommand> commands;
  update_stall_recovery(snapshot_with(entity, 100.0F), context, commands);

  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands.front().type, AICommandType::MoveUnits);
  ASSERT_EQ(commands.front().units.size(), 1U);
  EXPECT_EQ(commands.front().units.front(), k_unit);
  ASSERT_EQ(commands.front().move_target_x.size(), 1U);

  EXPECT_GT(std::abs(commands.front().move_target_z.front()), 1.0F)
      << "the unit was re-sent straight down the route it is already wedged on";
}

TEST(AIStallRecoveryTest, ConsecutiveNudgesTakeDifferentSides) {
  auto context = claimed_context();
  const auto entity = wedged_unit();

  std::vector<float> lateral;
  for (int attempt = 0; attempt < 2; ++attempt) {
    std::vector<AICommand> commands;
    const float now =
        100.0F + (static_cast<float>(attempt) * k_stall_nudge_interval_seconds);
    update_stall_recovery(snapshot_with(entity, now), context, commands);
    ASSERT_EQ(commands.size(), 1U) << "attempt " << attempt;
    lateral.push_back(commands.front().move_target_z.front());
  }

  EXPECT_LT(lateral[0] * lateral[1], 0.0F)
      << "both attempts detoured to the same side of the same obstacle";
}

TEST(AIStallRecoveryTest, NudgingStopsAndTheUnitIsHandedBackForRetasking) {
  auto context = claimed_context();
  const auto entity = wedged_unit();

  float now = 100.0F;
  int nudges = 0;
  for (int cycle = 0; cycle < 20; ++cycle) {
    std::vector<AICommand> commands;
    update_stall_recovery(snapshot_with(entity, now), context, commands);
    nudges += static_cast<int>(commands.size());
    now += k_stall_nudge_interval_seconds;
    if (is_stood_down(k_unit, context, now)) {
      break;
    }
  }

  EXPECT_LE(nudges, k_max_stall_nudges)
      << "the AI kept re-ordering a unit that never moved";
  EXPECT_TRUE(is_stood_down(k_unit, context, now));
  EXPECT_FALSE(context.assigned_units.contains(k_unit))
      << "the unit was stood down but its task still held it";
  EXPECT_TRUE(context.wave.members.empty())
      << "a unit that cannot reach the objective was left in the wave";
}

TEST(AIStallRecoveryTest, AUnitTheMoverAlreadyGaveUpOnTwiceIsNotNudgedAtAll) {
  auto context = claimed_context();
  auto entity = wedged_unit();
  entity.movement.objective_abandoned = true;
  entity.movement.abandon_count = 2;
  entity.movement.has_target = false;

  std::vector<AICommand> commands;
  update_stall_recovery(snapshot_with(entity, 100.0F), context, commands);

  EXPECT_TRUE(commands.empty())
      << "the AI re-issued an objective the mover had already given up on twice";
  EXPECT_TRUE(is_stood_down(k_unit, context, 100.0F));
}

TEST(AIStallRecoveryTest, AUnitThatIsActuallyMovingIsLeftAlone) {
  auto context = claimed_context();
  auto entity = wedged_unit();
  entity.movement.stalled = false;
  entity.movement.stalled_seconds = 0.0F;

  std::vector<AICommand> commands;
  update_stall_recovery(snapshot_with(entity, 100.0F), context, commands);

  EXPECT_TRUE(commands.empty());
  EXPECT_FALSE(is_stood_down(k_unit, context, 100.0F));
  EXPECT_TRUE(context.assigned_units.contains(k_unit))
      << "a unit that was marching normally was taken off its task";
}

TEST(AIStallRecoveryTest, AStandDownExpiresSoTheUnitComesBack) {
  auto context = claimed_context();
  auto entity = wedged_unit();
  entity.movement.abandon_count = 2;

  std::vector<AICommand> commands;
  update_stall_recovery(snapshot_with(entity, 100.0F), context, commands);
  ASSERT_TRUE(is_stood_down(k_unit, context, 100.0F));

  EXPECT_FALSE(is_stood_down(k_unit, context, 100.0F + 60.0F))
      << "the stand-down never ended, so the unit was benched for good";
}
