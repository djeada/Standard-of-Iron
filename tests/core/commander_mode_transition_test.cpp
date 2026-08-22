#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "app/commander/commander_mode_coordinator.h"

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
