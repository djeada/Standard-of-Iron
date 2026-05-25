#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace {

auto find_repo_root() -> std::filesystem::path {
  auto has_repo_markers = [](const std::filesystem::path& path) {
    return std::filesystem::exists(path / "CMakeLists.txt") &&
           std::filesystem::exists(path / "app" / "core" /
                                   "commander_control_controller.cpp") &&
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

auto contains(const std::string& text, const std::string& needle) -> bool {
  return text.find(needle) != std::string::npos;
}

} // namespace

TEST(CommanderControlRegressionTest, CommanderStrafeUsesRightHandedBasis) {
  const auto root = find_repo_root();
  const auto source =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(
      contains(source, "const QVector3D right(-forward.z(), 0.0F, forward.x());"));
}

TEST(CommanderControlRegressionTest, CommanderMouseLookIsPolledInEngine) {
  const auto root = find_repo_root();
  const auto engine_source = read_text(root / "app" / "core" / "game_engine.cpp");
  const auto controller_source =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  ASSERT_FALSE(engine_source.empty());
  ASSERT_FALSE(controller_source.empty());

  EXPECT_TRUE(contains(engine_source, "void GameEngine::poll_commander_mouse_look()"));
  EXPECT_TRUE(contains(engine_source, "poll_commander_mouse_look();"));
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

TEST(CommanderControlRegressionTest, GameViewRestoresInputFocusAcrossModes) {
  const auto root = find_repo_root();
  const auto source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto commander_source =
      read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  ASSERT_FALSE(source.empty());
  ASSERT_FALSE(commander_source.empty());

  EXPECT_TRUE(contains(source, "Keys.onPressed: function(event)") ||
              contains(source, "Keys.onPressed: function (event)"));
  EXPECT_TRUE(contains(source, "function handle_commander_key_pressed(event)"));
  EXPECT_TRUE(contains(source, "function is_commander_mode()"));
  EXPECT_TRUE(contains(source, "game.commander_key_down(event.key, event.modifiers)"));
  EXPECT_TRUE(contains(source, "game.toggle_commander_control_mode()"));
  EXPECT_TRUE(contains(source, "function onControl_mode_changed()"));
  EXPECT_TRUE(contains(source, "game_view.forceActiveFocus();"));

  EXPECT_TRUE(contains(source, "game.commander_input"));
  EXPECT_TRUE(contains(commander_source, "property var commanderInput"));
  EXPECT_FALSE(contains(commander_source, "root.game.commander_"));
}

TEST(CommanderControlRegressionTest,
     GameViewRoutesRightGestureThroughEngineController) {
  const auto root = find_repo_root();
  const auto source = read_text(root / "ui" / "qml" / "GameView.qml");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(source, "game.on_right_press(mouse.x, mouse.y);"));
  EXPECT_TRUE(contains(source, "game.on_right_move(mouse.x, mouse.y);"));
  EXPECT_TRUE(contains(source, "game.on_right_release(mouse.x, mouse.y);"));
  EXPECT_TRUE(contains(source, "game.on_right_double_click(mouse.x, mouse.y);"));
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
  const auto source = read_text(root / "app" / "core" / "game_engine.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(source, "enter_rts_runtime_mode();"));
  EXPECT_TRUE(contains(source, "void GameEngine::enter_rts_runtime_mode()"));
  EXPECT_FALSE(contains(source, "class GameEngine::RuntimeMode"));
  EXPECT_FALSE(contains(source, "m_control_mode_toggle"));
}

TEST(CommanderControlRegressionTest,
     GameEngineLetsDoubleRightClickOverridePressStartedFormationPlacement) {
  const auto root = find_repo_root();
  const auto header = read_text(root / "app" / "core" / "game_engine.h");
  const auto source = read_text(root / "app" / "core" / "game_engine.cpp");
  ASSERT_FALSE(header.empty());
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(header, "bool placement_was_active_on_press = false;"));
  EXPECT_TRUE(contains(header, "bool started_formation_placement = false;"));
  EXPECT_TRUE(
      contains(source, "if (m_right_mouse_gesture.placement_was_active_on_press)"));
  EXPECT_TRUE(contains(source, "if (started_formation_placement) {"));
  EXPECT_TRUE(contains(source, "m_input_handler->on_formation_cancel();"));
  EXPECT_TRUE(contains(source, "m_right_mouse_gesture.started_formation_placement ="));
}

TEST(CommanderControlRegressionTest, CommanderRallyKeyIsWiredThroughAdapter) {
  const auto root = find_repo_root();
  const auto layer_source = read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
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
  EXPECT_TRUE(contains(engine_header, "Q_INVOKABLE void commander_trigger_rally();"));
}

TEST(CommanderControlRegressionTest, CommanderCameraUsesChaseOffsetView) {
  const auto root = find_repo_root();
  const auto source =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(source, "constexpr float k_camera_back_offset = 2.15F;"));
  EXPECT_TRUE(contains(source, "constexpr float k_close_camera_back_offset = 1.25F;"));
  EXPECT_TRUE(contains(source, "constexpr float k_commander_near_plane = 0.05F;"));
  EXPECT_TRUE(contains(source, "const QVector3D flat_forward("));
  EXPECT_TRUE(contains(source, "pivot - flat_forward * back_offset"));

  EXPECT_TRUE(contains(source, "camera.look_at(m_cam_eye_smooth + shake_offset"));
  EXPECT_TRUE(contains(source, "m_cam_target_smooth + shake_offset"));

  EXPECT_TRUE(contains(source, "bob_v + breath_v"));
  EXPECT_TRUE(contains(source, "flat_right * bob_l"));
}

TEST(CommanderControlRegressionTest, CommanderModePreservesAndRestoresRtsSelection) {
  const auto root = find_repo_root();
  const auto engine_header = read_text(root / "app" / "core" / "game_engine.h");
  const auto engine_source = read_text(root / "app" / "core" / "game_engine.cpp");
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  ASSERT_FALSE(engine_header.empty());
  ASSERT_FALSE(engine_source.empty());
  ASSERT_FALSE(game_view_source.empty());

  EXPECT_TRUE(contains(
      engine_header, "std::vector<Engine::Core::EntityID> m_saved_rts_selection_ids;"));
  EXPECT_TRUE(contains(engine_source, "void GameEngine::store_rts_selection()"));
  EXPECT_TRUE(
      contains(engine_source, "void GameEngine::select_controlled_commander()"));
  EXPECT_TRUE(contains(engine_source, "void GameEngine::restore_rts_selection()"));
  EXPECT_TRUE(contains(engine_source, "store_rts_selection();"));
  EXPECT_TRUE(contains(engine_source, "select_controlled_commander();"));
  EXPECT_TRUE(contains(engine_source, "restore_rts_selection();"));
  EXPECT_TRUE(contains(engine_source,
                       "m_selection_controller->select_single_unit("
                       "m_controlled_commander_id,"));
  EXPECT_TRUE(contains(game_view_source, "game.cancel_barracks_rally_placement();"));
}

TEST(CommanderControlRegressionTest, BarracksRallyPlacementUsesDedicatedCursorMode) {
  const auto root = find_repo_root();
  const auto engine_header = read_text(root / "app" / "core" / "game_engine.h");
  const auto cursor_mode_header = read_text(root / "app" / "models" / "cursor_mode.h");
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto hud_source = read_text(root / "ui" / "qml" / "HUDBottom.qml");
  const auto production_panel_source =
      read_text(root / "ui" / "qml" / "ProductionPanel.qml");
  ASSERT_FALSE(engine_header.empty());
  ASSERT_FALSE(cursor_mode_header.empty());
  ASSERT_FALSE(game_view_source.empty());
  ASSERT_FALSE(hud_source.empty());
  ASSERT_FALSE(production_panel_source.empty());

  EXPECT_TRUE(
      contains(engine_header, "Q_INVOKABLE void begin_barracks_rally_placement();"));
  EXPECT_TRUE(contains(
      engine_header,
      "Q_INVOKABLE void confirm_barracks_rally_placement(qreal sx, qreal sy);"));
  EXPECT_TRUE(
      contains(engine_header, "Q_INVOKABLE void cancel_barracks_rally_placement();"));
  EXPECT_TRUE(contains(cursor_mode_header, "PlaceBarracksRally"));
  EXPECT_TRUE(contains(cursor_mode_header, "\"place_barracks_rally\""));
  EXPECT_TRUE(contains(game_view_source, "function is_barracks_rally_placement()"));
  EXPECT_TRUE(contains(game_view_source,
                       "game.confirm_barracks_rally_placement(mouse.x, mouse.y);"));
  EXPECT_TRUE(contains(game_view_source, "game.cancel_barracks_rally_placement();"));
  EXPECT_TRUE(contains(production_panel_source,
                       "gameView.cursor_mode === \"place_barracks_rally\""));
  EXPECT_TRUE(contains(hud_source, "game.begin_barracks_rally_placement();"));
}

TEST(CommanderControlRegressionTest, SaveAndLoadForceCommanderModeBackToRts) {
  const auto root = find_repo_root();
  const auto engine_source = read_text(root / "app" / "core" / "game_engine.cpp");
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
  EXPECT_TRUE(
      contains(hud_source,
               "sourceComponent: typeof game !== 'undefined' && game.control_mode === "
               "\"commander\" ? commanderBottomHudComponent : rtsBottomHudComponent"));
  EXPECT_TRUE(contains(hud_source, "HUDBottomCommander {"));
  EXPECT_TRUE(contains(qrc_source, "ui/qml/HUDBottomCommander.qml"));
  EXPECT_TRUE(contains(commander_hud_source, "game.get_controlled_commander_status"));
  EXPECT_TRUE(contains(commander_hud_source, "game.commander_trigger_rally"));
  EXPECT_TRUE(contains(engine_header,
                       "Q_INVOKABLE [[nodiscard]] QVariantMap "
                       "get_controlled_commander_status() const;"));
}

TEST(CommanderControlRegressionTest, FpvCommanderHitOverlayUsesRichDamageBurstData) {
  const auto root = find_repo_root();
  const auto engine_source = read_text(root / "app" / "core" / "game_engine.cpp");
  const auto hud_source = read_text(root / "ui" / "qml" / "HUD.qml");
  const auto damage_numbers_source =
      read_text(root / "ui" / "qml" / "RpgDamageNumbers.qml");
  ASSERT_FALSE(engine_source.empty());
  ASSERT_FALSE(hud_source.empty());
  ASSERT_FALSE(damage_numbers_source.empty());

  EXPECT_TRUE(contains(engine_source, "m[\"damageRatio\"] = static_cast<double>("));
  EXPECT_TRUE(contains(engine_source, "m[\"lane\"] = ev.lane;"));
  EXPECT_TRUE(contains(engine_source, "m[\"killingBlow\"] = ev.killing_blow;"));

  EXPECT_TRUE(contains(hud_source, "game.control_mode === \"commander\""));

  EXPECT_TRUE(contains(damage_numbers_source, "property real ringSize"));
  EXPECT_TRUE(contains(damage_numbers_source, "damageRatio"));
  EXPECT_TRUE(contains(damage_numbers_source, "killingBlow"));
}

TEST(CommanderControlRegressionTest, MainWindowHidesCursorDuringFpvCommanderGameplay) {
  const auto root = find_repo_root();
  const auto main_qml = read_text(root / "ui" / "qml" / "Main.qml");
  ASSERT_FALSE(main_qml.empty());

  EXPECT_TRUE(contains(main_qml, "id: commanderCursorOverlay"));
  EXPECT_TRUE(contains(main_qml, "acceptedButtons: Qt.NoButton"));
  EXPECT_TRUE(contains(main_qml, "cursorShape: Qt.BlankCursor"));
  EXPECT_TRUE(contains(main_qml, "game.control_mode === \"commander\" &&"));
  EXPECT_TRUE(contains(main_qml, "game.game_mode === \"rpg\" &&"));
  EXPECT_TRUE(contains(main_qml, "!save_game_panel.visible &&"));
}

TEST(CommanderControlRegressionTest, FpvMovementSetsHasTargetForAnimationSystem) {
  const auto root = find_repo_root();
  const auto source =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(source, "movement->has_target = true;"));

  EXPECT_TRUE(contains(source, "stamina->run_requested = m_move_running;"));
  EXPECT_TRUE(contains(source, "stamina->run_requested = false;"));
}

TEST(CommanderControlRegressionTest, FpvAttackAlwaysTriggersAnimationEvenWithNoTarget) {
  const auto root = find_repo_root();
  const auto source =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  ASSERT_FALSE(source.empty());

  EXPECT_TRUE(contains(source,
                       "combat_state->animation_state = "
                       "Engine::Core::CombatAnimationState::Advance;"));
  EXPECT_TRUE(
      contains(source, "find_primary_target(world, commander_id, local_owner_id);"));
  EXPECT_TRUE(contains(source, "if (target_id == 0) {"));
  // Damage is now deferred to Strike phase in combat_state_processor
  EXPECT_TRUE(
      contains(source, "combat_state->damage_dealt_this_swing = false;"));
  EXPECT_FALSE(contains(source, "target_comp->target_id = target_id;"));
}

TEST(CommanderControlRegressionTest, FpvCombatUsesSharedCombatRulesHelper) {
  const auto root = find_repo_root();
  const auto combat_rules = read_text(root / "game" / "systems" / "combat_rules.h");
  const auto attack_processor =
      read_text(root / "game" / "systems" / "combat_system" / "attack_processor.cpp");
  const auto movement_system =
      read_text(root / "game" / "systems" / "movement_system.cpp");
  const auto command_service =
      read_text(root / "game" / "systems" / "command_service.cpp");
  const auto scene_renderer = read_text(root / "render" / "scene_renderer.cpp");
  const auto animation_inputs = read_text(root / "render" / "gl" / "humanoid" /
                                          "animation" / "animation_inputs.cpp");
  const auto prepared_state = read_text(root / "render" / "creature" / "pipeline" /
                                        "creature_prepared_state.cpp");
  const auto combat_dust_renderer =
      read_text(root / "render" / "entity" / "combat_dust_renderer.cpp");
  const auto combat_dust_pipeline =
      read_text(root / "render" / "gl" / "backend" / "combat_dust_pipeline.cpp");
  const auto command_controller =
      read_text(root / "app" / "controllers" / "command_controller.cpp");
  const auto game_engine = read_text(root / "app" / "core" / "game_engine.cpp");
  const auto controller =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  ASSERT_FALSE(combat_rules.empty());
  ASSERT_FALSE(attack_processor.empty());
  ASSERT_FALSE(movement_system.empty());
  ASSERT_FALSE(command_service.empty());
  ASSERT_FALSE(scene_renderer.empty());
  ASSERT_FALSE(animation_inputs.empty());
  ASSERT_FALSE(prepared_state.empty());
  ASSERT_FALSE(combat_dust_renderer.empty());
  ASSERT_FALSE(combat_dust_pipeline.empty());
  ASSERT_FALSE(command_controller.empty());
  ASSERT_FALSE(game_engine.empty());
  ASSERT_FALSE(controller.empty());

  EXPECT_TRUE(contains(combat_rules, "uses_rpg_combat_rules"));
  EXPECT_TRUE(contains(combat_rules, "participates_in_rts_melee_lock"));
  EXPECT_TRUE(contains(combat_rules, "clear_rts_combat_tracking"));

  EXPECT_TRUE(
      contains(attack_processor, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_FALSE(
      contains(attack_processor, "CombatRules::clear_rts_melee_lock(attacker);"));
  EXPECT_FALSE(
      contains(attack_processor, "CombatRules::clear_rts_melee_lock(target);"));
  EXPECT_TRUE(contains(movement_system, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_TRUE(contains(command_service, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_TRUE(contains(scene_renderer, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_TRUE(
      contains(animation_inputs, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_TRUE(contains(prepared_state, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_TRUE(
      contains(combat_dust_renderer, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_TRUE(
      contains(combat_dust_pipeline, "CombatRules::participates_in_rts_melee_lock"));
  EXPECT_TRUE(
      contains(command_controller, "CombatRules::clear_rts_melee_lock(entity);"));

  EXPECT_TRUE(contains(game_engine, "commander_data->fpv_controlled = true;"));
  EXPECT_TRUE(contains(game_engine, "commander_data->fpv_controlled = false;"));
  EXPECT_FALSE(
      contains(game_engine, "CombatRules::clear_rts_combat_tracking(commander);"));
  EXPECT_FALSE(contains(controller, "atk->in_melee_lock = false;"));
}

TEST(CommanderControlRegressionTest, FpvHitShakeUsesHitFeedbackComponentTrauma) {
  const auto root = find_repo_root();
  const auto controller_src =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  const auto controller_hdr =
      read_text(root / "app" / "core" / "commander_control_controller.h");
  ASSERT_FALSE(controller_src.empty());
  ASSERT_FALSE(controller_hdr.empty());

  EXPECT_TRUE(contains(controller_hdr, "m_hit_trauma"));
  EXPECT_TRUE(contains(controller_hdr, "m_hit_shake_phase"));

  EXPECT_TRUE(contains(controller_src, "m_hit_trauma = fb->reaction_intensity;"));

  EXPECT_TRUE(contains(controller_src, "shake_offset"));
  EXPECT_TRUE(contains(controller_src, "m_cam_eye_smooth + shake_offset"));
}

TEST(CommanderControlRegressionTest, CommanderJumpKeyIsWiredThroughAdapter) {
  const auto root = find_repo_root();
  const auto layer_source = read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto adapter_header =
      read_text(root / "app" / "core" / "commander_input_adapter.h");
  const auto adapter_source =
      read_text(root / "app" / "core" / "commander_input_adapter.cpp");
  const auto engine_header = read_text(root / "app" / "core" / "game_engine.h");
  const auto engine_source = read_text(root / "app" / "core" / "game_engine.cpp");
  const auto controller_header =
      read_text(root / "app" / "core" / "commander_control_controller.h");
  const auto controller_source =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  ASSERT_FALSE(layer_source.empty());
  ASSERT_FALSE(game_view_source.empty());
  ASSERT_FALSE(adapter_header.empty());
  ASSERT_FALSE(adapter_source.empty());
  ASSERT_FALSE(engine_header.empty());
  ASSERT_FALSE(engine_source.empty());
  ASSERT_FALSE(controller_header.empty());
  ASSERT_FALSE(controller_source.empty());

  EXPECT_TRUE(contains(layer_source, "case Qt.Key_Alt:"));
  EXPECT_TRUE(contains(layer_source, "root.commanderInput.jump()"));
  EXPECT_TRUE(contains(game_view_source, "case Qt.Key_Alt:"));
  EXPECT_TRUE(contains(game_view_source, "game.commander_jump()"));
  EXPECT_TRUE(contains(adapter_header, "Q_INVOKABLE void jump();"));
  EXPECT_TRUE(contains(adapter_source, "m_engine->commander_jump();"));
  EXPECT_TRUE(contains(engine_header, "Q_INVOKABLE void commander_jump();"));
  EXPECT_TRUE(contains(engine_source, "void GameEngine::commander_jump()"));
  EXPECT_TRUE(contains(controller_header, "void request_jump();"));
  EXPECT_TRUE(
      contains(controller_source, "void CommanderControlController::request_jump()"));
}

TEST(CommanderControlRegressionTest, CommanderCameraToggleIsWiredThroughAdapter) {
  const auto root = find_repo_root();
  const auto layer_source = read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto adapter_header =
      read_text(root / "app" / "core" / "commander_input_adapter.h");
  const auto adapter_source =
      read_text(root / "app" / "core" / "commander_input_adapter.cpp");
  const auto engine_header = read_text(root / "app" / "core" / "game_engine.h");
  const auto engine_source = read_text(root / "app" / "core" / "game_engine.cpp");
  const auto controller_header =
      read_text(root / "app" / "core" / "commander_control_controller.h");
  const auto controller_source =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  ASSERT_FALSE(layer_source.empty());
  ASSERT_FALSE(game_view_source.empty());
  ASSERT_FALSE(adapter_header.empty());
  ASSERT_FALSE(adapter_source.empty());
  ASSERT_FALSE(engine_header.empty());
  ASSERT_FALSE(engine_source.empty());
  ASSERT_FALSE(controller_header.empty());
  ASSERT_FALSE(controller_source.empty());

  EXPECT_TRUE(contains(layer_source, "case Qt.Key_C:"));
  EXPECT_TRUE(contains(layer_source, "root.commanderInput.toggle_camera_mode()"));
  EXPECT_TRUE(contains(game_view_source, "case Qt.Key_C:"));
  EXPECT_TRUE(contains(game_view_source, "game.commander_toggle_camera_mode()"));
  EXPECT_TRUE(contains(adapter_header, "Q_INVOKABLE void toggle_camera_mode();"));
  EXPECT_TRUE(contains(adapter_source, "m_engine->commander_toggle_camera_mode();"));
  EXPECT_TRUE(
      contains(engine_header, "Q_INVOKABLE void commander_toggle_camera_mode();"));
  EXPECT_TRUE(
      contains(engine_source, "void GameEngine::commander_toggle_camera_mode()"));
  EXPECT_TRUE(contains(controller_header, "void toggle_close_camera_mode("));
  EXPECT_TRUE(contains(controller_source,
                       "void CommanderControlController::toggle_close_camera_mode("));
}

TEST(CommanderControlRegressionTest, CommanderAbilityKitIsWiredThroughAdapter) {
  const auto root = find_repo_root();
  const auto layer_source = read_text(root / "ui" / "qml" / "CommanderInputLayer.qml");
  const auto game_view_source = read_text(root / "ui" / "qml" / "GameView.qml");
  const auto commander_hud_source =
      read_text(root / "ui" / "qml" / "HUDBottomCommander.qml");
  const auto adapter_header =
      read_text(root / "app" / "core" / "commander_input_adapter.h");
  const auto adapter_source =
      read_text(root / "app" / "core" / "commander_input_adapter.cpp");
  const auto engine_header = read_text(root / "app" / "core" / "game_engine.h");
  const auto engine_source = read_text(root / "app" / "core" / "game_engine.cpp");
  const auto controller_header =
      read_text(root / "app" / "core" / "commander_control_controller.h");
  ASSERT_FALSE(layer_source.empty());
  ASSERT_FALSE(game_view_source.empty());
  ASSERT_FALSE(commander_hud_source.empty());
  ASSERT_FALSE(adapter_header.empty());
  ASSERT_FALSE(adapter_source.empty());
  ASSERT_FALSE(engine_header.empty());
  ASSERT_FALSE(engine_source.empty());
  ASSERT_FALSE(controller_header.empty());

  EXPECT_TRUE(contains(layer_source, "case Qt.Key_1:"));
  EXPECT_TRUE(contains(layer_source, "root.commanderInput.vanguard_rush()"));
  EXPECT_TRUE(contains(layer_source, "case Qt.Key_2:"));
  EXPECT_TRUE(contains(layer_source, "root.commanderInput.second_wind()"));
  EXPECT_TRUE(contains(game_view_source, "case Qt.Key_1:"));
  EXPECT_TRUE(contains(game_view_source, "game.commander_vanguard_rush()"));
  EXPECT_TRUE(contains(game_view_source, "case Qt.Key_2:"));
  EXPECT_TRUE(contains(game_view_source, "game.commander_second_wind()"));
  EXPECT_TRUE(contains(commander_hud_source, "\"key\": \"1\""));
  EXPECT_TRUE(contains(commander_hud_source, "\"key\": \"2\""));
  EXPECT_TRUE(contains(commander_hud_source, "get_controlled_commander_status"));
  EXPECT_TRUE(contains(commander_hud_source, "cooldown_progress"));
  EXPECT_TRUE(contains(commander_hud_source, "vanguard_rush_cooldown_remaining"));
  EXPECT_TRUE(contains(commander_hud_source, "second_wind_cooldown_remaining"));
  EXPECT_TRUE(contains(commander_hud_source, "shield_bash_cooldown_remaining"));
  EXPECT_TRUE(contains(commander_hud_source, "Vanguard Rush"));
  EXPECT_TRUE(contains(commander_hud_source, "Second Wind"));
  EXPECT_TRUE(contains(adapter_header, "Q_INVOKABLE void vanguard_rush();"));
  EXPECT_TRUE(contains(adapter_header, "Q_INVOKABLE void second_wind();"));
  EXPECT_TRUE(contains(adapter_source, "m_engine->commander_vanguard_rush();"));
  EXPECT_TRUE(contains(adapter_source, "m_engine->commander_second_wind();"));
  EXPECT_TRUE(contains(engine_header, "Q_INVOKABLE void commander_vanguard_rush();"));
  EXPECT_TRUE(contains(engine_header, "Q_INVOKABLE void commander_second_wind();"));
  EXPECT_TRUE(contains(engine_source, "void GameEngine::commander_vanguard_rush()"));
  EXPECT_TRUE(contains(engine_source, "void GameEngine::commander_second_wind()"));
  EXPECT_TRUE(contains(controller_header, "void request_vanguard_rush();"));
  EXPECT_TRUE(contains(controller_header, "void request_second_wind();"));
}

TEST(CommanderControlRegressionTest, CommanderJumpAddsVisualLiftToRenderAndCamera) {
  const auto root = find_repo_root();
  const auto component_source = read_text(root / "game" / "core" / "component.h");
  const auto controller_source =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  const auto engine_source = read_text(root / "app" / "core" / "game_engine.cpp");
  const auto prepare_source = read_text(root / "render" / "humanoid" / "prepare.cpp");
  ASSERT_FALSE(component_source.empty());
  ASSERT_FALSE(controller_source.empty());
  ASSERT_FALSE(engine_source.empty());
  ASSERT_FALSE(prepare_source.empty());

  EXPECT_TRUE(contains(component_source, "bool jump_active{false};"));
  EXPECT_TRUE(contains(component_source, "float jump_phase{0.0F};"));
  EXPECT_TRUE(contains(component_source, "float jump_height_offset{0.0F};"));

  EXPECT_TRUE(contains(controller_source, "constexpr float k_jump_duration = 0.58F;"));
  EXPECT_TRUE(contains(controller_source,
                       "cmd_comp->jump_height_offset = jump_height_offset;"));
  EXPECT_TRUE(contains(controller_source, "m_jump_last_walkable_position"));
  EXPECT_TRUE(contains(controller_source, "m_jump_timer <= 0.0F"));

  EXPECT_TRUE(contains(engine_source, "commander_data->jump_active = false;"));

  EXPECT_TRUE(contains(prepare_source, "RCP::set_model_world_y("));
  EXPECT_TRUE(
      contains(prepare_source, "RCP::model_world_origin(inst_ctx.model).y() +"));
  EXPECT_TRUE(contains(prepare_source,
                       "locomotion_state.gait.state = "
                       "Render::GL::HumanoidMotionState::Idle;"));
  EXPECT_TRUE(
      contains(prepare_source, "anim_ctx.ambient_idle_type = AmbientIdleType::Jump;"));
}

TEST(CommanderControlRegressionTest,
     CommanderJumpAllowsAirborneTraversalAcrossGroundObstacles) {
  const auto root = find_repo_root();
  const auto controller_source =
      read_text(root / "app" / "core" / "commander_control_controller.cpp");
  const auto movement_source =
      read_text(root / "game" / "systems" / "movement_system.cpp");
  ASSERT_FALSE(controller_source.empty());
  ASSERT_FALSE(movement_source.empty());

  EXPECT_TRUE(contains(controller_source, "jump_active || is_walkable_at(nx, nz)"));
  EXPECT_TRUE(contains(controller_source, "m_jump_safe_position_valid"));
  EXPECT_TRUE(contains(movement_source, "commander->jump_active"));
}
