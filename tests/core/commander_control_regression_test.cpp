#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

auto find_repo_root() -> std::filesystem::path {
  auto path = std::filesystem::current_path();
  while (!path.empty()) {
    if (std::filesystem::exists(path / "todo.md") &&
        std::filesystem::exists(path / "render" / "scene_renderer.cpp")) {
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

auto read_text(const std::filesystem::path &path) -> std::string {
  std::ifstream input(path);
  if (!input.is_open()) {
    return {};
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

auto contains(const std::string &text, const std::string &needle) -> bool {
  return text.find(needle) != std::string::npos;
}

} // namespace

TEST(CommanderControlRegressionTest, CommanderStrafeUsesRightHandedBasis) {
  const auto root = find_repo_root();
  const auto source =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(
      source, "const QVector3D right(-forward.z(), 0.0F, forward.x());"));
}

TEST(CommanderControlRegressionTest, CommanderMouseLookIsPolledInEngine) {
  const auto root = find_repo_root();
  const auto engine_source =
      read_text(root / "app" / "core" / "game_engine.cpp");
  const auto controller_source =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  ASSERT_FALSE(engine_source.empty());
  ASSERT_FALSE(controller_source.empty());

  EXPECT_TRUE(
      contains(engine_source, "void GameEngine::poll_commander_mouse_look()"));
  EXPECT_TRUE(contains(engine_source, "poll_commander_mouse_look();"));
  EXPECT_TRUE(contains(controller_source,
                       "void CommanderControlController::poll_mouse_look"));
  EXPECT_TRUE(contains(controller_source, "mouse_move(delta.x(), delta.y());"));
}

TEST(CommanderControlRegressionTest,
     CommanderInputLayerLeavesMouseLookToEnginePolling) {
  const auto root = find_repo_root();
  const auto source =
      read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  ASSERT_FALSE(source.empty());

  EXPECT_FALSE(contains(source, "commander_mouse_look_at"));
  EXPECT_TRUE(contains(source, "root.gameView.forceActiveFocus();"));
}

TEST(CommanderControlRegressionTest, GameViewRestoresInputFocusAcrossModes) {
  const auto root = find_repo_root();
  const auto source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto commander_source =
      read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  ASSERT_FALSE(source.empty());
  ASSERT_FALSE(commander_source.empty());

  EXPECT_TRUE(contains(source, "Keys.onPressed: function(event)"));
  EXPECT_TRUE(contains(source, "function handle_commander_key_pressed(event)"));
  EXPECT_TRUE(contains(source, "function is_commander_mode()"));
  EXPECT_TRUE(
      contains(source, "game.commander_key_down(event.key, event.modifiers)"));
  EXPECT_TRUE(contains(source, "game.toggle_commander_control_mode()"));
  EXPECT_TRUE(contains(source, "function onControl_mode_changed()"));
  EXPECT_TRUE(contains(source, "game_view.forceActiveFocus();"));

  EXPECT_TRUE(contains(source, "game.commander_input"));
  EXPECT_TRUE(contains(commander_source, "property var commanderInput"));
  EXPECT_FALSE(contains(commander_source, "root.game.commander_"));
}

TEST(CommanderControlRegressionTest, CommanderRallyKeyIsWiredThroughAdapter) {
  const auto root = find_repo_root();
  const auto layer_source =
      read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  const auto adapter_header =
      read_text(root / "app" / "core" / "commander_input_adapter.h");
  const auto adapter_source =
      read_text(root / "app" / "core" / "commander_input_adapter.cpp");
  const auto engine_header = read_text(root / "app" / "core" / "game_engine.h");
  ASSERT_FALSE(layer_source.empty());
  ASSERT_FALSE(adapter_header.empty());
  ASSERT_FALSE(adapter_source.empty());
  ASSERT_FALSE(engine_header.empty());

  EXPECT_TRUE(contains(layer_source, "case Qt.Key_R:"));
  EXPECT_TRUE(contains(layer_source, "root.commanderInput.trigger_rally()"));
  EXPECT_TRUE(contains(adapter_header, "Q_INVOKABLE void trigger_rally();"));
  EXPECT_TRUE(contains(adapter_source, "m_engine->commander_trigger_rally();"));
  EXPECT_TRUE(
      contains(engine_header, "Q_INVOKABLE void commander_trigger_rally();"));
}

TEST(CommanderControlRegressionTest, CommanderCameraUsesChaseOffsetView) {
  const auto root = find_repo_root();
  const auto source =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(
      contains(source, "constexpr float k_camera_back_offset = 2.15F;"));
  EXPECT_TRUE(contains(source, "const QVector3D flat_forward("));
  EXPECT_TRUE(contains(source, "pivot - flat_forward * k_camera_back_offset"));
  EXPECT_TRUE(contains(
      source, "camera.look_at(eye, pivot + forward * k_target_distance,"));
}

TEST(CommanderControlRegressionTest,
     CommanderModePreservesAndRestoresRtsSelection) {
  const auto root = find_repo_root();
  const auto engine_header = read_text(root / "app" / "core" / "game_engine.h");
  const auto engine_source =
      read_text(root / "app" / "core" / "game_engine.cpp");
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  ASSERT_FALSE(engine_header.empty());
  ASSERT_FALSE(engine_source.empty());
  ASSERT_FALSE(game_view_source.empty());

  EXPECT_TRUE(contains(
      engine_header,
      "std::vector<Engine::Core::EntityID> m_saved_rts_selection_ids;"));
  EXPECT_TRUE(
      contains(engine_source, "void GameEngine::store_rts_selection()"));
  EXPECT_TRUE(contains(engine_source,
                       "void GameEngine::select_controlled_commander()"));
  EXPECT_TRUE(
      contains(engine_source, "void GameEngine::restore_rts_selection()"));
  EXPECT_TRUE(contains(engine_source, "store_rts_selection();"));
  EXPECT_TRUE(contains(engine_source, "select_controlled_commander();"));
  EXPECT_TRUE(contains(engine_source, "restore_rts_selection();"));
  EXPECT_TRUE(contains(engine_source,
                       "m_selection_controller->select_single_unit("
                       "m_controlled_commander_id,"));
  EXPECT_TRUE(contains(game_view_source, "game_view.set_rally_mode = false;"));
}

TEST(CommanderControlRegressionTest, SaveAndLoadForceCommanderModeBackToRts) {
  const auto root = find_repo_root();
  const auto engine_source =
      read_text(root / "app" / "core" / "game_engine.cpp");
  ASSERT_FALSE(engine_source.empty());

  EXPECT_TRUE(contains(engine_source,
                       "if (m_control_mode == PlayerControlMode::Commander) {\n"
                       "    exit_commander_control_mode();\n"
                       "  }\n\n"
                       "  reset_preload_interaction_state();"));
  EXPECT_TRUE(contains(engine_source,
                       "if (m_control_mode == PlayerControlMode::Commander) {\n"
                       "    exit_commander_control_mode();\n"
                       "  }\n"
                       "  Game::Systems::RuntimeSnapshot const runtime_snap = "
                       "to_runtime_snapshot();"));
}

TEST(CommanderControlRegressionTest,
     HUDUsesSeparateBottomPanelsForRtsAndCommanderModes) {
  const auto root = find_repo_root();
  const auto hud_source = read_text(root / "ui" / "qml" / "HUD.qml");
  const auto commander_hud_source =
      read_text(root / "ui" / "qml" / "HUDBottomCommander.qml");
  const auto qrc_source = read_text(root / "qml_resources.qrc");
  const auto engine_header = read_text(root / "app" / "core" / "game_engine.h");
  ASSERT_FALSE(hud_source.empty());
  ASSERT_FALSE(commander_hud_source.empty());
  ASSERT_FALSE(qrc_source.empty());
  ASSERT_FALSE(engine_header.empty());

  EXPECT_TRUE(contains(hud_source, "Loader {"));
  EXPECT_TRUE(contains(
      hud_source,
      "sourceComponent: typeof game !== 'undefined' && game.control_mode === "
      "\"commander\" ? commanderBottomHudComponent : rtsBottomHudComponent"));
  EXPECT_TRUE(contains(hud_source, "HUDBottomCommander {"));
  EXPECT_TRUE(contains(qrc_source, "ui/qml/HUDBottomCommander.qml"));
  EXPECT_TRUE(
      contains(commander_hud_source, "game.get_controlled_commander_status"));
  EXPECT_TRUE(contains(commander_hud_source, "game.commander_trigger_rally"));
  EXPECT_TRUE(contains(engine_header,
                       "Q_INVOKABLE [[nodiscard]] QVariantMap "
                       "get_controlled_commander_status() const;"));
}
