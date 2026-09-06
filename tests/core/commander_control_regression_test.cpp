#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>

namespace {

auto find_repo_root() -> std::filesystem::path {
  auto has_repo_markers = [](const std::filesystem::path& path) {
    return std::filesystem::exists(path / "CMakeLists.txt") &&
           std::filesystem::exists(path / "app" / "core" / "game_engine.cpp") &&
           std::filesystem::exists(path / "ui" / "qml" / "GameView.qml");
  };

  auto walk_up = [&](std::filesystem::path path) -> std::filesystem::path {
    while (!path.empty()) {
      if (has_repo_markers(path)) {
        return path;
      }
      const auto parent = path.parent_path();
      if (parent == path) {
        break;
      }
      path = parent;
    }
    return {};
  };

  if (const auto from_file = walk_up(std::filesystem::path(__FILE__).parent_path());
      !from_file.empty()) {
    return from_file;
  }
  if (const auto from_cwd = walk_up(std::filesystem::current_path());
      !from_cwd.empty()) {
    return from_cwd;
  }
  return std::filesystem::current_path();
}

auto read_text(const std::filesystem::path& path) -> std::string {
  std::ifstream input(path);
  if (!input.is_open()) {
    return {};
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

auto app_source(const std::filesystem::path& root,
                std::string_view name) -> std::string {
  const auto app = root / "app";
  if (!std::filesystem::is_directory(app)) {
    return {};
  }
  for (const auto& entry : std::filesystem::recursive_directory_iterator(app)) {
    if (entry.is_regular_file() && entry.path().filename() == name) {
      return read_text(entry.path());
    }
  }
  return {};
}

auto contains(const std::string& text, const std::string& needle) -> bool {
  return text.find(needle) != std::string::npos;
}

auto occurrences(const std::string& text, const std::string& needle) -> int {
  if (needle.empty()) {
    return 0;
  }
  int count = 0;
  for (std::size_t pos = text.find(needle); pos != std::string::npos;
       pos = text.find(needle, pos + needle.size())) {
    ++count;
  }
  return count;
}

} // namespace

TEST(CommanderControlRegressionTest, CommanderStrafeUsesRightHandedBasis) {
  const auto root = find_repo_root();
  const auto source = app_source(root, "commander_control_controller.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(
      contains(source, "const QVector3D right(-forward.z(), 0.0F, forward.x());"));
}

TEST(CommanderControlRegressionTest, CommanderMouseLookIsPolledEveryRenderedFrame) {
  const auto root = find_repo_root();
  const auto engine_source = app_source(root, "commander_view_model.cpp");
  const auto controller_source = app_source(root, "commander_control_controller.cpp");
  const auto game_engine_source = app_source(root, "game_engine.cpp");
  ASSERT_FALSE(engine_source.empty());
  ASSERT_FALSE(controller_source.empty());
  ASSERT_FALSE(game_engine_source.empty());

  EXPECT_TRUE(contains(engine_source,
                       "void CommanderViewModel::update_control_mode(float dt)"));
  EXPECT_TRUE(
      contains(engine_source, "m_control.sample_frame_intent(m_context.window);"));
  EXPECT_FALSE(contains(engine_source, "m_control.poll_mouse_look(m_context.window);"));
  EXPECT_TRUE(
      contains(game_engine_source, "m_commander_view_model->sample_frame_intent();"));
  EXPECT_TRUE(
      contains(controller_source, "void CommanderControlController::poll_mouse_look"));
  EXPECT_TRUE(contains(controller_source, "mouse_move(delta.x(), delta.y());"));
}

TEST(CommanderControlRegressionTest,
     CommanderInputLayerLeavesMouseLookToEnginePolling) {
  const auto root = find_repo_root();
  const auto source = read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  ASSERT_FALSE(source.empty());

  EXPECT_FALSE(contains(source, "commander_mouse_look_at"));
  EXPECT_TRUE(contains(source, "root.gameView.forceActiveFocus();"));
}

TEST(CommanderControlRegressionTest, CommanderInputLayerReleasesEveryHeldInputAtOnce) {
  const auto root = find_repo_root();
  const auto source = read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  const auto view_model_source = app_source(root, "commander_view_model.cpp");
  ASSERT_FALSE(source.empty());
  ASSERT_FALSE(view_model_source.empty());

  EXPECT_TRUE(contains(source, "root.commander.release_input();"));
  EXPECT_TRUE(contains(source, "onActiveChanged"));
  EXPECT_TRUE(contains(source, "onMenu_visibleChanged"));
  EXPECT_TRUE(contains(view_model_source, "void CommanderViewModel::release_input()"));
  EXPECT_TRUE(contains(view_model_source, "m_control.release_all_input();"));
  EXPECT_FALSE(contains(view_model_source, "m_control.primary_action("));
  EXPECT_FALSE(contains(view_model_source, "m_control.release_guard("));
}

TEST(CommanderControlRegressionTest, GameViewRestoresInputFocusAcrossModes) {
  const auto root = find_repo_root();
  const auto source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto commander_source =
      read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  ASSERT_FALSE(source.empty());
  ASSERT_FALSE(commander_source.empty());

  EXPECT_TRUE(contains(source, "Keys.onPressed: function(event)") ||
              contains(source, "Keys.onPressed: function (event)"));
  EXPECT_TRUE(contains(source, "function perform_commander_action(actionId, event)"));
  EXPECT_TRUE(contains(source, "function is_commander_mode()"));
  EXPECT_TRUE(contains(source, "function input_context()"));

  EXPECT_TRUE(contains(source, "game.commander.key_down(canonical, event.modifiers)"));
  EXPECT_TRUE(contains(source, "InputBindings.canonical_key_for(actionId)"));
  EXPECT_TRUE(contains(source, "game.commander.toggle_mode()"));
  EXPECT_TRUE(contains(source, "function onControl_mode_changed()"));
  EXPECT_TRUE(contains(source, "game_view.forceActiveFocus();"));

  EXPECT_TRUE(contains(source, "game.commander"));
  EXPECT_TRUE(contains(commander_source, "property var commander"));
  EXPECT_FALSE(contains(commander_source, "root.game.commander_"));
}

TEST(CommanderControlRegressionTest, OnlyCursorLayerReplacesThePointer) {
  const auto root = find_repo_root();
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto cursor_layer_source = read_text(root / "ui" / "qml" / "CursorLayer.qml");
  const auto cursor_manager_header = app_source(root, "cursor_manager.h");
  ASSERT_FALSE(game_view_source.empty());
  ASSERT_FALSE(cursor_layer_source.empty());
  ASSERT_FALSE(cursor_manager_header.empty());

  EXPECT_FALSE(
      std::filesystem::exists(root / "ui" / "qml" / "ContextIntentPreview.qml"));
  EXPECT_FALSE(std::filesystem::exists(root / "ui" / "qml" / "CursorManager.qml"));

  EXPECT_TRUE(contains(cursor_layer_source, "readonly property bool replacesPointer"));
  EXPECT_TRUE(contains(
      game_view_source,
      "cursorShape: cursorLayer.replacesPointer ? Qt.BlankCursor : Qt.ArrowCursor"));
  EXPECT_FALSE(contains(game_view_source, "Qt.CrossCursor"));

  EXPECT_FALSE(contains(cursor_manager_header, "update_cursor_shape"));

  EXPECT_EQ(occurrences(game_view_source, "cursorShape:"), 1);
}

TEST(CommanderControlRegressionTest, OrderFeedbackSpeaksThroughOneCursorChip) {
  const auto root = find_repo_root();
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto cursor_layer_source = read_text(root / "ui" / "qml" / "CursorLayer.qml");
  const auto context_intent_header = app_source(root, "context_intent.h");
  ASSERT_FALSE(game_view_source.empty());
  ASSERT_FALSE(cursor_layer_source.empty());
  ASSERT_FALSE(context_intent_header.empty());

  EXPECT_TRUE(contains(game_view_source,
                       "cursorLayer.report_order_feedback(kind, accepted, message);"));
  EXPECT_TRUE(contains(cursor_layer_source, "readonly property string source:"));

  EXPECT_FALSE(contains(game_view_source, "orderFeedbackBanner"));
  EXPECT_FALSE(contains(game_view_source, "attackTargetHint"));
  EXPECT_FALSE(contains(game_view_source, "interactionTargetHint"));

  EXPECT_TRUE(contains(context_intent_header, "ContextIntent::None"));
  EXPECT_TRUE(contains(context_intent_header, "auto advises() const -> bool"));
}

TEST(CommanderControlRegressionTest, GameViewRoutesRightGestureThroughTheOrdersSlice) {
  const auto root = find_repo_root();
  const auto source = read_text(root / "ui" / "qml" / "GameView.qml");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(source, "game.orders.on_right_press(mouse.x, mouse.y);"));
  EXPECT_TRUE(contains(source, "game.orders.on_right_move(mouse.x, mouse.y);"));
  EXPECT_TRUE(contains(source, "game.orders.on_right_release(mouse.x, mouse.y);"));
  EXPECT_TRUE(contains(source, "game.orders.on_right_double_click(mouse.x, mouse.y);"));
}

TEST(CommanderControlRegressionTest, GameViewNeverWritesTheDerivedEdgeScrollFlag) {

  const auto root = find_repo_root();
  const auto main_source = read_text(root / "ui" / "qml" / "Main.qml");
  const auto source = read_text(root / "ui" / "qml" / "GameView.qml");
  ASSERT_FALSE(main_source.empty());
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(main_source, "readonly property bool edge_scroll_disabled"));
  EXPECT_FALSE(contains(source, "edge_scroll_disabled ="));
  EXPECT_FALSE(contains(source, "mainWindow.edge_scroll_disabled"));
}

TEST(CommanderControlRegressionTest,
     GameViewDoesNotKeepLocalRightGestureSuppressionState) {
  const auto root = find_repo_root();
  const auto source = read_text(root / "ui" / "qml" / "GameView.qml");
  ASSERT_FALSE(source.empty());

  EXPECT_FALSE(contains(source, "property bool is_right_drag_orient"));
  EXPECT_FALSE(contains(source, "property bool right_dragged"));
  EXPECT_FALSE(contains(source, "property bool suppress_right_release_click"));
}

TEST(CommanderControlRegressionTest,
     GameEngineInitializesRtsRuntimeStateDuringConstruction) {
  const auto root = find_repo_root();

  const auto engine_source = app_source(root, "game_engine.cpp") +
                             app_source(root, "game_engine_composition.cpp");
  const auto commander_source = app_source(root, "commander_view_model.cpp");
  ASSERT_FALSE(engine_source.empty());
  ASSERT_FALSE(commander_source.empty());

  EXPECT_TRUE(contains(engine_source, "apply_game_mode_render_policy();"));
  EXPECT_TRUE(
      contains(commander_source, "void CommanderViewModel::enter_rts_runtime_mode()"));
  EXPECT_FALSE(contains(engine_source, "class GameEngine::RuntimeMode"));
  EXPECT_FALSE(contains(engine_source, "m_control_mode_toggle"));
}

TEST(CommanderControlRegressionTest,
     OrdersLetDoubleRightClickOverridePressStartedFormationPlacement) {
  const auto root = find_repo_root();
  const auto header = app_source(root, "orders_view_model.h");
  const auto source = app_source(root, "orders_view_model.cpp");
  ASSERT_FALSE(header.empty());
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(header, "bool placement_was_active_on_press = false;"));
  EXPECT_TRUE(contains(header, "bool started_formation_placement = false;"));
  EXPECT_TRUE(contains(source, "if (m_right_mouse.placement_was_active_on_press)"));
  EXPECT_TRUE(contains(source, "if (started_formation_placement) {"));
  EXPECT_TRUE(contains(source, "input->on_formation_cancel();"));
  EXPECT_TRUE(contains(source, "m_right_mouse.started_formation_placement ="));
}

TEST(CommanderControlRegressionTest, CommanderPortraitBakesItsOwnBodyMeshes) {
  const auto root = find_repo_root();
  const auto scenes = read_text(root / "ui" / "commander_portrait_scenes.cpp");
  const auto view = read_text(root / "ui" / "commander_portrait_view.cpp");
  ASSERT_FALSE(scenes.empty());
  ASSERT_FALSE(view.empty());

  EXPECT_TRUE(contains(scenes, "render/creature/runtime_bake_guard.h"));
  EXPECT_TRUE(contains(scenes, "Render::Creature::RuntimeBakeAllowScope"));
  EXPECT_TRUE(contains(view, "render/creature/runtime_bake_guard.h"));
  EXPECT_TRUE(contains(view, "Render::Creature::RuntimeBakeAllowScope"));
}

TEST(CommanderControlRegressionTest, HiddenCommanderPortraitRendersNothing) {
  const auto root = find_repo_root();
  const auto view = read_text(root / "ui" / "commander_portrait_view.cpp");
  ASSERT_FALSE(view.empty());

  EXPECT_TRUE(contains(view, "m_speaking = view->speaking() && view->isVisible();"));
}

TEST(CommanderControlRegressionTest, CommanderPortraitScenesTakeDistinctEntityIds) {
  const auto root = find_repo_root();
  const auto scenes = read_text(root / "ui" / "commander_portrait_scenes.cpp");
  ASSERT_FALSE(scenes.empty());

  EXPECT_TRUE(contains(scenes, "entry.world->set_next_entity_id("));
}

TEST(CommanderControlRegressionTest, CommanderRallyKeyReachesTheCommanderSlice) {
  const auto root = find_repo_root();
  const auto layer_source = read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  const auto view_model_header = app_source(root, "commander_view_model.h");
  const auto view_model_source = app_source(root, "commander_view_model.cpp");
  ASSERT_FALSE(layer_source.empty());
  ASSERT_FALSE(view_model_header.empty());
  ASSERT_FALSE(view_model_source.empty());

  EXPECT_TRUE(contains(layer_source, "case \"commander.rally\":"));
  EXPECT_TRUE(contains(layer_source, "root.commander.trigger_rally()"));
  EXPECT_TRUE(contains(view_model_header, "Q_INVOKABLE void trigger_rally();"));
  EXPECT_TRUE(contains(view_model_source, "void CommanderViewModel::trigger_rally()"));
}

TEST(CommanderControlRegressionTest, CommanderAuraKeyReachesTheCommanderSlice) {
  const auto root = find_repo_root();
  const auto layer_source = read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  const auto view_model_header = app_source(root, "commander_view_model.h");
  const auto view_model_source = app_source(root, "commander_view_model.cpp");

  EXPECT_TRUE(contains(layer_source, "case \"commander.ability_aura\":"));
  EXPECT_TRUE(contains(layer_source, "root.commander.trigger_aura()"));
  EXPECT_TRUE(contains(view_model_header, "Q_INVOKABLE void trigger_aura();"));
  EXPECT_TRUE(contains(view_model_source, "void CommanderViewModel::trigger_aura()"));
}

TEST(CommanderControlRegressionTest, CommanderCameraUsesChaseOffsetView) {
  const auto root = find_repo_root();
  const auto source = app_source(root, "commander_camera_rig.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(source, "Framing{3.10F, 1.15F, 0.90F, 6.0F, 68.0F, 0.0F}"));
  EXPECT_TRUE(contains(source, "Framing{2.25F, 1.05F, 0.72F, 5.2F, 64.0F, 0.0F}"));
  EXPECT_TRUE(contains(source, "constexpr float k_commander_near_plane = 0.05F;"));
  EXPECT_TRUE(contains(source, "QVector3D const flat_forward("));
  EXPECT_TRUE(contains(source, "pivot - flat_forward * m_framing_current.back"));

  EXPECT_TRUE(
      contains(source,
               "QVector3D const free_look_target =\n"
               "      eye_desired + forward_vec * m_framing_current.distance -"));

  EXPECT_TRUE(contains(source, "m_framing_current.look_drop * (1.0F - aim_blend)"));
  EXPECT_FALSE(contains(source, "target_desired = pivot + forward_vec"));

  EXPECT_TRUE(
      contains(source, "camera.look_at(m_eye_smooth, m_target_smooth, up_final);"));
  EXPECT_FALSE(contains(source, "shake_offset"));

  EXPECT_TRUE(contains(source, "bob_v + breath_v"));
  EXPECT_TRUE(contains(source, "flat_right * (side_offset + bob_l)"));
}

TEST(CommanderControlRegressionTest, CommanderModePreservesAndRestoresRtsSelection) {
  const auto root = find_repo_root();
  const auto view_model_header = app_source(root, "commander_view_model.h");
  const auto view_model_source = app_source(root, "commander_view_model.cpp");
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  ASSERT_FALSE(view_model_header.empty());
  ASSERT_FALSE(view_model_source.empty());
  ASSERT_FALSE(game_view_source.empty());

  EXPECT_TRUE(
      contains(view_model_header,
               "std::vector<Engine::Core::EntityID> m_saved_rts_selection_ids;"));
  EXPECT_TRUE(
      contains(view_model_source, "void CommanderViewModel::store_rts_selection()"));
  EXPECT_TRUE(contains(view_model_source,
                       "void CommanderViewModel::select_controlled_commander()"));
  EXPECT_TRUE(
      contains(view_model_source, "void CommanderViewModel::restore_rts_selection()"));
  EXPECT_TRUE(contains(view_model_source, "store_rts_selection();"));
  EXPECT_TRUE(contains(view_model_source, "select_controlled_commander();"));
  EXPECT_TRUE(contains(view_model_source, "restore_rts_selection();"));
  EXPECT_TRUE(contains(view_model_source, "m_mode->select_controlled_commander("));
  EXPECT_TRUE(contains(game_view_source, "game.commander.cancel_barracks_rally();"));
}

TEST(CommanderControlRegressionTest, BarracksRallyPlacementUsesDedicatedCursorMode) {
  const auto root = find_repo_root();
  const auto view_model_header = app_source(root, "commander_view_model.h");
  const auto cursor_mode_header = app_source(root, "cursor_mode.h");
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto hud_source = read_text(root / "ui" / "qml" / "HUDBottom.qml");
  const auto production_panel_source =
      read_text(root / "ui" / "qml" / "ProductionPanel.qml");
  ASSERT_FALSE(view_model_header.empty());
  ASSERT_FALSE(cursor_mode_header.empty());
  ASSERT_FALSE(game_view_source.empty());
  ASSERT_FALSE(hud_source.empty());
  ASSERT_FALSE(production_panel_source.empty());

  EXPECT_TRUE(contains(view_model_header, "Q_INVOKABLE void begin_barracks_rally();"));
  EXPECT_TRUE(contains(view_model_header,
                       "Q_INVOKABLE void confirm_barracks_rally(qreal sx, qreal sy);"));
  EXPECT_TRUE(contains(view_model_header, "Q_INVOKABLE void cancel_barracks_rally();"));
  EXPECT_TRUE(contains(cursor_mode_header, "PlaceBarracksRally"));
  EXPECT_TRUE(contains(cursor_mode_header, "\"place_barracks_rally\""));
  EXPECT_TRUE(contains(game_view_source, "function is_barracks_rally_placement()"));
  EXPECT_TRUE(contains(game_view_source,
                       "game.commander.confirm_barracks_rally(mouse.x, mouse.y);"));
  EXPECT_TRUE(contains(game_view_source, "game.commander.cancel_barracks_rally();"));
  EXPECT_TRUE(contains(production_panel_source,
                       "gameView.cursor_mode === \"place_barracks_rally\""));
  EXPECT_TRUE(contains(hud_source, "game.commander.begin_barracks_rally();"));
}

TEST(CommanderControlRegressionTest, SaveAndLoadForceCommanderModeBackToRts) {
  const auto root = find_repo_root();
  const auto engine_source = app_source(root, "game_engine.cpp");
  ASSERT_FALSE(engine_source.empty());

  EXPECT_TRUE(contains(engine_source,
                       "if (m_commander_view_model->active()) {\n"
                       "    m_commander_view_model->exit_mode();\n"
                       "  }\n\n"
                       "  reset_preload_interaction_state();"));
  EXPECT_TRUE(contains(engine_source,
                       "if (m_commander_view_model->active()) {\n"
                       "    m_commander_view_model->exit_mode();\n"
                       "  }\n\n"
                       "  m_save_progress_slot = slot_name;"));
}

TEST(CommanderControlRegressionTest,
     HUDUsesSeparateBottomPanelsForRtsAndCommanderModes) {
  const auto root = find_repo_root();
  const auto hud_source = read_text(root / "ui" / "qml" / "HUD.qml");
  const auto commander_hud_source =
      read_text(root / "ui" / "qml" / "HUDBottomCommander.qml");

  const auto cmake_source = read_text(root / "CMakeLists.txt");
  const auto view_model_header = app_source(root, "commander_view_model.h");
  ASSERT_FALSE(hud_source.empty());
  ASSERT_FALSE(commander_hud_source.empty());
  ASSERT_FALSE(cmake_source.empty());
  ASSERT_FALSE(view_model_header.empty());

  EXPECT_TRUE(contains(hud_source, "Loader {"));

  EXPECT_TRUE(contains(hud_source, "if (game.is_spectator_mode)"));
  EXPECT_TRUE(contains(hud_source, "return spectatorBottomHudComponent;"));
  EXPECT_TRUE(contains(
      hud_source,
      "return game.commander.mode_state === \"active\" ? commanderBottomHudComponent "
      ": rtsBottomHudComponent;"));
  EXPECT_TRUE(contains(hud_source, "HUDBottomCommander {"));
  EXPECT_TRUE(contains(hud_source, "HUDBottomSpectator {"));
  EXPECT_TRUE(contains(cmake_source, "ui/qml/HUDBottomCommander.qml"));
  EXPECT_TRUE(contains(cmake_source, "ui/qml/HUDBottomSpectator.qml"));
  EXPECT_TRUE(contains(commander_hud_source, "game.commander.status"));
  EXPECT_TRUE(contains(commander_hud_source, "game.commander.trigger_rally"));
  EXPECT_TRUE(contains(view_model_header,
                       "Q_INVOKABLE [[nodiscard]] QVariantMap status() const;"));
}

TEST(CommanderControlRegressionTest, FpvCommanderHitOverlayUsesRichDamageBurstData) {
  const auto root = find_repo_root();
  const auto view_model_source = app_source(root, "commander_view_model.cpp");
  const auto store_source = read_text(root / "app" / "world" / "world_feedback.cpp");
  const auto hud_source = read_text(root / "ui" / "qml" / "HUD.qml");
  const auto numbers_source = read_text(root / "ui" / "qml" / "FloatingNumbers.qml");
  ASSERT_FALSE(view_model_source.empty());
  ASSERT_FALSE(store_source.empty());
  ASSERT_FALSE(hud_source.empty());
  ASSERT_FALSE(numbers_source.empty());

  EXPECT_TRUE(contains(view_model_source, "return HitRouting::CommanderBurst;"))
      << "the commander view model classifies a hit; it no longer owns a second "
         "damage-number buffer of its own";
  EXPECT_FALSE(contains(view_model_source, "pop_damage_events"));

  EXPECT_TRUE(
      contains(store_source, "map[QStringLiteral(\"severity\")] = tick.severity;"));
  EXPECT_TRUE(contains(store_source, "map[QStringLiteral(\"lane\")] = tick.lane;"));
  EXPECT_TRUE(contains(store_source,
                       "map[QStringLiteral(\"killingBlow\")] = tick.killing_blow;"));
  EXPECT_TRUE(contains(store_source, "map[QStringLiteral(\"style\")]"))
      << "one store feeds both presentations, so the style has to survive the "
         "trip into QML";

  EXPECT_TRUE(contains(hud_source, "game.commander.mode_state === \"active\""));

  EXPECT_TRUE(contains(numbers_source, "readonly property real ringSize"));
  EXPECT_TRUE(contains(numbers_source, "severityRatio"));
  EXPECT_TRUE(contains(numbers_source, "killingBlow"));
}

TEST(CommanderControlRegressionTest, CommanderRpgHudUsesSingleOverlayPresentation) {
  const auto root = find_repo_root();
  const auto hud_source = read_text(root / "ui" / "qml" / "HUD.qml");
  const auto commander_hud_source =
      read_text(root / "ui" / "qml" / "HUDBottomCommander.qml");
  const auto fpv_overlay_source = read_text(root / "ui" / "qml" / "RpgFpvOverlay.qml");
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  ASSERT_FALSE(hud_source.empty());
  ASSERT_FALSE(commander_hud_source.empty());
  ASSERT_FALSE(fpv_overlay_source.empty());
  ASSERT_FALSE(game_view_source.empty());

  EXPECT_TRUE(contains(hud_source,
                       "property bool commander_rpg_mode: typeof game !== "
                       "'undefined' && game.commander.mode_state === \"active\""));
  EXPECT_TRUE(
      contains(hud_source,
               "property bool commander_rally_overlay_blocked: commander_rpg_mode && "
               "typeof game !== 'undefined' && (game.cursor_mode === "
               "\"place_commander_rally\" || game.cursor_mode === "
               "\"place_barracks_rally\")"));
  EXPECT_TRUE(contains(hud_source, "bottomInset: bottomPanel.height"));
  EXPECT_TRUE(contains(hud_source,
                       "visible: hud.commander_rpg_mode && "
                       "!hud.commander_rally_overlay_blocked"));

  EXPECT_TRUE(
      contains(game_view_source,
               "visible: typeof game !== 'undefined' && game.commander.mode_state === "
               "\"active\" && game.commander.game_mode !== \"rpg\" && "
               "!game_view.is_rally_placement()"));

  EXPECT_TRUE(contains(commander_hud_source,
                       "readonly property bool fpv_mode: typeof game !== "
                       "'undefined' && game.commander.mode_state === \"active\""));
  EXPECT_TRUE(contains(commander_hud_source,
                       "text: bottomRoot.fpv_mode ? qsTr(\"ORDERS\") : "
                       "qsTr(\"ABILITIES\")"));
  EXPECT_TRUE(contains(commander_hud_source, "qsTr(\"[Space] Dodge  [Alt] Jump\")"));
  EXPECT_TRUE(contains(commander_hud_source,
                       "qsTr(\"[Tab] Cycle Target  [3] Aura  [C] Camera\")"));

  EXPECT_TRUE(contains(fpv_overlay_source, "property real bottomInset: 0"));

  EXPECT_TRUE(contains(fpv_overlay_source, "id: hudBand"));
  EXPECT_TRUE(contains(fpv_overlay_source,
                       "anchors.bottomMargin: root.bottomInset + root.scaled(20)"));
  EXPECT_EQ(1,
            occurrences(fpv_overlay_source,
                        "anchors.bottomMargin: root.bottomInset + root.scaled(20)"))
      << "the commander HUD is one bottom band; every plate hangs off it so a "
         "single inset keeps them all clear of the RTS panel";
}

TEST(CommanderControlRegressionTest,
     CommanderRpgHudUsesLocalizedCombatFeedbackWithoutFullscreenFlashes) {
  const auto root = find_repo_root();
  const auto fpv_overlay_source = read_text(root / "ui" / "qml" / "RpgFpvOverlay.qml");
  const auto damage_numbers_source =
      read_text(root / "ui" / "qml" / "FloatingNumbers.qml");
  ASSERT_FALSE(fpv_overlay_source.empty());
  ASSERT_FALSE(damage_numbers_source.empty());

  EXPECT_TRUE(contains(fpv_overlay_source, "id: lockBrackets"));
  EXPECT_TRUE(contains(fpv_overlay_source, "id: combatFrame"));

  EXPECT_FALSE(contains(fpv_overlay_source, "id: punishPulseRing"));
  EXPECT_FALSE(contains(fpv_overlay_source, "id: finisherBurst"));
  EXPECT_TRUE(contains(fpv_overlay_source, "id: attackSweep"));
  EXPECT_TRUE(contains(fpv_overlay_source, "id: dodgeTrail"));
  EXPECT_TRUE(contains(fpv_overlay_source, "id: guardBreakShock"));
  EXPECT_TRUE(contains(fpv_overlay_source,
                       "id: perfectGuardFlash\n        anchors.centerIn: parent"));
  EXPECT_TRUE(contains(fpv_overlay_source,
                       "id: combatEntryFlash\n        anchors.centerIn: parent"));
  EXPECT_TRUE(contains(fpv_overlay_source,
                       "id: guardBreakShock\n        anchors.centerIn: parent"));
  EXPECT_FALSE(contains(fpv_overlay_source,
                        "id: perfectGuardFlash\n        anchors.fill: parent"));
  EXPECT_FALSE(contains(fpv_overlay_source,
                        "id: combatEntryFlash\n        anchors.fill: parent"));
  EXPECT_FALSE(contains(fpv_overlay_source,
                        "id: guardBreakShock\n        anchors.fill: parent"));
  EXPECT_TRUE(contains(fpv_overlay_source, "\"key\": \"F\""));
  EXPECT_TRUE(contains(fpv_overlay_source, "\"key\": \"1\""));
  EXPECT_TRUE(contains(fpv_overlay_source, "\"key\": \"2\""));
  EXPECT_TRUE(contains(damage_numbers_source, "id: burstCore"));
  EXPECT_TRUE(contains(damage_numbers_source, "id: burstLayer"));
  EXPECT_TRUE(contains(damage_numbers_source, "id: tickLayer"));
  EXPECT_FALSE(contains(damage_numbers_source, "impactFlashOpacity"));
  EXPECT_FALSE(contains(damage_numbers_source, "impactFlashDecay"));
}

TEST(CommanderControlRegressionTest, WorldAnchoredOverlaysShareOneProjectionClock) {
  const auto root = find_repo_root();
  const auto qml = root / "ui" / "qml";
  const auto hud_source = read_text(qml / "HUD.qml");
  const auto projector_source = read_text(qml / "WorldProjector.qml");
  ASSERT_FALSE(hud_source.empty());
  ASSERT_FALSE(projector_source.empty());

  EXPECT_TRUE(contains(hud_source, "id: worldProjector"));
  EXPECT_EQ(3, occurrences(hud_source, "projector: worldProjector"))
      << "the numbers layer, the FPV overlay and the tutorial markers all hang "
         "off the one clock";

  for (const char* overlay :
       {"FloatingNumbers.qml", "RpgFpvOverlay.qml", "TutorialFocusOverlay.qml"}) {
    const auto source = read_text(qml / overlay);
    ASSERT_FALSE(source.empty()) << overlay;
    EXPECT_FALSE(contains(source, "project_world"))
        << overlay
        << " must go through WorldProjector.project so one timer drives every "
           "world-anchored overlay";
  }

  EXPECT_TRUE(contains(projector_source, "onTriggered: root.tick++"));
}

TEST(CommanderControlRegressionTest, MainWindowHidesCursorDuringFpvCommanderGameplay) {
  const auto root = find_repo_root();
  const auto main_qml = read_text(root / "ui" / "qml" / "Main.qml");
  ASSERT_FALSE(main_qml.empty());

  EXPECT_TRUE(contains(main_qml, "id: commanderCursorOverlay"));
  EXPECT_TRUE(contains(main_qml, "acceptedButtons: Qt.NoButton"));
  EXPECT_TRUE(contains(main_qml, "cursorShape: Qt.BlankCursor"));
  EXPECT_TRUE(contains(main_qml, "game.commander.mode_state === \"active\" &&"));
  EXPECT_TRUE(contains(main_qml, "!save_game_panel.visible &&"));
}

TEST(CommanderControlRegressionTest, FpvMovementSetsHasTargetForAnimationSystem) {
  const auto root = find_repo_root();
  const auto source = app_source(root, "commander_control_controller.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(source, "movement->engage_manual_move("));

  EXPECT_TRUE(contains(source, "stamina->run_requested = m_move_running;"));
  EXPECT_TRUE(contains(source, "stamina->run_requested = false;"));
}

TEST(CommanderControlRegressionTest, FpvAttackAlwaysTriggersAnimationEvenWithNoTarget) {
  const auto root = find_repo_root();
  const auto source = app_source(root, "commander_control_controller.cpp");
  const auto action_service = read_text(root / "game" / "systems" / "combat_actions" /
                                        "combat_action_service.cpp");
  ASSERT_FALSE(source.empty());
  ASSERT_FALSE(action_service.empty());

  EXPECT_TRUE(
      contains(source, "find_primary_target(world, commander_id, local_owner_id);"));
  EXPECT_TRUE(contains(source, "CombatActionService::request_attack("));
  EXPECT_TRUE(contains(source, ".target_hint_id = target_id,"));

  EXPECT_TRUE(contains(action_service,
                       "combat_state->animation_state = "
                       "Engine::Core::CombatAnimationState::Advance;"));
  EXPECT_TRUE(contains(
      source,
      "if (attack_result.outcome == Engine::Core::CombatIntentOutcome::Accepted) {"))
      << "a refused swing must not drop direct control; the reason belongs on "
         "the intent queue";

  EXPECT_TRUE(
      contains(action_service, "combat_state->damage_dealt_this_swing = false;"));
  EXPECT_FALSE(contains(source, "target_comp->target_id = target_id;"));
}

TEST(CommanderControlRegressionTest, FpvCombatUsesSharedCombatRulesHelper) {
  const auto root = find_repo_root();
  const auto combat_rules = read_text(root / "game" / "systems" / "combat_rules.h");
  const auto attack_processor =
      read_text(root / "game" / "systems" / "combat_system" / "attack_processor.cpp");
  const auto movement_system =
      read_text(root / "game" / "systems" / "movement_system.cpp");

  const auto route_follow_system =
      read_text(root / "game" / "systems" / "route_follow_system.cpp");
  const auto movement_orders =
      read_text(root / "game" / "systems" / "movement_orders.cpp");
  const auto command_service =
      read_text(root / "game" / "systems" / "command_service.cpp");
  const auto scene_walk = read_text(root / "render" / "scene_walk.cpp");
  const auto animation_inputs = read_text(root / "render" / "gl" / "humanoid" /
                                          "animation" / "animation_inputs.cpp");
  const auto prepared_state = read_text(root / "render" / "creature" / "pipeline" /
                                        "creature_prepared_state.cpp");
  const auto combat_dust_renderer =
      read_text(root / "render" / "entity" / "combat_dust_renderer.cpp");

  const auto command_dispatcher =
      read_text(root / "game" / "command" / "command_dispatcher.cpp");
  const auto game_engine = app_source(root, "game_engine.cpp");
  const auto controller = app_source(root, "commander_control_controller.cpp");
  const auto commander_mode = app_source(root, "commander_mode_coordinator.cpp");
  ASSERT_FALSE(combat_rules.empty());
  ASSERT_FALSE(attack_processor.empty());
  ASSERT_FALSE(movement_system.empty());
  ASSERT_FALSE(route_follow_system.empty());
  ASSERT_FALSE(movement_orders.empty());
  ASSERT_FALSE(command_service.empty());
  ASSERT_FALSE(scene_walk.empty());
  ASSERT_FALSE(animation_inputs.empty());
  ASSERT_FALSE(prepared_state.empty());
  ASSERT_FALSE(combat_dust_renderer.empty());
  ASSERT_FALSE(command_dispatcher.empty());
  ASSERT_FALSE(game_engine.empty());
  ASSERT_FALSE(controller.empty());
  ASSERT_FALSE(commander_mode.empty());

  EXPECT_TRUE(contains(combat_rules, "uses_rpg_combat_rules"));
  EXPECT_TRUE(contains(combat_rules, "participates_in_rts_melee_lock"));
  EXPECT_TRUE(contains(combat_rules, "clear_rts_combat_tracking"));

  EXPECT_TRUE(
      contains(attack_processor, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_FALSE(
      contains(attack_processor, "CombatRules::clear_rts_melee_lock(attacker);"));
  EXPECT_FALSE(
      contains(attack_processor, "CombatRules::clear_rts_melee_lock(target);"));
  EXPECT_TRUE(
      contains(route_follow_system, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_TRUE(contains(movement_orders, "CombatRules::participates_in_rts_melee_lock"));

  EXPECT_FALSE(contains(scene_walk, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_FALSE(
      contains(animation_inputs, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_FALSE(contains(prepared_state, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_TRUE(
      contains(combat_dust_renderer, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_TRUE(
      contains(command_dispatcher, "CombatRules::clear_rts_melee_lock(&entity);"));

  EXPECT_TRUE(contains(commander_mode, "commander_data->fpv_controlled = true;"));
  EXPECT_TRUE(contains(commander_mode, "commander_data->fpv_controlled = false;"));
  EXPECT_FALSE(
      contains(game_engine, "CombatRules::clear_rts_combat_tracking(commander);"));
  EXPECT_FALSE(contains(controller, "atk->in_melee_lock = false;"));
}

TEST(CommanderControlRegressionTest, FpvCombatCameraHasNoSyntheticHitShakeOrPunch) {
  const auto root = find_repo_root();
  const auto rig_src = app_source(root, "commander_camera_rig.cpp");
  const auto rig_hdr = app_source(root, "commander_camera_rig.h");
  ASSERT_FALSE(rig_src.empty());
  ASSERT_FALSE(rig_hdr.empty());

  EXPECT_FALSE(contains(rig_hdr, "m_hit_trauma"));
  EXPECT_FALSE(contains(rig_hdr, "m_hit_shake_phase"));
  EXPECT_FALSE(contains(rig_hdr, "m_strike_camera_punch"));
  EXPECT_FALSE(contains(rig_hdr, "m_impact_shake"));
  EXPECT_FALSE(contains(rig_src, "shake_offset"));
  EXPECT_TRUE(
      contains(rig_src, "camera.look_at(m_eye_smooth, m_target_smooth, up_final);"));
}

TEST(CommanderControlRegressionTest, CommanderJumpKeyReachesTheController) {
  const auto root = find_repo_root();
  const auto layer_source = read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto view_model_header = app_source(root, "commander_view_model.h");
  const auto view_model_source = app_source(root, "commander_view_model.cpp");
  const auto controller_header = app_source(root, "commander_control_controller.h");
  const auto controller_source = app_source(root, "commander_control_controller.cpp");
  ASSERT_FALSE(layer_source.empty());
  ASSERT_FALSE(game_view_source.empty());
  ASSERT_FALSE(view_model_header.empty());
  ASSERT_FALSE(view_model_source.empty());
  ASSERT_FALSE(controller_header.empty());
  ASSERT_FALSE(controller_source.empty());

  EXPECT_TRUE(contains(layer_source, "case \"commander.jump\":"));
  EXPECT_TRUE(contains(layer_source, "root.commander.jump()"));
  EXPECT_TRUE(contains(game_view_source, "case \"commander.jump\":"));
  EXPECT_TRUE(contains(game_view_source, "game.commander.jump()"));
  EXPECT_TRUE(contains(view_model_header, "Q_INVOKABLE void jump();"));
  EXPECT_TRUE(contains(view_model_source, "void CommanderViewModel::jump()"));
  EXPECT_TRUE(contains(controller_header, "void request_jump();"));
  EXPECT_TRUE(
      contains(controller_source, "void CommanderControlController::request_jump()"));
}

TEST(CommanderControlRegressionTest, CommanderCameraToggleReachesTheController) {
  const auto root = find_repo_root();
  const auto layer_source = read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto view_model_header = app_source(root, "commander_view_model.h");
  const auto view_model_source = app_source(root, "commander_view_model.cpp");
  const auto controller_header = app_source(root, "commander_control_controller.h");
  const auto controller_source = app_source(root, "commander_control_controller.cpp");
  ASSERT_FALSE(layer_source.empty());
  ASSERT_FALSE(game_view_source.empty());
  ASSERT_FALSE(view_model_header.empty());
  ASSERT_FALSE(view_model_source.empty());
  ASSERT_FALSE(controller_header.empty());
  ASSERT_FALSE(controller_source.empty());

  EXPECT_TRUE(contains(layer_source, "case \"commander.toggle_camera_mode\":"));
  EXPECT_TRUE(contains(layer_source, "root.commander.toggle_camera_mode()"));
  EXPECT_TRUE(contains(game_view_source, "case \"commander.toggle_camera_mode\":"));
  EXPECT_TRUE(contains(game_view_source, "game.commander.toggle_camera_mode()"));
  EXPECT_TRUE(contains(view_model_header, "Q_INVOKABLE void toggle_camera_mode();"));
  EXPECT_TRUE(
      contains(view_model_source, "void CommanderViewModel::toggle_camera_mode()"));
  EXPECT_TRUE(contains(controller_header, "void toggle_close_camera_mode("));
  EXPECT_TRUE(contains(controller_source,
                       "void CommanderControlController::toggle_close_camera_mode("));
}

TEST(CommanderControlRegressionTest, CommanderAbilityKitReachesTheController) {
  const auto root = find_repo_root();
  const auto layer_source = read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto commander_hud_source =
      read_text(root / "ui" / "qml" / "HUDBottomCommander.qml");
  const auto view_model_header = app_source(root, "commander_view_model.h");
  const auto view_model_source = app_source(root, "commander_view_model.cpp");
  const auto controller_header = app_source(root, "commander_control_controller.h");
  ASSERT_FALSE(layer_source.empty());
  ASSERT_FALSE(game_view_source.empty());
  ASSERT_FALSE(commander_hud_source.empty());
  ASSERT_FALSE(view_model_header.empty());
  ASSERT_FALSE(view_model_source.empty());
  ASSERT_FALSE(controller_header.empty());

  EXPECT_TRUE(contains(layer_source, "case \"commander.ability_vanguard_rush\":"));
  EXPECT_TRUE(contains(layer_source, "root.commander.vanguard_rush()"));
  EXPECT_TRUE(contains(layer_source, "case \"commander.ability_second_wind\":"));
  EXPECT_TRUE(contains(layer_source, "root.commander.second_wind()"));
  EXPECT_TRUE(contains(game_view_source, "case \"commander.ability_vanguard_rush\":"));
  EXPECT_TRUE(contains(game_view_source, "game.commander.vanguard_rush()"));
  EXPECT_TRUE(contains(game_view_source, "case \"commander.ability_second_wind\":"));
  EXPECT_TRUE(contains(game_view_source, "game.commander.second_wind()"));
  EXPECT_TRUE(contains(commander_hud_source, "\"key\": \"1\""));
  EXPECT_TRUE(contains(commander_hud_source, "\"key\": \"2\""));
  EXPECT_TRUE(contains(commander_hud_source, "game.commander.status"));
  EXPECT_TRUE(contains(commander_hud_source, "cooldown_progress"));
  EXPECT_TRUE(contains(commander_hud_source, "vanguard_rush_cooldown_remaining"));
  EXPECT_TRUE(contains(commander_hud_source, "second_wind_cooldown_remaining"));
  EXPECT_TRUE(contains(commander_hud_source, "shield_bash_cooldown_remaining"));
  EXPECT_TRUE(contains(commander_hud_source, "Vanguard Rush"));
  EXPECT_TRUE(contains(commander_hud_source, "Second Wind"));
  EXPECT_TRUE(contains(view_model_header, "Q_INVOKABLE void vanguard_rush();"));
  EXPECT_TRUE(contains(view_model_header, "Q_INVOKABLE void second_wind();"));
  EXPECT_TRUE(contains(view_model_source, "void CommanderViewModel::vanguard_rush()"));
  EXPECT_TRUE(contains(view_model_source, "void CommanderViewModel::second_wind()"));
  EXPECT_TRUE(contains(controller_header, "void request_vanguard_rush();"));
  EXPECT_TRUE(contains(controller_header, "void request_second_wind();"));
}

TEST(CommanderControlRegressionTest, CommanderJumpAddsVisualLiftToRenderAndCamera) {
  const auto root = find_repo_root();
  const auto component_source =
      read_text(root / "game" / "core" / "component_commander.h") +
      read_text(root / "game" / "core" / "component_presentation.h");
  const auto controller_source = app_source(root, "commander_control_controller.cpp");
  const auto commander_mode_source = app_source(root, "commander_mode_coordinator.cpp");
  const auto prepare_submission_source =
      read_text(root / "render" / "humanoid" / "runtime" / "instance_prepare.cpp");
  const auto locomotion_manifest_source =
      read_text(root / "animation" / "locomotion_manifest.cpp");
  const auto ambient_manifest_source =
      read_text(root / "animation" / "ambient_pose_manifest.cpp");
  ASSERT_FALSE(component_source.empty());
  ASSERT_FALSE(controller_source.empty());
  ASSERT_FALSE(commander_mode_source.empty());
  ASSERT_FALSE(prepare_submission_source.empty());
  ASSERT_FALSE(locomotion_manifest_source.empty());
  ASSERT_FALSE(ambient_manifest_source.empty());

  EXPECT_TRUE(contains(component_source, "bool jump_active{false};"));
  EXPECT_TRUE(contains(component_source, "float jump_phase{0.0F};"));
  EXPECT_TRUE(contains(component_source, "float jump_height_offset{0.0F};"));

  EXPECT_TRUE(contains(controller_source, "constexpr float k_jump_duration ="));
  EXPECT_TRUE(contains(controller_source,
                       "cmd_comp->jump_height_offset = jump_height_offset;"));
  EXPECT_TRUE(contains(controller_source, "m_jump_last_walkable_position"));
  EXPECT_TRUE(contains(controller_source, "m_jump_timer <= 0.0F"));

  EXPECT_TRUE(contains(commander_mode_source, "commander_data->jump_active = false;"));

  EXPECT_TRUE(contains(prepare_submission_source, "RCP::set_model_world_y("));
  EXPECT_TRUE(contains(prepare_submission_source,
                       "RCP::model_world_origin(inst_ctx.model).y() +"));
  EXPECT_TRUE(contains(prepare_submission_source,
                       "resolve_humanoid_locomotion_action_override"));
  EXPECT_TRUE(
      contains(locomotion_manifest_source, "if (!inputs.commander_jump_active)"));
  EXPECT_TRUE(
      contains(locomotion_manifest_source, ".state = HumanoidMotionState::Idle,"));
  EXPECT_TRUE(contains(locomotion_manifest_source, ".airborne = true,"));
  EXPECT_TRUE(
      contains(prepare_submission_source, "resolve_humanoid_ambient_selection"));
  EXPECT_TRUE(contains(ambient_manifest_source, "if (inputs.jump_active)"));
  EXPECT_TRUE(contains(ambient_manifest_source, ".type = HumanoidAmbientIdle::Jump,"));
}

TEST(CommanderControlRegressionTest, OnlyTheMotorTranslatesTheCommander) {
  const auto root = find_repo_root();
  const auto controller_source = app_source(root, "commander_control_controller.cpp");
  const auto motor_source = app_source(root, "commander_motor.cpp");
  ASSERT_FALSE(controller_source.empty());
  ASSERT_FALSE(motor_source.empty());

  for (const std::string write : {"transform->position.x =",
                                  "transform->position.z =",
                                  "transform.position.x =",
                                  "transform.position.z ="}) {
    EXPECT_FALSE(contains(controller_source, write))
        << "the controller writes " << write
        << " directly. Commander translation goes through CommanderMotor so that "
           "every displacement is collision-checked and reported once.";
  }

  EXPECT_TRUE(contains(motor_source, "transform.position.x = step.x;"));
  EXPECT_TRUE(contains(motor_source, "transform.position.z = step.z;"));

  for (const std::string source : {"CommanderDisplacementSource::Walk",
                                   "CommanderDisplacementSource::Airborne",
                                   "CommanderDisplacementSource::DodgeRoll",
                                   "CommanderDisplacementSource::DodgeRecover",
                                   "CommanderDisplacementSource::StrikeLunge",
                                   "CommanderDisplacementSource::BodySeparation"}) {
    EXPECT_TRUE(contains(controller_source, source))
        << source << " has to reach the motor as a named displacement source";
  }
}

TEST(CommanderControlRegressionTest,
     CommanderJumpAllowsAirborneTraversalAcrossGroundObstacles) {
  const auto root = find_repo_root();
  const auto controller_source = app_source(root, "commander_control_controller.cpp");
  const auto motor_source = app_source(root, "commander_motor.cpp");
  const auto movement_source =
      read_text(root / "game" / "systems" / "movement_system.cpp");
  ASSERT_FALSE(controller_source.empty());
  ASSERT_FALSE(motor_source.empty());
  ASSERT_FALSE(movement_source.empty());

  EXPECT_TRUE(contains(controller_source, ".airborne = jump_active,"));
  EXPECT_TRUE(contains(motor_source, "airborne_step(request.to.x(), request.to.z())"));
  EXPECT_TRUE(contains(controller_source, "jump_active"));
  EXPECT_TRUE(contains(controller_source, "m_jump_safe_position_valid"));
  EXPECT_TRUE(contains(movement_source, "commander->jump_active"));
}
