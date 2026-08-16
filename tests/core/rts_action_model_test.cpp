#include <gtest/gtest.h>

#include "app/input/cursor_mode.h"
#include "app/orders/rts_action_model.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/selection_system.h"

namespace {

auto add_selected_unit(Engine::Core::World& world,
                       Game::Systems::SelectionSystem& selection,
                       Game::Units::SpawnType spawn_type) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = spawn_type;
  selection.select_unit(entity->get_id());
  return entity;
}

TEST(RtsActionModel, EngagedUnitDoesNotReportAttackCommandModeWithNormalCursor) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* unit = add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);
  unit->add_component<Engine::Core::AttackTargetComponent>()->target_id = 42U;

  App::Core::ActionContext context;
  context.world = &world;
  context.cursor_mode = CursorMode::Normal;

  EXPECT_EQ(App::Core::get_current_action_mode(context), QStringLiteral("normal"));
}

TEST(RtsActionModel, AttackCursorModeReportsAttackCommandMode) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);

  App::Core::ActionContext context;
  context.world = &world;
  context.cursor_mode = CursorMode::Attack;

  EXPECT_EQ(App::Core::get_current_action_mode(context), QStringLiteral("attack"));
}

TEST(RtsActionModel, GuardModeStillReportsGuardCommandMode) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* unit = add_selected_unit(world, *selection, Game::Units::SpawnType::Spearman);
  unit->add_component<Engine::Core::GuardModeComponent>()->active = true;

  App::Core::ActionContext context;
  context.world = &world;
  context.cursor_mode = CursorMode::Normal;

  EXPECT_EQ(App::Core::get_current_action_mode(context), QStringLiteral("guard"));
}

TEST(RtsActionModel, SelectedCommanderAuraReflectsReadyActiveAndCooldownStates) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* entity =
      add_selected_unit(world, *selection, Game::Units::SpawnType::RomanVeteranConsul);
  auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander, nullptr);

  App::Core::ActionContext context;
  context.world = &world;
  auto aura = App::Core::get_action_states(context)[QStringLiteral("aura")].toMap();
  EXPECT_TRUE(aura[QStringLiteral("enabled")].toBool());
  EXPECT_FALSE(aura[QStringLiteral("active")].toBool());

  commander->aura_ability_active = true;
  commander->aura_ability_remaining = 3.0F;
  aura = App::Core::get_action_states(context)[QStringLiteral("aura")].toMap();
  EXPECT_FALSE(aura[QStringLiteral("enabled")].toBool());
  EXPECT_TRUE(aura[QStringLiteral("active")].toBool());

  commander->aura_ability_active = false;
  commander->aura_ability_remaining = 0.0F;
  commander->aura_ability_cooldown_remaining = 8.0F;
  aura = App::Core::get_action_states(context)[QStringLiteral("aura")].toMap();
  EXPECT_FALSE(aura[QStringLiteral("enabled")].toBool());
  EXPECT_FALSE(aura[QStringLiteral("active")].toBool());
}

TEST(RtsActionModel, RepairIsOfferedToBuildersAndNobodyElse) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);

  App::Core::ActionContext context;
  context.world = &world;

  EXPECT_FALSE(App::Core::get_action_states(context)
                   .value(QStringLiteral("repair"))
                   .toMap()
                   .value(QStringLiteral("enabled"))
                   .toBool());

  auto* builder = add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);
  builder->add_component<Engine::Core::BuilderProductionComponent>();

  EXPECT_TRUE(App::Core::get_action_states(context)
                  .value(QStringLiteral("repair"))
                  .toMap()
                  .value(QStringLiteral("enabled"))
                  .toBool());
}

TEST(RtsActionModel, ARepairingBuilderReportsTheOrderAsActive) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* builder = add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);
  auto* production = builder->add_component<Engine::Core::BuilderProductionComponent>();
  production->product_type = "repair_structure";

  App::Core::ActionContext context;
  context.world = &world;

  EXPECT_EQ(App::Core::get_toggle_state(&world, QStringLiteral("repair")),
            QStringLiteral("all"));
  EXPECT_TRUE(App::Core::get_mode_availability(&world)
                  .value(QStringLiteral("canRepair"))
                  .toBool());
}

TEST(RtsActionModel, ArmingRepairShowsUpAsTheCurrentCommandMode) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* builder = add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);
  builder->add_component<Engine::Core::BuilderProductionComponent>();

  App::Core::ActionContext context;
  context.world = &world;
  context.cursor_mode = CursorMode::Repair;

  EXPECT_EQ(App::Core::get_current_action_mode(context), QStringLiteral("repair"));
  EXPECT_EQ(App::Core::action_id_for_cursor_mode(CursorMode::Repair),
            QStringLiteral("repair"));
}

TEST(RtsActionModel, AutoGatherIsOfferedToBuildersAndNobodyElse) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);

  App::Core::ActionContext context;
  context.world = &world;

  EXPECT_FALSE(App::Core::get_action_states(context)
                   .value(QStringLiteral("auto_gather"))
                   .toMap()
                   .value(QStringLiteral("enabled"))
                   .toBool());

  auto* builder = add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);
  builder->add_component<Engine::Core::BuilderProductionComponent>();

  EXPECT_TRUE(App::Core::get_action_states(context)
                  .value(QStringLiteral("auto_gather"))
                  .toMap()
                  .value(QStringLiteral("enabled"))
                  .toBool());
  EXPECT_TRUE(App::Core::get_mode_availability(&world)
                  .value(QStringLiteral("canAutoGather"))
                  .toBool());
}

TEST(RtsActionModel, AnAutoGatheringBuilderShowsTheOrderAsActive) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* first = add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);
  first->add_component<Engine::Core::BuilderProductionComponent>()->auto_gather = true;

  EXPECT_EQ(App::Core::get_toggle_state(&world, QStringLiteral("auto_gather")),
            QStringLiteral("all"));

  auto* second = add_selected_unit(world, *selection, Game::Units::SpawnType::Builder);
  second->add_component<Engine::Core::BuilderProductionComponent>();

  EXPECT_EQ(App::Core::get_toggle_state(&world, QStringLiteral("auto_gather")),
            QStringLiteral("mixed"));
}

TEST(RtsActionModel, GuardQuotesTheRingItActuallyFightsIn) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* unit = add_selected_unit(world, *selection, Game::Units::SpawnType::Spearman);

  App::Core::ActionContext context;
  context.world = &world;

  auto detail = App::Core::get_action_states(context)[QStringLiteral("guard")]
                    .toMap()[QStringLiteral("detail")]
                    .toMap();
  EXPECT_FLOAT_EQ(detail[QStringLiteral("radius")].toFloat(),
                  Engine::Core::Defaults::k_guard_default_radius);

  auto* guard = unit->add_component<Engine::Core::GuardModeComponent>();
  guard->guard_radius = 4.5F;
  detail = App::Core::get_action_states(context)[QStringLiteral("guard")]
               .toMap()[QStringLiteral("detail")]
               .toMap();
  EXPECT_FLOAT_EQ(detail[QStringLiteral("radius")].toFloat(), 4.5F);
}

TEST(RtsActionModel, HoldQuotesTheBonusesThatMakeItWorthPressing) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);

  App::Core::ActionContext context;
  context.world = &world;

  const auto detail = App::Core::get_action_states(context)[QStringLiteral("hold")]
                          .toMap()[QStringLiteral("detail")]
                          .toMap();
  EXPECT_EQ(detail[QStringLiteral("archerRangeBonusPercent")].toInt(), 50);
  EXPECT_EQ(detail[QStringLiteral("spearmanRangeBonusPercent")].toInt(), 100);
  EXPECT_EQ(detail[QStringLiteral("damageBonusPercent")].toInt(), 50);
  EXPECT_EQ(detail[QStringLiteral("healthBonusPercent")].toInt(), 20);
}

TEST(RtsActionModel, PatrolSaysWhichWaypointComesNext) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  add_selected_unit(world, *selection, Game::Units::SpawnType::Archer);

  const auto stage = [](const App::Core::ActionContext& context) {
    return App::Core::get_action_states(context)[QStringLiteral("patrol")]
        .toMap()[QStringLiteral("detail")]
        .toMap()[QStringLiteral("waypointStage")]
        .toInt();
  };

  App::Core::ActionContext context;
  context.world = &world;
  EXPECT_EQ(stage(context), 0);

  context.cursor_mode = CursorMode::Patrol;
  EXPECT_EQ(stage(context), 1);

  context.has_patrol_first_waypoint = true;
  EXPECT_EQ(stage(context), 2);
}

TEST(RtsActionModel, AuraQuotesTheSelectedCommandersOwnNumbers) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());
  auto* selection = world.get_system<Game::Systems::SelectionSystem>();
  ASSERT_NE(selection, nullptr);

  auto* entity =
      add_selected_unit(world, *selection, Game::Units::SpawnType::RomanVeteranConsul);
  auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander, nullptr);
  commander->aura_radius = 13.0F;
  commander->aura_ability_duration = 12.0F;
  commander->aura_ability_cooldown = 45.0F;
  commander->aura_ability_cooldown_remaining = 9.0F;
  commander->bonus_summary = "Nearby spearmen regenerate health.";

  App::Core::ActionContext context;
  context.world = &world;

  const auto detail = App::Core::get_action_states(context)[QStringLiteral("aura")]
                          .toMap()[QStringLiteral("detail")]
                          .toMap();
  EXPECT_FLOAT_EQ(detail[QStringLiteral("radius")].toFloat(), 13.0F);
  EXPECT_FLOAT_EQ(detail[QStringLiteral("duration")].toFloat(), 12.0F);
  EXPECT_FLOAT_EQ(detail[QStringLiteral("cooldown")].toFloat(), 45.0F);
  EXPECT_FLOAT_EQ(detail[QStringLiteral("cooldownRemaining")].toFloat(), 9.0F);
  EXPECT_EQ(detail[QStringLiteral("summary")].toString(),
            QStringLiteral("Nearby spearmen regenerate health."));
}

TEST(RtsActionModel, AnEmptySelectionStillCarriesTheStaticOrderFacts) {
  Engine::Core::World world;
  world.add_system(std::make_unique<Game::Systems::SelectionSystem>());

  App::Core::ActionContext context;
  context.world = &world;

  const auto states = App::Core::get_action_states(context);
  EXPECT_FALSE(states[QStringLiteral("guard")]
                   .toMap()[QStringLiteral("detail")]
                   .toMap()
                   .isEmpty());
  EXPECT_TRUE(states[QStringLiteral("aura")]
                  .toMap()[QStringLiteral("detail")]
                  .toMap()
                  .isEmpty());
}

} // namespace
