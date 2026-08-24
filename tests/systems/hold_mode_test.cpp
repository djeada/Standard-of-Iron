#include <QVector3D>

#include <gtest/gtest.h>
#include <vector>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "game/command/command.h"
#include "game/command/command_dispatcher.h"
#include "systems/combat_system/damage_processor.h"
#include "systems/command_service.h"
#include "systems/movement_pipeline.h"
#include "systems/nav_grid.h"
#include "systems/owner_registry.h"
#include "units/spawn_type.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

class HoldModeTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    OwnerRegistry::instance().clear();
    NavGrid::initialize(64, 64);
  }

  void TearDown() override { world.reset(); }

  auto spawn(Game::Units::SpawnType type,
             float x = 0.0F,
             float z = 0.0F,
             int owner_id = 1,
             int health = 100) -> Entity* {
    auto* entity = world->create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<UnitComponent>(health, health, 1.0F, 12.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = type;
    entity->add_component<MovementComponent>();
    return entity;
  }

  void set_hold(Entity* entity, bool active) {
    Game::Command::Command command{};
    command.source = Game::Command::Source::LocalPlayer;
    command.owner_id = 1;
    command.payload =
        Game::Command::SetHold{.units = {entity->get_id()}, .active = active};
    Game::Command::dispatch(*world, command);
  }

  void set_guard(Entity* entity) {
    Game::Command::Command command{};
    command.source = Game::Command::Source::LocalPlayer;
    command.owner_id = 1;
    command.payload =
        Game::Command::SetGuard{.units = {entity->get_id()}, .active = true};
    Game::Command::dispatch(*world, command);
  }

  void advance(float seconds, float step = 1.0F / 60.0F) {
    for (float elapsed = 0.0F; elapsed < seconds; elapsed += step) {
      movement.update(world.get(), step);
    }
  }

  std::unique_ptr<World> world;
  MovementPipeline movement;
};

TEST_F(HoldModeTest, StanceStaysLimitedToArchersAndSpearmen) {
  auto* archer = spawn(Game::Units::SpawnType::Archer);
  auto* spearman = spawn(Game::Units::SpawnType::Spearman, 2.0F);
  auto* swordsman = spawn(Game::Units::SpawnType::Knight, 4.0F);
  auto* civilian = spawn(Game::Units::SpawnType::Civilian, 6.0F);

  for (auto* entity : {archer, spearman, swordsman, civilian}) {
    set_hold(entity, true);
  }

  auto const* archer_hold = archer->get_component<HoldModeComponent>();
  auto const* spearman_hold = spearman->get_component<HoldModeComponent>();
  ASSERT_NE(archer_hold, nullptr);
  ASSERT_NE(spearman_hold, nullptr);
  EXPECT_TRUE(archer_hold->active);
  EXPECT_TRUE(spearman_hold->active);

  EXPECT_EQ(swordsman->get_component<HoldModeComponent>(), nullptr);
  EXPECT_EQ(civilian->get_component<HoldModeComponent>(), nullptr);
}

TEST_F(HoldModeTest, KneelProgressRampsOverTheEntryDuration) {
  auto* archer = spawn(Game::Units::SpawnType::Archer);
  set_hold(archer, true);

  auto* hold = archer->get_component<HoldModeComponent>();
  ASSERT_NE(hold, nullptr);
  EXPECT_FLOAT_EQ(hold->kneel_entry_progress, 0.0F);

  advance(hold->kneel_duration * 0.5F);
  EXPECT_GT(hold->kneel_entry_progress, 0.2F);
  EXPECT_LT(hold->kneel_entry_progress, 0.8F);

  advance(hold->kneel_duration);
  EXPECT_FLOAT_EQ(hold->kneel_entry_progress, 1.0F);
}

TEST_F(HoldModeTest, MoveOrderStandsUpInsteadOfSnapping) {
  auto* spearman = spawn(Game::Units::SpawnType::Spearman);
  set_hold(spearman, true);
  advance(2.0F);

  auto* hold = spearman->get_component<HoldModeComponent>();
  ASSERT_NE(hold, nullptr);
  ASSERT_FLOAT_EQ(hold->kneel_entry_progress, 1.0F);

  CommandService::move_unit(*world, spearman->get_id(), QVector3D(6.0F, 0.0F, 0.0F));

  EXPECT_FALSE(hold->active);
  EXPECT_FLOAT_EQ(hold->exit_cooldown, hold->stand_up_duration);
}

TEST_F(HoldModeTest, GuardOrderStandsUpInsteadOfSnapping) {
  auto* spearman = spawn(Game::Units::SpawnType::Spearman);
  set_hold(spearman, true);
  advance(2.0F);

  auto* hold = spearman->get_component<HoldModeComponent>();
  ASSERT_NE(hold, nullptr);

  set_guard(spearman);

  EXPECT_FALSE(hold->active);
  EXPECT_GT(hold->exit_cooldown, 0.0F);
}

TEST_F(HoldModeTest, DeathReleasesTheStance) {
  auto* archer = spawn(Game::Units::SpawnType::Archer, 0.0F, 0.0F, 1, 40);
  auto* attacker = spawn(Game::Units::SpawnType::Knight, 2.0F, 0.0F, 2);
  set_hold(archer, true);
  advance(2.0F);

  auto* hold = archer->get_component<HoldModeComponent>();
  ASSERT_NE(hold, nullptr);
  ASSERT_TRUE(hold->active);

  Game::Systems::Combat::deal_damage(world.get(), archer, 40, attacker->get_id());

  EXPECT_FALSE(hold->active);
  EXPECT_GT(hold->exit_cooldown, 0.0F);
}

TEST_F(HoldModeTest, ExitCooldownScalesWithHowDeepTheKneelGot) {
  auto* archer = spawn(Game::Units::SpawnType::Archer);
  set_hold(archer, true);

  auto* hold = archer->get_component<HoldModeComponent>();
  ASSERT_NE(hold, nullptr);
  advance(hold->kneel_duration * 0.5F);
  float const partial_progress = hold->kneel_entry_progress;
  ASSERT_LT(partial_progress, 1.0F);

  set_hold(archer, false);

  EXPECT_FALSE(hold->active);
  EXPECT_FLOAT_EQ(hold->exit_cooldown, hold->stand_up_duration * partial_progress);
  EXPECT_LT(hold->exit_cooldown, hold->stand_up_duration);
}

TEST_F(HoldModeTest, RepeatedTogglingReentersFromAStandingStart) {
  auto* spearman = spawn(Game::Units::SpawnType::Spearman);

  for (int cycle = 0; cycle < 3; ++cycle) {
    set_hold(spearman, true);
    auto* hold = spearman->get_component<HoldModeComponent>();
    ASSERT_NE(hold, nullptr) << "cycle " << cycle;
    EXPECT_TRUE(hold->active) << "cycle " << cycle;
    EXPECT_FLOAT_EQ(hold->exit_cooldown, 0.0F) << "cycle " << cycle;
    EXPECT_FLOAT_EQ(hold->kneel_entry_progress, 0.0F) << "cycle " << cycle;

    advance(2.0F);
    EXPECT_FLOAT_EQ(hold->kneel_entry_progress, 1.0F) << "cycle " << cycle;

    set_hold(spearman, false);
    EXPECT_FALSE(hold->active) << "cycle " << cycle;
    EXPECT_GT(hold->exit_cooldown, 0.0F) << "cycle " << cycle;

    advance(hold->stand_up_duration + 0.2F);
    EXPECT_FLOAT_EQ(hold->exit_cooldown, 0.0F) << "cycle " << cycle;
  }
}

TEST_F(HoldModeTest, ReenteringMidStandUpClearsTheLeftoverExitCooldown) {
  auto* archer = spawn(Game::Units::SpawnType::Archer);
  set_hold(archer, true);
  advance(2.0F);

  auto* hold = archer->get_component<HoldModeComponent>();
  ASSERT_NE(hold, nullptr);

  set_hold(archer, false);
  advance(hold->stand_up_duration * 0.4F);
  ASSERT_GT(hold->exit_cooldown, 0.0F);

  set_hold(archer, true);

  EXPECT_TRUE(hold->active);
  EXPECT_FLOAT_EQ(hold->exit_cooldown, 0.0F);
  EXPECT_FLOAT_EQ(hold->kneel_entry_progress, 0.0F);
}

TEST_F(HoldModeTest, StandUpKeepsTheUnitPlantedUntilItFinishes) {
  auto* spearman = spawn(Game::Units::SpawnType::Spearman);
  set_hold(spearman, true);
  advance(2.0F);

  auto const* transform = spearman->get_component<TransformComponent>();
  ASSERT_NE(transform, nullptr);
  float const start_x = transform->position.x;

  CommandService::move_unit(*world, spearman->get_id(), QVector3D(10.0F, 0.0F, 0.0F));
  auto* hold = spearman->get_component<HoldModeComponent>();
  ASSERT_NE(hold, nullptr);

  advance(hold->stand_up_duration * 0.5F);
  EXPECT_NEAR(transform->position.x, start_x, 0.001F);
  EXPECT_GT(hold->exit_cooldown, 0.0F);

  advance(hold->stand_up_duration * 0.6F);
  ASSERT_FLOAT_EQ(hold->exit_cooldown, 0.0F);

  float const released_x = transform->position.x;
  advance(3.0F);
  EXPECT_GT(transform->position.x, released_x + 0.5F);
}

} // namespace
