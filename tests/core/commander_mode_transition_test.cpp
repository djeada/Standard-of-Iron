#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "app/commander/commander_control_controller.h"
#include "app/commander/commander_defence.h"
#include "app/commander/commander_mode_coordinator.h"
#include "game/core/component_commander.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/commander_system.h"
#include "game/systems/stamina_system.h"
#include "scene/camera.h"

namespace {

auto find_repo_root() -> std::filesystem::path {
  auto path = std::filesystem::path(__FILE__).parent_path();
  while (!path.empty()) {
    if (std::filesystem::exists(path / "CMakeLists.txt") &&
        std::filesystem::exists(path / "ui" / "qml" / "GameView.qml")) {
      return path;
    }
    const auto parent = path.parent_path();
    if (parent == path) {
      break;
    }
    path = parent;
  }
  return std::filesystem::current_path();
}

auto read_text(const std::filesystem::path& path) -> std::string {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

auto contains(const std::string& text, const std::string& needle) -> bool {
  return text.find(needle) != std::string::npos;
}

using App::Core::CommanderModeCoordinator;
using App::Core::CommanderModeState;

TEST(CommanderModeTransitionTest, EnteringIsAThreeStepTransitionOwnedByTheCoordinator) {
  CommanderModeCoordinator coordinator;
  EXPECT_EQ(coordinator.state(), CommanderModeState::Inactive);
  EXPECT_FALSE(coordinator.is_active());
  EXPECT_FALSE(coordinator.is_transitioning());

  ASSERT_TRUE(coordinator.begin_enter());
  EXPECT_EQ(coordinator.state(), CommanderModeState::Entering);
  EXPECT_FALSE(coordinator.is_active());
  EXPECT_TRUE(coordinator.is_transitioning());

  coordinator.complete_transition();
  EXPECT_EQ(coordinator.state(), CommanderModeState::Active);
  EXPECT_TRUE(coordinator.is_active());
  EXPECT_FALSE(coordinator.is_transitioning());
}

TEST(CommanderModeTransitionTest, ASecondEnterCannotStartWhileOneIsUnderway) {
  CommanderModeCoordinator coordinator;
  ASSERT_TRUE(coordinator.begin_enter());
  EXPECT_FALSE(coordinator.begin_enter());
  coordinator.complete_transition();
  EXPECT_FALSE(coordinator.begin_enter());
}

TEST(CommanderModeTransitionTest, ExitOnlyStartsFromAnActiveMode) {
  CommanderModeCoordinator coordinator;
  EXPECT_FALSE(coordinator.begin_exit());

  ASSERT_TRUE(coordinator.begin_enter());
  EXPECT_FALSE(coordinator.begin_exit());
  coordinator.complete_transition();

  ASSERT_TRUE(coordinator.begin_exit());
  EXPECT_EQ(coordinator.state(), CommanderModeState::Exiting);
  coordinator.complete_transition();
  EXPECT_EQ(coordinator.state(), CommanderModeState::Inactive);
}

TEST(CommanderModeTransitionTest, AnAbortedEntryLeavesTheModeInactive) {
  CommanderModeCoordinator coordinator;
  ASSERT_TRUE(coordinator.begin_enter());
  coordinator.abort_transition();
  EXPECT_EQ(coordinator.state(), CommanderModeState::Inactive);

  ASSERT_TRUE(coordinator.begin_enter());
  coordinator.complete_transition();
  ASSERT_TRUE(coordinator.begin_exit());
  coordinator.abort_transition();
  EXPECT_EQ(coordinator.state(), CommanderModeState::Active);
}

TEST(CommanderModeTransitionTest, EveryStateHasAStableName) {
  EXPECT_STREQ(App::Core::to_string(CommanderModeState::Inactive), "inactive");
  EXPECT_STREQ(App::Core::to_string(CommanderModeState::Entering), "entering");
  EXPECT_STREQ(App::Core::to_string(CommanderModeState::Active), "active");
  EXPECT_STREQ(App::Core::to_string(CommanderModeState::Exiting), "exiting");
}

TEST(CommanderModeTransitionTest, ModeSignalsAreEmittedOnceTheTransitionIsFinished) {
  const auto source =
      read_text(find_repo_root() / "app" / "viewmodels" / "commander_view_model.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(source, "if (!m_mode->begin_enter())"));
  EXPECT_TRUE(contains(source, "m_mode->complete_transition();"));
  EXPECT_TRUE(contains(source, "m_mode->abort_transition();"));
  EXPECT_TRUE(contains(source, "void CommanderViewModel::emit_mode_signal_changes"));

  EXPECT_FALSE(contains(source, R"(  m_game_mode = GameMode::Rgb;)"));
  EXPECT_FALSE(
      contains(source, "  m_game_mode = GameMode::Rts;\n  emit game_mode_changed();"));
  EXPECT_FALSE(
      contains(source, "  m_game_mode = GameMode::Rpg;\n  emit game_mode_changed();"));
}

TEST(CommanderModeTransitionTest, QmlReadsTheCoordinatorStateInsteadOfPairedFlags) {
  const auto root = find_repo_root();
  for (const char* name :
       {"GameView.qml", "HUD.qml", "Main.qml", "HUDBottomCommander.qml"}) {
    const auto source = read_text(root / "ui" / "qml" / name);
    ASSERT_FALSE(source.empty()) << name;
    EXPECT_FALSE(contains(source, R"(game.commander.control_mode === "commander")"))
        << name << " still infers commander mode from control_mode";
    EXPECT_TRUE(contains(source, R"(game.commander.mode_state === "active")") ||
                contains(source, R"(game.commander.mode_state !== "active")"))
        << name << " never reads the coordinator state";
  }
}

} // namespace

TEST(CommanderModeTransitionTest, ARallyInDirectControlNeedsNoCursorPlacement) {
  Game::Session::SessionContext session;
  auto& world = session.world();
  world.set_presentation_enabled(false);
  auto* commander = world.create_entity();
  commander->add_component<Engine::Core::TransformComponent>(3.0F, 0.0F, 4.0F);
  auto* unit =
      commander->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 12.0F);
  unit->owner_id = 1;
  auto* data = commander->add_component<Engine::Core::CommanderComponent>();
  data->fpv_controlled = true;

  CommanderModeCoordinator coordinator;
  auto const effects =
      coordinator.rally_where_the_commander_stands({.world = &world,
                                                    .controlled_commander = commander,
                                                    .local_owner_id = 1,
                                                    .commander_mode_active = true});

  EXPECT_FALSE(effects.cursor_mode.has_value())
      << "a rally in the middle of a melee freed the cursor and opened a placement "
         "mode while the wave kept cutting at the commander";
  EXPECT_TRUE(effects.reset_commander_input);
  EXPECT_FALSE(data->fpv_controlled) << "the commander plants the flag where he stands";
}

namespace {
class CommanderModeResourcesTest : public ::testing::Test {
protected:
  void SetUp() override {
    commander = session.world().create_entity();
    commander->add_component<Engine::Core::TransformComponent>();
    auto* unit = commander->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = 1;
    unit->health = 40;
    unit->max_health = 100;
    unit->spawn_type = Game::Units::SpawnType::Swordsman;
    commander->add_component<Engine::Core::MovementComponent>();
    commander->add_component<Engine::Core::CommanderComponent>();
    stamina = commander->add_component<Engine::Core::StaminaComponent>();
    guard = commander->add_component<Engine::Core::CommanderGuardComponent>();
  }
  auto context() -> App::Core::CommanderModeContext {
    return {.world = &session.world(),
            .commander = commander,
            .commander_camera = &camera,
            .commander_control = &controller,
            .controlled_commander_id = commander->get_id(),
            .local_owner_id = 1};
  }
  Game::Session::SessionContext session;
  Engine::Core::Entity* commander{};
  Engine::Core::StaminaComponent* stamina{};
  Engine::Core::CommanderGuardComponent* guard{};
  CommanderControlController controller;
  Render::GL::Camera camera;
  App::Core::CommanderModeCoordinator coordinator;
};

TEST_F(CommanderModeResourcesTest, FirstEntryDoesNotRefillASpentPool) {
  stamina->max_stamina = 100.0F;
  stamina->stamina = 12.0F;
  stamina->regen_delay_remaining = 0.6F;
  ASSERT_TRUE(coordinator.enter_commander_control_mode(context()).entered);
  EXPECT_FLOAT_EQ(stamina->stamina, 12.0F);
  EXPECT_FLOAT_EQ(stamina->regen_delay_remaining, 0.6F);
  EXPECT_GE(stamina->max_stamina, 100.0F);
}

TEST_F(CommanderModeResourcesTest, RepeatedSwitchesCannotEraseCombatDebt) {
  stamina->stamina = 17.0F;
  stamina->regen_delay_remaining = 0.6F;
  guard->guard_break_remaining = 0.8F;
  auto* data = commander->get_component<Engine::Core::CommanderComponent>();
  data->posture = 90.0F;
  data->second_wind_cooldown_remaining = 12.0F;
  for (int cycle = 0; cycle < 100; ++cycle) {
    ASSERT_TRUE(coordinator.enter_commander_control_mode(context()).entered);
    static_cast<void>(coordinator.exit_commander_control_mode(context()));
    EXPECT_FLOAT_EQ(stamina->stamina, 17.0F);
    EXPECT_FLOAT_EQ(stamina->regen_delay_remaining, 0.6F);
    EXPECT_FLOAT_EQ(guard->guard_break_remaining, 0.8F);
    EXPECT_FLOAT_EQ(data->posture, 90.0F);
    EXPECT_FLOAT_EQ(data->second_wind_cooldown_remaining, 12.0F);
    EXPECT_EQ(commander->get_component<Engine::Core::UnitComponent>()->health, 40);
  }
}

TEST_F(CommanderModeResourcesTest, SwitchingDoesNotStartStaminaRecoveryEarly) {
  stamina->stamina = 40.0F;
  stamina->spend(20.0F);
  ASSERT_TRUE(coordinator.enter_commander_control_mode(context()).entered);
  Game::Systems::StaminaSystem system;
  system.update(&session.world(), 0.25F);
  EXPECT_FLOAT_EQ(stamina->stamina, 20.0F);
  EXPECT_FLOAT_EQ(stamina->regen_delay_remaining, 0.5F);
  static_cast<void>(coordinator.exit_commander_control_mode(context()));
  system.update(&session.world(), 0.25F);
  ASSERT_TRUE(coordinator.enter_commander_control_mode(context()).entered);
  EXPECT_FLOAT_EQ(stamina->stamina, 20.0F);
  EXPECT_FLOAT_EQ(stamina->regen_delay_remaining, 0.25F);
  system.update(&session.world(), 0.3F);
  EXPECT_GT(stamina->stamina, 20.0F);
}

TEST_F(CommanderModeResourcesTest, AHandoffLeavesNoAirborneStateToCutTheNextJump) {
  auto* data = commander->get_component<Engine::Core::CommanderComponent>();
  for (bool const entering : {true, false}) {
    data->jump_active = true;
    data->jump_height_offset = 0.4F;
    data->airborne_velocity = -11.0F;
    data->dive_attack_active = true;
    if (entering) {
      ASSERT_TRUE(coordinator.enter_commander_control_mode(context()).entered);
    } else {
      static_cast<void>(coordinator.exit_commander_control_mode(context()));
    }
    EXPECT_FALSE(data->jump_active) << "entering=" << entering;
    EXPECT_FLOAT_EQ(data->jump_height_offset, 0.0F) << "entering=" << entering;
    EXPECT_FLOAT_EQ(data->airborne_velocity, 0.0F) << "entering=" << entering;
    EXPECT_FALSE(data->dive_attack_active)
        << "a stale dive shortens the next jump; entering=" << entering;
  }
}

TEST_F(CommanderModeResourcesTest, GuardBreakRecoveryHasOneSimulationOwnerInBothModes) {
  Game::Systems::CommanderSystem system;
  App::Core::CommanderDefence defence;
  auto* data = commander->get_component<Engine::Core::CommanderComponent>();
  for (bool controlled : {false, true}) {
    data->fpv_controlled = controlled;
    guard->guard_break_remaining = 0.5F;
    if (controlled) {
      defence.advance_upkeep(*commander, data, false, 0.1F);
    }
    // Controller upkeep cannot advance combat recovery independently of the world.
    EXPECT_FLOAT_EQ(guard->guard_break_remaining, 0.5F);
    system.update(&session.world(), 0.1F);
    EXPECT_NEAR(guard->guard_break_remaining, 0.4F, 1.0e-6F);
    defence.apply_guard(*commander, {.guard_held = true, .dt = 0.0F}, nullptr);
    EXPECT_FALSE(guard->active);
    system.update(&session.world(), 0.5F);
    EXPECT_FLOAT_EQ(guard->guard_break_remaining, 0.0F);
    defence.apply_guard(*commander, {.guard_held = true, .dt = 0.0F}, nullptr);
    EXPECT_TRUE(guard->active);
    defence.apply_guard(*commander, {.guard_held = false, .dt = 0.0F}, nullptr);
  }
}
} // namespace
