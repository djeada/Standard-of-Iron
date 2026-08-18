#include "app/core/game_engine.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLContext>
#include <QPainter>
#include <QPointer>
#include <QQuickWindow>
#include <QSet>
#include <QSize>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <qbuffer.h>
#include <qcoreapplication.h>
#include <qdir.h>
#include <qevent.h>
#include <qglobal.h>
#include <qimage.h>
#include <qjsonobject.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qpoint.h>
#include <qsize.h>
#include <qstringliteral.h>
#include <qstringview.h>
#include <qtmetamacros.h>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "app/audio/audio_coordinator.h"
#include "app/audio/audio_resource_loader.h"
#include "app/audio/audio_system_proxy.h"
#include "app/audio/weather_audio.h"
#include "app/commander/commander_mode_coordinator.h"
#include "app/commander/commander_status_builder.h"
#include "app/core/frame_ui_coordinator.h"
#include "app/core/game_speed.h"
#include "app/core/match_presentation_sync.h"
#include "app/core/user_settings.h"
#include "app/economy/production_manager.h"
#include "app/input/cursor_manager.h"
#include "app/input/cursor_mode.h"
#include "app/input/hover_tracker.h"
#include "app/input/input_command_handler.h"
#include "app/input/rts_camera_controller.h"
#include "app/models/selected_units_model.h"
#include "app/orders/action_vfx.h"
#include "app/orders/command_controller.h"
#include "app/orders/movement_utils.h"
#include "app/orders/order_submission.h"
#include "app/orders/rts_action_model.h"
#include "app/persistence/game_state_restorer.h"
#include "app/persistence/save_load_coordinator.h"
#include "app/session/environment.h"
#include "app/session/level_loader.h"
#include "app/session/loading_progress_tracker.h"
#include "app/session/renderer_bootstrap.h"
#include "app/session/skirmish_loader.h"
#include "app/session/skirmish_runtime_coordinator.h"
#include "app/session/world_bootstrap.h"
#include "app/utils/engine_view_helpers.h"
#include "app/viewmodels/save_slots_view_model.h"
#include "app/world/ambient_state_manager.h"
#include "app/world/minimap_manager.h"
#include "app/world/selection_query_service.h"
#include "app/world/unit_queries.h"
#include "app/world/visibility_coordinator.h"
#include "game/audio/audio_cues.h"
#include "game/audio/audio_event_handler.h"
#include "game/audio/audio_system.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/system.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/game_config.h"
#include "game/map/campaign_loader.h"
#include "game/map/map_catalog.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/mission_context.h"
#include "game/map/mission_loader.h"
#include "game/map/render_visibility_rules.h"
#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/mission_commander_setup.h"
#include "game/mission/mission_definition_view.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/mission/mission_waves.h"
#include "game/render_bridge/camera_service.h"
#include "game/render_bridge/minimap/map_preview_generator.h"
#include "game/render_bridge/minimap/minimap_generator.h"
#include "game/render_bridge/minimap/minimap_utils.h"
#include "game/render_bridge/minimap/unit_layer.h"
#include "game/render_bridge/picking_service.h"
#include "game/render_bridge/selection_controller.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/ai_system/ai_strategy.h"
#include "game/systems/attack_range.h"
#include "game/systems/attack_targeting.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/capture_system.h"
#include "game/systems/cleanup_system.h"
#include "game/systems/combat_rules.h"
#include "game/systems/combat_system.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/guard_system.h"
#include "game/systems/healing_system.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/movement_system.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/patrol_system.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/production_service.h"
#include "game/systems/production_system.h"
#include "game/systems/rain_manager.h"
#include "game/systems/rpg_combat_system/rpg_combat_processor.h"
#include "game/systems/save_load_service.h"
#include "game/systems/selection_system.h"
#include "game/systems/terrain_alignment_system.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/systems/undead_awakening_system.h"
#include "game/systems/victory_service.h"
#include "game/units/commander_catalog.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"
#include "game/util/asset_text.h"
#include "game/util/selection_utils.h"
#include "game/visuals/team_colors.h"
#include "render/camera_visibility.h"
#include "render/geom/stone.h"
#include "render/gl/bootstrap.h"
#include "render/ground/ambient_fog_renderer.h"
#include "render/ground/biome_renderer.h"
#include "render/ground/firecamp_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/ground_renderer.h"
#include "render/ground/olive_renderer.h"
#include "render/ground/pine_renderer.h"
#include "render/ground/plant_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/stone_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/scene_renderer.h"
#include "render/terrain_scene_proxy.h"
#include "scene/camera.h"
#include "utils/resource_utils.h"

namespace {

constexpr float k_mission_stage_poll_seconds = 0.25F;
constexpr float k_interaction_targeting_interval = 0.1F;

auto build_resource_map(int owner_id) -> QVariantMap {
  QVariantMap resources;
  Game::Systems::ResourceAmounts const amounts =
      Game::Systems::PlayerResourceRegistry::instance().get_all(owner_id);
  for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
    resources[QLatin1String(Game::Systems::resource_type_key(type))] =
        amounts.get(type);
  }
  return resources;
}

auto build_player_state_map(int owner_id, int population_cap) -> QVariantMap {
  QVariantMap state;
  state["owner_id"] = owner_id;
  state["population"] =
      Game::Systems::TroopCountRegistry::instance().get_troop_count(owner_id);
  state["population_cap"] = population_cap;
  state["resources"] = build_resource_map(owner_id);
  return state;
}

} // namespace

GameEngine::GameEngine(QObject* parent)
    : QObject(parent)
    , m_save_load_service(Game::Systems::SaveLoadService::instance())
    , m_selected_units_model(new SelectedUnitsModel(m_client, this)) {
  build_client_and_view_models();
  build_services_and_controllers();
}

GameEngine::~GameEngine() {

  m_autosave_timer.stop();
  if (m_save_load_service != nullptr) {

    m_save_load_service->wait_for_pending_saves();
    m_save_load_service->disconnect(this);
    m_save_load_service->shutdown();
  }

  if (m_audio_event_handler) {
    m_audio_event_handler->shutdown();
  }
  AudioSystem::get_instance().shutdown();
  qInfo() << "AudioSystem shut down";
}

void GameEngine::cleanup_opengl_resources() {
  qInfo() << "Cleaning up OpenGL resources...";

  QOpenGLContext* context = QOpenGLContext::currentContext();
  const bool has_valid_context = (context != nullptr);

  if (!has_valid_context) {
    qInfo() << "No valid OpenGL context, skipping OpenGL cleanup";
  }

  if (m_renderer && has_valid_context) {
    m_renderer->shutdown();
    qInfo() << "Renderer shut down";
  }

  m_terrain_scene.reset();

  m_surface.reset();
  m_features.reset();
  m_scatter.reset();
  m_fog.reset();
  m_boundary_fog.reset();
  m_ambient_fog.reset();
  m_rain.reset();
  if (m_weather_audio) {
    m_weather_audio->stop();
  }
  m_weather_audio.reset();
  m_rain_manager.reset();

  m_renderer.reset();
  m_resources.reset();

  qInfo() << "OpenGL resources cleaned up";
}

bool GameEngine::release_self_test_mission_ready() const {
  return m_runtime.initialized && !is_loading() &&
         m_match_setup_view_model->is_campaign_mission() && m_world != nullptr &&
         m_world->entity_count() > 0U && m_runtime.last_error.isEmpty();
}

QString GameEngine::release_self_test_pending_reason() const {
  QStringList pending;
  if (!m_runtime.initialized) {
    pending << QStringLiteral("engine not initialized");
  }

  if (m_runtime.loading) {
    pending << QStringLiteral("still loading (stage: %1, progress %2%)")
                   .arg(loading_stage_text())
                   .arg(static_cast<int>(loading_progress() * 100.0F));
  }
  if (m_loading_overlay_active) {

    pending << QStringLiteral(
                   "loading overlay still up (frames remaining %1, elapsed %2ms, "
                   "renderer %3, gpu resources %4, waiting for first frame %5)")
                   .arg(m_loading_overlay_frames_remaining)
                   .arg(m_loading_overlay_timer.isValid()
                            ? m_loading_overlay_timer.elapsed()
                            : -1)
                   .arg(m_renderer ? QStringLiteral("up") : QStringLiteral("null"))
                   .arg((m_renderer && m_renderer->resources() != nullptr)
                            ? QStringLiteral("up")
                            : QStringLiteral("null"))
                   .arg(m_loading_overlay_wait_for_first_frame.load(
                            std::memory_order_acquire)
                            ? QStringLiteral("yes")
                            : QStringLiteral("no"));
  }
  if (!m_match_setup_view_model->is_campaign_mission()) {
    pending << QStringLiteral("mission context is not a campaign mission");
  }
  if (m_world == nullptr) {
    pending << QStringLiteral("no world");
  } else if (m_world->entity_count() == 0U) {
    pending << QStringLiteral("world has no entities");
  }
  if (!m_runtime.last_error.isEmpty()) {
    pending << QStringLiteral("error: %1").arg(m_runtime.last_error);
  }
  if (pending.isEmpty()) {
    return QStringLiteral("nothing pending");
  }
  return pending.join(QStringLiteral("; "));
}

void GameEngine::apply_game_mode_render_policy() {
  const bool rpg = m_commander_view_model->rpg_mode();
  if (m_renderer != nullptr) {
    m_renderer->set_world_render_mode(rpg ? Render::GL::Renderer::WorldRenderMode::Rpg
                                          : Render::GL::Renderer::WorldRenderMode::Rts);
    m_renderer->set_rpg_camera_focus(
        rpg ? m_commander_view_model->controlled_commander_id() : 0);
  }
  if (m_fog != nullptr) {
    m_fog->set_soft_reveal_enabled(rpg);
  }
}

void GameEngine::set_active_camera(Render::GL::Camera* camera) {
  m_camera = camera;
  publish_client_context();
  if (m_renderer != nullptr) {
    m_renderer->set_camera(m_camera);
    if (m_viewport.width > 0 && m_viewport.height > 0) {
      m_renderer->set_viewport(m_viewport.width, m_viewport.height);
    }
  }
  Render::GL::CameraVisibility::instance().set_camera(m_camera);
}

void GameEngine::update_cursor(Qt::CursorShape new_cursor) {
  if (m_window == nullptr) {
    return;
  }
  if (m_runtime.current_cursor != new_cursor) {
    m_runtime.current_cursor = new_cursor;
    QPointer<QQuickWindow> const safe_window(m_window);
    QMetaObject::invokeMethod(
        m_window,
        [safe_window, new_cursor]() {
          if (safe_window != nullptr) {
            safe_window->setCursor(new_cursor);
          }
        },
        Qt::AutoConnection);
  }
}

void GameEngine::set_error(const QString& error_message) {
  if (m_runtime.last_error != error_message) {
    m_runtime.last_error = error_message;
    qCritical() << "GameEngine error:" << error_message;
    emit last_error_changed();
  }
}

void GameEngine::set_cursor_mode(CursorMode mode) {
  if (!m_cursor_manager) {
    return;
  }
  m_cursor_manager->set_mode(mode);
  m_cursor_manager->update_cursor_shape(m_window);
}

void GameEngine::set_cursor_mode(const QString& mode) {
  set_cursor_mode(CursorModeUtils::fromString(mode));
}

auto GameEngine::cursor_mode() const -> QString {
  if (!m_cursor_manager) {
    return "normal";
  }
  return m_cursor_manager->mode_string();
}

auto GameEngine::global_cursor_x() const -> qreal {
  if (!m_cursor_manager) {
    return 0;
  }
  return m_cursor_manager->global_cursor_x(m_window);
}

auto GameEngine::global_cursor_y() const -> qreal {
  if (!m_cursor_manager) {
    return 0;
  }
  return m_cursor_manager->global_cursor_y(m_window);
}

void GameEngine::ensure_initialized() {
  const bool was_initialized = m_runtime.initialized;
  QString error;
  App::Core::WorldBootstrap::ensure_initialized(m_runtime.initialized,
                                                *m_renderer,
                                                *m_camera,
                                                m_surface ? m_surface->ground()
                                                          : nullptr,
                                                &error);
  if (!error.isEmpty()) {
    set_error(error);
  }
  if (!was_initialized && m_runtime.initialized) {
    emit renderer_initialized_changed();
  }
}

auto GameEngine::enemy_troops_defeated() const -> int {
  return m_enemy_troops_defeated;
}

auto GameEngine::selected_player_state() const -> QVariantMap {
  return m_selected_player_state;
}

void GameEngine::set_selected_player_id(int id) {
  if (m_selected_player_id == id) {
    return;
  }
  m_selected_player_id = id;
  sync_selected_player_state();
  emit selected_player_id_changed();
}

auto GameEngine::scene_context() const -> AppSceneContext {
  return AppSceneContext{.session = m_session.get(),
                         .world = m_world,
                         .renderer = m_renderer.get(),
                         .active_camera = m_camera,
                         .ground = m_surface ? m_surface->ground() : nullptr,
                         .terrain = m_surface ? m_surface->terrain() : nullptr,
                         .features = m_features.get(),
                         .scatter = m_scatter.get(),
                         .fog = m_fog.get(),
                         .boundary_fog = m_boundary_fog.get(),
                         .ambient_fog = m_ambient_fog.get(),
                         .rain = m_rain.get(),
                         .minimap_manager = m_minimap_manager.get(),
                         .visibility_coordinator = m_visibility_coordinator.get(),
                         .victory_service = m_victory_service.get(),
                         .rain_manager = m_rain_manager.get(),
                         .weather_audio = m_weather_audio.get(),
                         .environment_clock = m_environment_clock.get()};
}

auto GameEngine::get_player_stats(int owner_id) -> QVariantMap {
  QVariantMap result;

  auto& stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  const auto* stats = stats_registry.get_stats(owner_id);

  if (stats != nullptr) {
    result["troopsRecruited"] = stats->troops_recruited;
    result["enemiesKilled"] = stats->enemies_killed;
    result["losses"] = stats->losses;
    result["barracksOwned"] = stats->barracks_owned;
    result["playTimeSec"] = stats->play_time_sec;
    result["gameEnded"] = stats->game_ended;
  } else {
    result["troopsRecruited"] = 0;
    result["enemiesKilled"] = 0;
    result["losses"] = 0;
    result["barracksOwned"] = 0;
    result["playTimeSec"] = 0.0F;
    result["gameEnded"] = false;
  }

  return result;
}

void GameEngine::note_dropped_simulation_ticks(std::uint64_t dropped, float real_dt) {
  m_dropped_tick_report_cooldown =
      std::max(0.0F, m_dropped_tick_report_cooldown - std::max(real_dt, 0.0F));
  if (dropped == 0) {
    return;
  }

  m_dropped_simulation_ticks += dropped;
  if (m_dropped_tick_report_cooldown > 0.0F) {
    return;
  }

  m_dropped_tick_report_cooldown = k_dropped_tick_report_interval;
  qWarning() << "Simulation could not keep up at" << m_runtime.time_scale
             << "x speed: dropped" << dropped << "tick(s) this frame,"
             << m_dropped_simulation_ticks << "since the mission started";
}

void GameEngine::update_active_runtime_simulation(float dt) {
  if (m_world == nullptr) {
    return;
  }

  if (m_commander_view_model->active()) {
    m_commander_view_model->update_control_mode(dt);
    m_world->update(dt);
    m_commander_view_model->restore_direct_control_if_ready();
    return;
  }

  m_world->update(dt);
  m_camera_view_model->update_follow();
  finish_replay_verification_if_done();
}

void GameEngine::finish_replay_verification_if_done() {
  if (!m_replay_verify_exit || m_session == nullptr) {
    return;
  }
  auto* player = m_session->replay_player();
  if (player == nullptr) {
    return;
  }
  const auto& file = player->file();
  const std::uint64_t last_recorded_tick = std::max<std::uint64_t>(
      file.last_tick(), file.digests.empty() ? 0U : file.digests.back().tick);
  if (!player->finished() || m_session->clock().tick() <= last_recorded_tick) {
    return;
  }
  if (const auto& divergence = player->divergence(); divergence.has_value()) {
    qCritical() << "SOI_REPLAY_VERIFY: FAIL - diverged at tick" << divergence->tick
                << "(recorded" << divergence->recorded << ", observed"
                << divergence->observed << ")";
    QCoreApplication::exit(12);
  } else {
    qInfo() << "SOI_REPLAY_VERIFY: PASS -" << player->fed_count() << "commands,"
            << player->checked_count() << "digests matched";
    QCoreApplication::exit(0);
  }
  m_replay_verify_exit = false;
}

void GameEngine::update(float dt) {
  if (m_runtime.loading) {
    return;
  }

  const float real_dt = dt;
  m_order_markers.update(dt, m_world);
  m_activity_view_model->advance_feedback(dt);

  float simulation_time_scale = 0.0F;
  if (!m_runtime.paused) {
    simulation_time_scale =
        m_runtime.time_scale * m_commander_view_model->time_effect_scale(
                                   dt * m_runtime.time_scale, m_runtime.paused);
  }

  update_mission_waves(dt * simulation_time_scale);
  update_mission_stages(dt * simulation_time_scale);

  RuntimeFrameState frame_state{
      .local_owner_id = m_runtime.local_owner_id,
      .spectator_mode = m_level.is_spectator_mode,
      .viewport_width = m_viewport.width,
      .viewport_height = m_viewport.height,
      .selection_refresh_enabled = (m_selected_units_model != nullptr),
      .selection_refresh_counter = m_runtime.selection_refresh_counter,
      .minimap_unit_update_accumulator = m_runtime.minimap_unit_update_accumulator,
      .simulation_time_scale = simulation_time_scale};
  const FrameUpdateCallbacks callbacks{
      .on_minimap_image_changed =
          [this]() { m_minimap_view_model->notify_image_changed(); },
      .on_selected_units_data_changed =
          [this]() {
            emit selected_units_data_changed();
          }};

  m_frame_orchestrator.update(
      scene_context(),
      frame_state,
      m_entity_cache,
      (!m_runtime.paused && !m_runtime.loading) ? m_ambient_state_manager.get()
                                                : nullptr,
      m_runtime.victory_state,
      dt,
      callbacks,
      [this](float step_dt) { update_active_runtime_simulation(step_dt); });
  m_runtime.selection_refresh_counter = frame_state.selection_refresh_counter;
  m_runtime.minimap_unit_update_accumulator =
      frame_state.minimap_unit_update_accumulator;
  note_dropped_simulation_ticks(frame_state.dropped_simulation_ticks, dt);
  sync_scatter_world_props();
  sync_selected_player_state();
  sync_economy_state();
  sync_attack_targeting();
  sync_interaction_targeting(dt);
  sync_attack_range_rings();
  sync_focus_targets();
  sync_target_focus_markers();
  update_tutorial(real_dt);
}

void GameEngine::render(int pixel_width, int pixel_height) {
  if (!m_renderer || !m_world || !m_runtime.initialized || m_runtime.loading) {
    return;
  }

  Render::GL::CameraVisibility::instance().set_camera(m_camera);

  if (pixel_width > 0 && pixel_height > 0) {
    m_viewport.width = pixel_width;
    m_viewport.height = pixel_height;
    m_renderer->set_viewport(pixel_width, pixel_height);
  }

  if (auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>()) {
    const auto& sel = selection_system->get_selected_units();
    std::vector<Engine::Core::EntityID> ids;
    ids.reserve(sel.size());
    for (const auto id : sel) {
      if (!m_commander_view_model->should_render_selected_entity(id)) {
        continue;
      }
      ids.push_back(id);
    }
    m_renderer->set_selected_entities(ids);
  }

  m_renderer->set_world_view(m_session != nullptr
                                 ? Render::WorldView::of(*m_session)
                                 : Render::WorldView::of_active_session());

  m_renderer->begin_frame();

  if (m_terrain_scene) {
    m_terrain_scene->submit(*m_renderer, m_renderer->resources());
  }

  if (m_renderer && m_hover_tracker) {
    m_renderer->set_hovered_entity_id(m_hover_tracker->get_last_hovered_entity());
  }
  if (m_renderer) {
    m_renderer->set_local_owner_id(m_runtime.local_owner_id);
    m_renderer->set_order_marker_spectator_mode(m_level.is_spectator_mode);
  }

  m_renderer->render_world(m_world);
  App::Core::FrameUiCoordinator::render_effects(
      {.renderer = m_renderer.get(),
       .world = m_world,
       .command_controller = m_command_controller.get(),
       .local_owner_id = m_runtime.local_owner_id,
       .commander_rally_preview_pos = m_commander_view_model->rally_preview_position(),
       .attack_targeting = &m_attack_targeting,
       .attack_range_rings = &m_attack_range_rings,
       .order_markers = &m_order_markers.markers(),
       .target_focus = &m_target_focus,
       .interaction_targeting = &m_interaction_targeting,
       .objective_marker = m_mission_stage_tracker.active_target()},
      [this]() { m_commander_view_model->render_effects(); });
  m_renderer->end_frame();

  update_loading_overlay();
  update_cursor_position();
}

void GameEngine::set_input_viewport_size(qreal width, qreal height) {
  if (width > 0.0 && height > 0.0) {
    m_viewport.input_width = width;
    m_viewport.input_height = height;
  }
}

void GameEngine::update_loading_overlay() {
  if (!m_loading_overlay_wait_for_first_frame.load(std::memory_order_acquire)) {
    return;
  }

  if (QThread::currentThread() != thread()) {
    QMetaObject::invokeMethod(
        this, [this]() { update_loading_overlay(); }, Qt::QueuedConnection);
    return;
  }

  if (!m_renderer || (m_renderer->resources() == nullptr)) {
    m_loading_overlay_frames_remaining = 5;
    m_loading_overlay_last_frame_ms = 0;
    m_loading_overlay_timer.restart();
    return;
  }

  if (m_loading_overlay_frames_remaining > 0) {
    m_loading_overlay_frames_remaining--;

    const qint64 now_ms =
        m_loading_overlay_timer.isValid() ? m_loading_overlay_timer.elapsed() : 0;
    qInfo().noquote() << QStringLiteral(
                             "SOI_LOADING_OVERLAY: frame %1 of 5 presented at %2ms "
                             "(+%3ms since the previous one)")
                             .arg(5 - m_loading_overlay_frames_remaining)
                             .arg(now_ms)
                             .arg(now_ms - m_loading_overlay_last_frame_ms);
    m_loading_overlay_last_frame_ms = now_ms;
  }

  constexpr qint64 k_loading_overlay_max_wait_ms = 15000;
  const qint64 elapsed_ms =
      m_loading_overlay_timer.isValid() ? m_loading_overlay_timer.elapsed() : 0;
  const bool enough_time = m_loading_overlay_timer.isValid() &&
                           (elapsed_ms >= m_loading_overlay_min_duration_ms);
  const bool exceeded_max_wait = m_loading_overlay_timer.isValid() &&
                                 (elapsed_ms >= k_loading_overlay_max_wait_ms);

  QStringList pending_components;
  const bool scatter_ready = !m_scatter || m_scatter->is_gpu_ready();

  if (!scatter_ready) {
    pending_components << QStringLiteral("terrain scatter");
  }

  const bool biome_gpu_ready = pending_components.isEmpty();

  if (enough_time && m_loading_overlay_frames_remaining <= 0 &&
      (biome_gpu_ready || exceeded_max_wait)) {
    if (exceeded_max_wait && !biome_gpu_ready) {
      qWarning() << "Loading overlay timed out waiting for GPU readiness"
                 << pending_components.join(", ");
    }
    m_loading_overlay_wait_for_first_frame.store(false, std::memory_order_release);
    m_loading_overlay_active = false;
    if (m_finalize_progress_after_overlay && m_loading_progress_tracker) {
      m_loading_progress_tracker->set_stage(
          LoadingProgressTracker::LoadingStage::COMPLETED);
    }
    m_finalize_progress_after_overlay = false;
    emit is_loading_changed();

    if (m_show_objectives_after_loading) {
      m_show_objectives_after_loading = false;
      m_match_setup_view_model->notify_current_mission_changed();
    }
  }
}

void GameEngine::update_cursor_position() {
  if (QThread::currentThread() != thread()) {
    QMetaObject::invokeMethod(
        this, [this]() { update_cursor_position(); }, Qt::QueuedConnection);
    return;
  }
  qreal const current_x = global_cursor_x();
  qreal const current_y = global_cursor_y();
  if (current_x != m_runtime.last_cursor_x || current_y != m_runtime.last_cursor_y) {
    m_runtime.last_cursor_x = current_x;
    m_runtime.last_cursor_y = current_y;
    emit global_cursor_changed();
  }
}

auto GameEngine::screen_to_ground(const QPointF& screen_pt,
                                  QVector3D& out_world) -> bool {
  return App::Utils::screen_to_ground(m_picking_service.get(),
                                      m_camera,
                                      m_window,
                                      m_viewport.width,
                                      m_viewport.height,
                                      screen_pt,
                                      out_world);
}

auto GameEngine::world_to_screen(const QVector3D& world,
                                 QPointF& out_screen) const -> bool {
  return App::Utils::world_to_screen(m_picking_service.get(),
                                     m_camera,
                                     m_window,
                                     m_viewport.width,
                                     m_viewport.height,
                                     world,
                                     out_screen);
}

void GameEngine::sync_selection_flags() {
  if (!m_world) {
    return;
  }
  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }

  Game::Selection::sanitize_selection(m_world, selection_system);
  const auto prune_effects =
      App::Core::FrameUiCoordinator::prune_selection_action_context(
          {.world = m_world,
           .cursor_manager = m_cursor_manager.get(),
           .production_manager = m_production_manager.get(),
           .command_controller = m_command_controller.get(),
           .local_owner_id = m_runtime.local_owner_id,
           .hud_action_states = m_orders_view_model->action_states()});
  if (prune_effects.cancel_construction) {
    m_placement_view_model->on_construction_cancel();
  }
  if (prune_effects.cancel_formation) {
    m_placement_view_model->on_formation_cancel();
  }
  switch (prune_effects.cursor_resolution) {
  case App::Core::FrameUiCoordinator::CursorResolution::CancelBarracksRallyPlacement:
    m_commander_view_model->cancel_barracks_rally();
    break;
  case App::Core::FrameUiCoordinator::CursorResolution::CancelCommanderFlagRally:
    m_commander_view_model->cancel_flag_rally();
    break;
  case App::Core::FrameUiCoordinator::CursorResolution::ResetToNormal:
    if (prune_effects.clear_patrol_first_waypoint && m_command_controller) {
      m_command_controller->clear_patrol_first_waypoint();
    }
    set_cursor_mode(CursorMode::Normal);
    break;
  case App::Core::FrameUiCoordinator::CursorResolution::None:
    break;
  }
}

void GameEngine::sync_attack_range_rings() {
  m_attack_range_rings =
      App::Core::PresentationSync::collect_attack_range_rings(attack_sync_context());
}

namespace {

auto accepted_order_cue(App::Core::OrderKind kind) -> const char* {
  switch (kind) {
  case App::Core::OrderKind::Move:
  case App::Core::OrderKind::Deliver:
  case App::Core::OrderKind::Repair:
    return Game::Audio::Cue::k_order_move;
  case App::Core::OrderKind::Attack:
    return Game::Audio::Cue::k_order_attack;
  case App::Core::OrderKind::Patrol:
    return Game::Audio::Cue::k_order_patrol;
  case App::Core::OrderKind::Stop:
    return Game::Audio::Cue::k_order_stop;
  case App::Core::OrderKind::Rally:
    return Game::Audio::Cue::k_order_rally_set;
  case App::Core::OrderKind::Build:
  case App::Core::OrderKind::Gather:
    return Game::Audio::Cue::k_build_placement_confirmed;
  case App::Core::OrderKind::Guard:
  case App::Core::OrderKind::Hold:
  case App::Core::OrderKind::Formation:
  case App::Core::OrderKind::None:
    break;
  }
  return nullptr;
}

} // namespace

void GameEngine::handle_order_feedback(const App::Core::OrderOutcome& outcome) {
  if (!outcome.issued()) {
    return;
  }

  m_order_markers.push(outcome, m_world);

  if (outcome.accepted()) {
    switch (outcome.kind) {
    case App::Core::OrderKind::Move:
      m_tutorial_notes.move_accepted = true;
      break;
    case App::Core::OrderKind::Attack:
      m_tutorial_notes.attack_accepted = true;
      break;
    case App::Core::OrderKind::Hold:
      m_tutorial_notes.hold_accepted = true;
      break;
    case App::Core::OrderKind::Guard:
      m_tutorial_notes.guard_accepted = true;
      break;
    case App::Core::OrderKind::Patrol:
      m_tutorial_notes.patrol_accepted = true;
      break;
    case App::Core::OrderKind::Gather:
      m_tutorial_notes.gather_accepted = true;
      break;
    case App::Core::OrderKind::Build:
      m_tutorial_notes.build_accepted = true;
      break;
    default:
      break;
    }
    m_tutorial_notes.last_rejection_reason.clear();
  } else {
    m_tutorial_notes.last_rejection_reason = outcome.reason;
  }

  QString message;
  if (outcome.accepted()) {
    if (const char* cue = accepted_order_cue(outcome.kind)) {
      Game::Audio::play_cue(cue);
    }
    if (outcome.kind == App::Core::OrderKind::Attack && outcome.target != 0) {
      App::Controllers::ActionVFX::spawn_attack_arrow(m_world, outcome.target);
    }
    message = App::Core::accepted_order_message(outcome);
  } else {
    Game::Audio::play_cue(Game::Audio::Cue::k_ui_error);
    message = outcome.reason;
  }

  emit order_feedback(QString::fromLatin1(App::Core::order_kind_name(outcome.kind)),
                      outcome.accepted(),
                      message);
}

void GameEngine::sync_attack_targeting() {
  auto result =
      App::Core::PresentationSync::collect_attack_targeting(attack_sync_context());
  m_attack_targeting = std::move(result.highlights);

  if ((m_activity_view_model == nullptr) || (m_attack_target_hint == result.hint)) {
    return;
  }
  m_attack_target_hint = result.hint;
  QMetaObject::invokeMethod(
      m_activity_view_model.get(),
      [view_model = m_activity_view_model.get(), hint = result.hint]() {
        view_model->set_attack_target_hint(hint);
      },
      Qt::QueuedConnection);
}

void GameEngine::sync_interaction_targeting(float delta_time) {
  m_interaction_targeting_accumulator += delta_time;
  if (m_interaction_targeting_accumulator < k_interaction_targeting_interval) {
    return;
  }
  m_interaction_targeting_accumulator = 0.0F;

  Game::Systems::InteractionTargetingHighlights highlights;
  QVariantMap hint;
  hint[QStringLiteral("action")] = QStringLiteral("none");

  if ((m_world != nullptr) && !m_level.is_spectator_mode) {
    std::vector<Engine::Core::EntityID> selection;
    if (auto* selection_system =
            m_world->get_system<Game::Systems::SelectionSystem>()) {
      selection = selection_system->get_selected_units();
    }

    Game::Systems::InteractionTargetingRequest request;
    request.world = m_world;
    request.local_owner_id = m_runtime.local_owner_id;

    for (const auto id : selection) {
      auto* entity = m_world->get_entity(id);
      const auto* unit = entity != nullptr
                             ? entity->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
      if (unit == nullptr || unit->owner_id != m_runtime.local_owner_id ||
          unit->health <= 0) {
        continue;
      }
      if (unit->spawn_type == Game::Units::SpawnType::Builder) {
        request.has_builders = true;
      } else if (unit->spawn_type == Game::Units::SpawnType::Civilian) {
        request.has_civilians = true;
      }
    }

    if (request.has_builders || request.has_civilians) {
      auto& visibility = Game::Map::VisibilityService::instance();
      const auto snapshot =
          visibility.is_initialized() ? visibility.snapshot_ptr() : nullptr;

      request.hovered_entity_id =
          m_hover_tracker ? m_hover_tracker->get_last_hovered_entity() : 0;
      if (m_camera != nullptr) {
        const QVector3D anchor = m_camera->get_target();
        request.anchor_x = anchor.x();
        request.anchor_z = anchor.z();
      }
      request.max_distance = Game::Systems::k_interaction_highlight_max_distance;
      request.max_markers = Game::Systems::k_interaction_highlight_max_markers;
      request.visibility = snapshot.get();

      QVector3D ground;
      if (screen_to_ground(QPointF(m_runtime.last_cursor_x, m_runtime.last_cursor_y),
                           ground)) {
        request.has_hovered_ground = true;
        request.hovered_ground_x = ground.x();
        request.hovered_ground_z = ground.z();
      }

      highlights = Game::Systems::collect_interaction_target_highlights(request);

      const auto action_key =
          Game::Systems::interaction_action_key(highlights.hovered_action);
      hint[QStringLiteral("action")] = QString::fromLatin1(
          action_key.data(), static_cast<qsizetype>(action_key.size()));
    }
  }

  m_interaction_targeting = std::move(highlights);

  if ((m_activity_view_model == nullptr) || (m_interaction_target_hint == hint)) {
    return;
  }
  m_interaction_target_hint = hint;
  QMetaObject::invokeMethod(
      m_activity_view_model.get(),
      [view_model = m_activity_view_model.get(), hint]() {
        view_model->set_interaction_target_hint(hint);
      },
      Qt::QueuedConnection);
}

auto GameEngine::attack_sync_context() const
    -> App::Core::PresentationSync::SelectionAttackContext {
  return {.world = m_world,
          .hover = m_hover_tracker.get(),
          .cursor = m_cursor_manager.get(),
          .camera = m_camera,
          .local_owner_id = m_runtime.local_owner_id,
          .spectator_mode = m_level.is_spectator_mode};
}

auto GameEngine::selected_units_model() -> QAbstractItemModel* {
  return m_selected_units_model;
}

auto GameEngine::audio_system() -> QObject* {
  return m_audio_systemProxy.get();
}

void GameEngine::set_audio_frontend_context(const QString& context) {
  const QString normalized = context.trimmed().toLower();
  if (m_audio_frontend_context == normalized) {
    return;
  }

  m_audio_frontend_context = normalized;
  m_audio_coordinator->apply_frontend_music_context(normalized);
}

void GameEngine::set_paused(bool paused) {
  if (m_runtime.paused == paused) {
    return;
  }
  m_runtime.paused = paused;
  m_tutorial_notes.speed_changed = true;
  if (m_runtime.loading) {
    return;
  }
  Game::Audio::play_cue(paused ? Game::Audio::Cue::k_state_pause
                               : Game::Audio::Cue::k_state_resume);
}

void GameEngine::set_game_speed(float speed) {
  const float sanitized = App::Core::GameSpeed::sanitize(speed);
  if (qFuzzyCompare(m_runtime.time_scale, sanitized)) {
    return;
  }
  m_runtime.time_scale = sanitized;
  m_tutorial_notes.speed_changed = true;
  Game::Audio::play_cue(Game::Audio::Cue::k_state_speed_change);
  emit time_scale_changed();
}

auto GameEngine::has_units_selected() const -> bool {
  if (!m_selection_controller) {
    return false;
  }
  return m_selection_controller->has_units_selected();
}

auto GameEngine::player_troop_count() const -> int {
  return m_entity_cache.player_troop_count;
}

void GameEngine::set_replay_record_path(const QString& path) {
  m_replay_record_path = path;
}

auto GameEngine::replay_playing() const -> bool {
  return m_session != nullptr && m_session->replay_player() != nullptr;
}

auto GameEngine::start_replay(const QString& path) -> bool {
  QString error;
  auto file = Game::Command::ReplayFile::load(path, &error);
  if (!file.has_value()) {
    set_error(tr("Cannot play replay: %1").arg(error));
    return false;
  }
  const Game::Command::ReplayHeader header = file->header;
  m_pending_replay = std::move(file);
  if (header.kind == QLatin1String("campaign-mission")) {
    m_match_setup_view_model->start_campaign_mission(header.reference);
  } else if (header.kind == QLatin1String("mission-file")) {
    m_match_setup_view_model->start_mission_file(header.reference);
  } else if (header.kind == QLatin1String("skirmish")) {
    m_match_setup_view_model->start_skirmish(
        header.reference,
        header.launch.value(QLatin1String("player_configs")).toArray().toVariantList());
  } else {
    m_pending_replay.reset();
    set_error(tr("Cannot play replay: unknown launch kind '%1'").arg(header.kind));
    return false;
  }
  return true;
}

void GameEngine::arm_replay_for_started_match() {
  if (m_session == nullptr) {
    return;
  }
  if (m_pending_replay.has_value()) {
    auto file = std::move(*m_pending_replay);
    m_pending_replay.reset();
    qInfo() << "Replay: driving" << m_replay_launch.kind << m_replay_launch.reference
            << "from" << file.commands.size() << "commands, last tick"
            << file.last_tick();
    m_session->set_replay_player(
        std::make_unique<Game::Command::ReplayPlayer>(std::move(file)));
    return;
  }
  if (m_replay_record_path.isEmpty()) {
    return;
  }
  Game::Command::ReplayHeader header;
  header.kind = m_replay_launch.kind;
  header.reference = m_replay_launch.reference;
  header.launch["player_configs"] =
      QJsonArray::fromVariantList(m_replay_launch.player_configs);
  header.tick_seconds = m_session->clock().tick_seconds();
  header.rng_seed = m_session->rng_seed();
  auto recorder = std::make_unique<Game::Command::ReplayRecorder>();
  if (!recorder->begin(m_replay_record_path, header, m_session->commands())) {
    qWarning() << "Replay: cannot write" << m_replay_record_path;
    return;
  }
  qInfo() << "Replay: recording to" << m_replay_record_path;
  m_session->set_replay_recorder(std::move(recorder));
}

void GameEngine::start_skirmish_internal(const QString& map_path,
                                         const QVariantList& player_configs,
                                         bool set_skirmish_context) {

  clear_error();
  reset_preload_interaction_state();
  reset_mission_runtime_state();

  m_level.map_path = map_path;
  m_level.map_name = map_path;

  if (m_campaign_manager && set_skirmish_context) {
    m_campaign_manager->set_skirmish_context(map_path);
  }

  if (!m_runtime.victory_state.isEmpty()) {
    m_runtime.victory_state = "";
    emit victory_state_changed();
  }
  if (m_victory_service) {
    m_victory_service->reset();
  }
  m_enemy_troops_defeated = 0;

  if (!m_runtime.initialized) {
    ensure_initialized();
  }

  if (!m_world || !m_renderer || (m_camera == nullptr) || !m_skirmish_runtime) {
    set_error(tr("Cannot start skirmish: renderer not initialized"));
    return;
  }

  m_finalize_progress_after_overlay = false;
  m_loading_overlay_active = true;
  m_runtime.loading = true;
  emit is_loading_changed();

  if (m_loading_progress_tracker) {
    m_loading_progress_tracker->start_loading();
  }

  QCoreApplication::processEvents(QEventLoop::AllEvents);
  if (m_release_self_test_mode) {

    qInfo() << "SOI_AUDIO_SELF_TEST: mission preload skipped after manifest "
               "validation";
  } else {
    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
  }
  QTimer::singleShot(50, this, [this, map_path, player_configs]() {
    if (!m_world || !m_renderer || (m_camera == nullptr) || !m_skirmish_runtime) {
      set_error(tr("Cannot start skirmish: renderer not initialized"));
      m_runtime.loading = false;
      emit is_loading_changed();
      return;
    }

    if (m_hover_tracker) {
      m_hover_tracker->update_hover(-1, -1, *m_world, *m_camera, 0, 0);
    }

    const bool allow_default_player_barracks =
        !m_campaign_manager ||
        !m_campaign_manager->current_mission_context().is_campaign();
    const auto load_effects =
        m_skirmish_runtime->perform_load({*m_world,
                                          m_level,
                                          m_entity_cache,
                                          map_path,
                                          player_configs,
                                          m_selected_player_id,
                                          scene_context(),
                                          m_victory_service.get(),
                                          m_minimap_manager.get(),
                                          m_visibility_coordinator.get(),
                                          allow_default_player_barracks,
                                          m_loading_progress_tracker.get(),
                                          [this]() {
                                            emit owner_info_changed();
                                          }});

    if (load_effects.selected_player_changed) {
      m_selected_player_id = load_effects.updated_player_id;
      emit selected_player_id_changed();
    }

    if (!load_effects.success) {
      set_error(load_effects.error);
      m_runtime.loading = false;
      m_loading_overlay_active = false;
      m_loading_overlay_wait_for_first_frame.store(false, std::memory_order_release);
      m_finalize_progress_after_overlay = false;
      m_show_objectives_after_loading = false;
      emit is_loading_changed();
      return;
    }

    m_runtime.local_owner_id = load_effects.updated_player_id;
    publish_client_context();
    m_audio_coordinator->configure_audio_manifest_mappings(m_runtime.local_owner_id);
    const Game::Mission::MissionDefinition* mission_def = nullptr;
    if (m_campaign_manager &&
        m_campaign_manager->current_mission_definition().has_value()) {
      mission_def = &*m_campaign_manager->current_mission_definition();
    }
    const Game::Mission::MissionDefinition* campaign_mission_def = nullptr;
    if (m_campaign_manager &&
        m_campaign_manager->current_mission_context().is_campaign() &&
        m_campaign_manager->current_mission_definition().has_value()) {
      campaign_mission_def = &*m_campaign_manager->current_mission_definition();
    }
    m_audio_coordinator->apply_mission_ambience(
        mission_def, map_path, m_runtime.local_owner_id);

    apply_skirmish_commander_setup(player_configs);
    apply_mission_setup();
    m_skirmish_runtime->initialize_player_resources(
        {m_level, m_runtime.local_owner_id, campaign_mission_def});
    configure_mission_victory_conditions();
    configure_rain_system();
    if (m_environment_clock) {
      m_environment_clock->reset(m_level.environment);
    }

    const auto finalize_effects = m_skirmish_runtime->finalize_load(
        {m_runtime.loading,
         m_loading_overlay_wait_for_first_frame,
         m_loading_overlay_frames_remaining,
         m_loading_overlay_min_duration_ms,
         m_loading_overlay_timer,
         m_finalize_progress_after_overlay,
         m_show_objectives_after_loading,
         m_match_setup_view_model->is_campaign_mission()});

    if (finalize_effects.emit_is_loading_changed) {
      emit is_loading_changed();
    }
    if (finalize_effects.rebuild_entity_cache) {
      GameStateRestorer::rebuild_entity_cache(
          m_world, m_entity_cache, m_runtime.local_owner_id);
    }
    if (finalize_effects.emit_troop_count_changed) {
      emit troop_count_changed();
    }
    if (finalize_effects.sync_scatter_world_props) {
      sync_scatter_world_props();
    }
    if (finalize_effects.sync_selected_player_state) {
      sync_selected_player_state();
    }
    if (finalize_effects.reset_ambient_state) {
      m_ambient_state_manager = std::make_unique<AmbientStateManager>();
      Engine::Core::EventManager::instance().publish(
          Engine::Core::AmbientStateChangedEvent(Engine::Core::AmbientState::PEACEFUL,
                                                 Engine::Core::AmbientState::PEACEFUL));
    }
    if (finalize_effects.apply_spectator_mode && m_input_handler) {
      m_input_handler->set_spectator_mode(m_level.is_spectator_mode);
    }
    if (finalize_effects.emit_owner_info_changed) {
      emit owner_info_changed();
    }
    if (finalize_effects.emit_spectator_mode_changed) {
      emit spectator_mode_changed();
    }
    arm_replay_for_started_match();
    activate_tutorial_if_configured();
  });
}

void GameEngine::apply_mission_setup() {
  if (!m_world || !m_campaign_manager || !m_mission_setup || !m_skirmish_runtime) {
    return;
  }

  std::vector<Game::Mission::PendingMissionWave> waves;
  auto effects = m_mission_setup->apply_mission_setup({*m_world,
                                                       *m_campaign_manager,
                                                       m_level,
                                                       m_selected_player_id,
                                                       m_runtime.local_owner_id,
                                                       waves});
  std::vector<Game::Mission::PendingMissionEvent> events;
  if (m_campaign_manager->current_mission_definition().has_value()) {
    events = Game::Mission::build_pending_mission_events(
        *m_campaign_manager->current_mission_definition());
  }
  m_mission_waves.bind_after_setup(
      mission_wave_binding(), std::move(waves), std::move(events));
  configure_mission_stages();
  if (effects.rebuild_entity_cache) {
    GameStateRestorer::rebuild_entity_cache(
        m_world, m_entity_cache, m_runtime.local_owner_id);
  }
  if (effects.selected_player_changed) {
    emit selected_player_id_changed();
  }
  if (effects.center_camera_on_local_forces) {
    m_skirmish_runtime->center_camera_on_local_forces(
        {m_world, m_camera, m_runtime.local_owner_id});
  }
  if (effects.troop_count_changed) {
    emit troop_count_changed();
  }
  if (effects.owner_info_changed) {
    emit owner_info_changed();
  }
}

void GameEngine::configure_mission_victory_conditions() {
  if (!m_campaign_manager || !m_victory_service) {
    return;
  }

  m_campaign_manager->configure_mission_victory_conditions(m_victory_service.get(),
                                                           m_runtime.local_owner_id);

  m_victory_service->set_victory_callback([this](const QString& state) {
    if (m_runtime.victory_state != state) {
      m_audio_coordinator->ensure_result_audio_ready(state, m_runtime.local_owner_id);
      if (state == "defeat") {
        Game::Audio::play_cue(Game::Audio::Cue::k_alert_objective_failed);
      }
      m_runtime.victory_state = state;
      emit victory_state_changed();

      if (state == "victory" && !m_campaign_manager->current_campaign_id().isEmpty()) {
        m_match_setup_view_model->mark_current_mission_completed();
      }
    }
  });
}

void GameEngine::configure_rain_system() {
  if (m_rain_manager) {
    m_rain_manager->configure(m_level.rain, m_level.biome_seed);
  }

  if (m_weather_audio) {
    m_weather_audio->stop();
    if (m_level.rain.enabled) {
      m_weather_audio->preload(m_level.rain.type);
    }
  }

  if (!m_rain) {
    return;
  }

  const float world_width = static_cast<float>(m_level.grid_width) * m_level.tile_size;
  const float world_height =
      static_cast<float>(m_level.grid_height) * m_level.tile_size;
  m_rain->configure(world_width, world_height, m_level.biome_seed, m_level.rain.type);
  m_rain->set_enabled(m_level.rain.enabled);
  m_rain->set_wind_strength(m_level.rain.wind_strength);
  m_rain->set_wind_direction_deg(m_level.rain.wind_direction_deg);

  const float initial_intensity =
      m_rain_manager ? m_rain_manager->get_intensity()
                     : (m_level.rain.enabled ? m_level.rain.intensity : 0.0F);
  m_rain->set_intensity(initial_intensity);
}

void GameEngine::reset_preload_interaction_state() {
  m_commander_view_model->reset_for_new_match();
  if (m_command_controller) {
    m_command_controller->reset_transient_state();
  }

  if (m_production_manager) {
    m_production_manager->reset_transient_state();
  }

  if (m_world) {
    if (auto* selection_system =
            m_world->get_system<Game::Systems::SelectionSystem>()) {
      selection_system->clear_selection();
    }
  }

  if (m_renderer) {
    m_renderer->set_selected_entities({});
    m_renderer->set_hovered_entity_id(0);
  }

  if (m_hover_tracker && m_world && (m_camera != nullptr)) {
    m_hover_tracker->update_hover(-1, -1, *m_world, *m_camera, 0, 0);
  }

  if (m_cursor_manager && m_cursor_manager->mode() != CursorMode::Normal) {
    set_cursor_mode(CursorMode::Normal);
  }

  m_camera_view_model->set_following_selection(false);
  m_commander_view_model->reset_for_new_match();
  m_runtime.selection_refresh_counter = 0;
  m_runtime.minimap_unit_update_accumulator = 0.0F;

  emit selected_units_changed();
}

void GameEngine::reset_mission_runtime_state() {
  if (m_tutorial_director) {
    m_tutorial_director->end();
  }
  m_tutorial_notes.reset();
  m_tutorial_observe_accumulator = 0.0F;
  m_runtime.minimap_unit_update_accumulator = 0.0F;
  m_mission_waves.reset();
  m_mission_stage_tracker.clear();
  m_mission_stage_poll_accumulator = 0.0F;
  m_interaction_targeting = {};
  m_interaction_targeting_accumulator = 0.0F;
  m_interaction_target_hint.clear();
  if (m_wave_view_model) {
    m_wave_view_model->clear();
  }
  if (m_mission_view_model) {
    m_mission_view_model->clear();
  }
  Game::Systems::PlayerResourceRegistry::instance().clear();
  sync_selected_player_state();
  reset_economy_coach();
  m_audio_coordinator->stop_mission_ambience();
  AudioSystem::get_instance().stop_music();
  AudioResourceLoader::unload_audio_resources(AudioLoadPolicy::Mission);
  AudioResourceLoader::unload_audio_resources(AudioLoadPolicy::Lazy);
}

void GameEngine::update_mission_waves(float dt) {
  if (!m_world || !m_mission_setup || !m_runtime.victory_state.isEmpty()) {
    return;
  }
  const bool tutorial_holds_clock =
      m_tutorial_director && m_tutorial_director->holds_mission_clock();

  const auto effects =
      m_mission_waves.advance(mission_wave_binding(), dt, tutorial_holds_clock);

  for (const auto& announcement : effects.announcements) {
    emit mission_announcement(announcement);
  }
  for (const auto& cue : effects.audio_cues) {
    Game::Audio::play_cue(cue.toStdString());
  }
  if (effects.reward_granted) {
    auto& resources = Game::Systems::PlayerResourceRegistry::instance();
    for (const auto type : Game::Systems::k_all_resource_types) {
      const int amount = effects.reward.get(type);
      if (amount > 0) {
        resources.add(m_runtime.local_owner_id, type, amount);
      }
    }
    sync_selected_player_state();
  }
  if (effects.wave_status_changed) {
    publish_wave_status();
  }
  if (effects.owner_info_changed) {
    emit owner_info_changed();
  }
}

void GameEngine::restore_mission_waves(const QJsonObject& wave_state) {
  m_mission_waves.restore(mission_wave_binding(), wave_state);
  configure_mission_stages();
  publish_wave_status();
}

auto GameEngine::mission_wave_binding() -> App::Mission::MissionWaveBinding {
  return {.world = m_world,
          .level = &m_level,
          .campaign = m_campaign_manager.get(),
          .victory_service = m_victory_service.get(),
          .local_owner_id = m_runtime.local_owner_id};
}

void GameEngine::configure_mission_stages() {
  m_mission_stage_tracker.clear();
  m_mission_stage_poll_accumulator = 0.0F;

  if (m_campaign_manager == nullptr ||
      !m_campaign_manager->current_mission_definition().has_value()) {
    publish_mission_stages();
    return;
  }

  const auto& mission = *m_campaign_manager->current_mission_definition();
  m_mission_stage_tracker.configure(
      mission,
      m_runtime.local_owner_id,
      Game::Mission::make_mission_position_to_world(m_level));

  if (m_session && m_mission_stage_tracker.has_stages()) {
    m_mission_stage_tracker.update(
        *m_session,
        {.elapsed_seconds = m_mission_waves.elapsed(),
         .cleared_wave_count = m_mission_waves.director().cleared_wave_count()});
  }
  publish_mission_stages();
}

void GameEngine::restore_mission_stages(const QJsonObject& stage_state) {
  m_mission_stage_tracker.restore(stage_state);
  publish_mission_stages();
}

void GameEngine::update_mission_stages(float delta_time) {
  if (!m_mission_stage_tracker.has_stages() || !m_session) {
    return;
  }

  m_mission_stage_poll_accumulator += delta_time;
  if (m_mission_stage_poll_accumulator < k_mission_stage_poll_seconds) {
    return;
  }
  m_mission_stage_poll_accumulator = 0.0F;

  const bool changed = m_mission_stage_tracker.update(
      *m_session,
      {.elapsed_seconds = m_mission_waves.elapsed(),
       .cleared_wave_count = m_mission_waves.director().cleared_wave_count()});
  if (changed) {
    publish_mission_stages();
  }
}

void GameEngine::publish_mission_stages() {
  if (!m_mission_view_model) {
    return;
  }
  if (!m_mission_stage_tracker.has_stages()) {
    m_mission_view_model->clear();
    return;
  }

  const bool has_minimap = m_minimap_manager && m_minimap_manager->has_minimap();
  const float world_width = has_minimap ? m_minimap_manager->get_world_width() : 0.0F;
  const float world_height = has_minimap ? m_minimap_manager->get_world_height() : 0.0F;

  QVariantList stages;
  int index = 0;
  for (const auto& status : m_mission_stage_tracker.stages()) {
    QVariantMap entry;
    entry["id"] = status.id;
    entry["index"] = index;
    entry["type"] = status.type;
    entry["title"] = Game::Util::tr_asset(Game::Util::k_missions_context, status.title);
    entry["description"] =
        Game::Util::tr_asset(Game::Util::k_missions_context, status.description);
    entry["hint"] = Game::Util::tr_asset(Game::Util::k_missions_context, status.hint);
    entry["progress"] = status.progress;
    entry["required"] = status.required;
    entry["complete"] = status.complete;
    entry["active"] = status.active;
    entry["has_target"] = status.has_target;
    if (status.has_target) {
      entry["world_x"] = status.target.x();
      entry["world_z"] = status.target.z();
      if (has_minimap) {
        const auto [nx, ny] = Game::Map::Minimap::world_to_pixel(status.target.x(),
                                                                 status.target.z(),
                                                                 world_width,
                                                                 world_height,
                                                                 1.0F,
                                                                 1.0F);
        entry["nx"] = std::clamp(nx, 0.0F, 1.0F);
        entry["ny"] = std::clamp(ny, 0.0F, 1.0F);
      }
    }
    stages.append(entry);
    ++index;
  }

  m_mission_view_model->set_stages(stages);
}

void GameEngine::publish_wave_status() {
  if (!m_wave_view_model) {
    return;
  }

  QVariantMap status = m_mission_waves.status();
  QVariantList alerts = status.value("alerts").toList();
  if (!alerts.isEmpty() && m_minimap_manager && m_minimap_manager->has_minimap()) {
    const float world_width = m_minimap_manager->get_world_width();
    const float world_height = m_minimap_manager->get_world_height();
    QVariantList normalized;
    for (const auto& value : alerts) {
      QVariantMap alert = value.toMap();
      const auto [nx, ny] =
          Game::Map::Minimap::world_to_pixel(alert.value("x").toFloat(),
                                             alert.value("z").toFloat(),
                                             world_width,
                                             world_height,
                                             1.0F,
                                             1.0F);
      alert["nx"] = std::clamp(nx, 0.0F, 1.0F);
      alert["ny"] = std::clamp(ny, 0.0F, 1.0F);
      normalized.append(alert);
    }
    status["alerts"] = normalized;
  }

  m_wave_view_model->set_status(status);
}

void GameEngine::apply_skirmish_commander_setup(const QVariantList& player_configs) {
  if (!m_world || !m_mission_setup) {
    return;
  }

  const auto effects = m_mission_setup->apply_skirmish_commander_setup(
      {*m_world, m_campaign_manager.get(), m_level, m_runtime.local_owner_id},
      player_configs);
  for (const auto& announcement : effects.mission_announcements) {
    emit mission_announcement(announcement);
  }
}

void GameEngine::open_settings() {
  if (m_save_load_service != nullptr) {
    Game::Systems::SaveLoadService::open_settings();
  }
}

void GameEngine::connect_save_service_signals() {
  if (m_save_load_service == nullptr) {
    return;
  }

  connect(
      m_save_load_service,
      &Game::Systems::SaveLoadService::save_progress,
      this,
      [this](
          quint64 job_id, const QString& slot_name, int percent, const QString& stage) {
        if (job_id != m_active_save_job) {
          return;
        }
        m_save_slots_view_model->set_save_progress(true, percent, stage, slot_name);
      });

  connect(m_save_load_service,
          &Game::Systems::SaveLoadService::save_finished,
          this,
          [this](quint64 job_id,
                 const QString& slot_name,
                 bool success,
                 const QString& error) {
            if (job_id == m_active_save_job) {
              m_active_save_job = 0;
              m_save_slots_view_model->set_save_progress(
                  false, success ? 100 : 0, QString(), m_save_progress_slot);
            }
            if (!success) {
              set_error(error);
              Game::Audio::play_cue(Game::Audio::Cue::k_ui_error);
            } else {
              Game::Audio::play_cue(Game::Audio::Cue::k_state_save_complete);
            }
            emit m_save_slots_view_model->save_completed(slot_name, success, error);
          });

  connect(m_save_load_service,
          &Game::Systems::SaveLoadService::save_slots_changed,
          m_save_slots_view_model.get(),
          &App::ViewModels::SaveSlotsViewModel::save_slots_changed);

  connect(&m_autosave_timer, &QTimer::timeout, this, &GameEngine::autosave);
  restart_autosave_timer();
}

void GameEngine::restart_autosave_timer() {
  const int minutes = m_save_slots_view_model->autosave_interval_minutes();
  if (minutes <= 0) {
    m_autosave_timer.stop();
    return;
  }
  m_autosave_timer.setInterval(minutes * 60 * 1000);
  m_autosave_timer.start();
}

void GameEngine::begin_save(const QString& slot_name,
                            Game::Systems::Save::SlotKind kind,
                            int autosave_retention) {
  if ((m_save_load_service == nullptr) || !m_world) {
    set_error(tr("Save: not initialized"));
    return;
  }

  if (m_active_save_job != 0) {
    set_error(tr("A save is already in progress"));
    return;
  }

  if (m_commander_view_model->active()) {
    m_commander_view_model->exit_mode();
  }
  const Game::Systems::RuntimeSnapshot runtime_snapshot = to_runtime_snapshot();
  Game::Systems::LevelSnapshot level_snapshot = m_level;
  if (m_environment_clock) {
    level_snapshot.environment = m_environment_clock->definition();
    level_snapshot.environment_clock = m_environment_clock->snapshot();
  }
  if (m_rain_manager) {
    level_snapshot.weather_runtime = m_rain_manager->snapshot();
  }
  std::optional<Game::Mission::MissionContext> mission_context;
  if (m_campaign_manager) {
    mission_context = m_campaign_manager->current_mission_context();
  }

  const App::Core::SaveToSlotEffects effects =
      m_save_load_coordinator->begin_save_to_slot(
          {.world = *m_world,
           .save_load_service = *m_save_load_service,
           .camera = m_camera,
           .level = level_snapshot,
           .runtime_snapshot = runtime_snapshot,
           .slot = slot_name,
           .title = slot_name,
           .map_name = m_level.map_name,
           .mission_context = std::move(mission_context),
           .kind = kind,
           .play_time_seconds = m_mission_waves.elapsed(),
           .autosave_retention = autosave_retention,
           .mission_wave_state = m_mission_waves.director().serialize(),
           .mission_stage_state = m_mission_stage_tracker.serialize()});
  if (!effects.queued) {
    set_error(effects.error);
    return;
  }

  m_active_save_job = effects.job_id;
  m_save_progress_slot = slot_name;
  m_save_slots_view_model->set_save_progress(true, 0, tr("Queued"), slot_name);

  m_screenshot_target_slot = slot_name;
  m_screenshot_requested.store(true, std::memory_order_release);
}

void GameEngine::save_game_to_slot(const QString& slot_name) {
  begin_save(slot_name, Game::Systems::Save::SlotKind::Manual, 0);
}

void GameEngine::quicksave() {
  begin_save(QStringLiteral("quicksave"), Game::Systems::Save::SlotKind::Quicksave, 0);
}

void GameEngine::autosave() {

  if ((m_save_load_service == nullptr) || !m_world || !m_runtime.initialized ||
      m_runtime.loading || m_level.map_path.isEmpty() ||
      !m_runtime.victory_state.isEmpty() || m_active_save_job != 0) {
    return;
  }

  const int retention = m_save_slots_view_model->autosave_slot_count();
  begin_save(m_save_load_service->next_autosave_slot(retention),
             Game::Systems::Save::SlotKind::Autosave,
             retention);
}

void GameEngine::cancel_active_save() {
  if (m_active_save_job == 0 || (m_save_load_service == nullptr)) {
    return;
  }
  m_save_load_service->cancel_save(m_active_save_job);
  m_save_slots_view_model->set_save_progress(
      true,
      m_save_slots_view_model->save_progress_percent(),
      tr("Cancelling..."),
      m_save_progress_slot);
}

void GameEngine::load_game_from_slot(const QString& slot_name) {
  if ((m_save_load_service == nullptr) || !m_world) {
    set_error(tr("Load: not initialized"));
    return;
  }

  if (m_commander_view_model->active()) {
    m_commander_view_model->exit_mode();
  }

  reset_preload_interaction_state();
  reset_mission_runtime_state();

  m_finalize_progress_after_overlay = false;
  m_loading_overlay_active = true;
  m_runtime.loading = true;
  emit is_loading_changed();

  Game::Systems::RuntimeSnapshot runtime_snapshot = to_runtime_snapshot();
  const App::Core::LoadFromSlotEffects effects =
      m_save_load_coordinator->load_from_slot(
          {.world = *m_world,
           .save_load_service = *m_save_load_service,
           .slot = slot_name,
           .campaign_manager = m_campaign_manager.get(),
           .level = m_level,
           .camera = m_camera,
           .viewport_width = m_viewport.width,
           .viewport_height = m_viewport.height,
           .runtime_snapshot = runtime_snapshot,
           .apply_runtime_snapshot =
               [this](const Game::Systems::RuntimeSnapshot& snapshot) {
                 apply_runtime_snapshot(snapshot);
               },
           .selected_player_id = m_selected_player_id,
           .scene = scene_context(),
           .entity_cache = m_entity_cache,
           .audio_coordinator = m_audio_coordinator.get(),
           .victory_service = m_victory_service.get(),
           .emit_troop_count_changed = [this]() { emit troop_count_changed(); },
           .restore_mission_waves =
               [this](const QJsonObject& wave_state) {
                 restore_mission_waves(wave_state);
               },
           .restore_mission_stages =
               [this](const QJsonObject& stage_state) {
                 restore_mission_stages(stage_state);
               }});
  if (!effects.success) {
    set_error(effects.error);
    m_runtime.loading = false;
    m_loading_overlay_active = false;
    m_loading_overlay_wait_for_first_frame.store(false, std::memory_order_release);
    m_finalize_progress_after_overlay = false;
    m_show_objectives_after_loading = false;
    emit is_loading_changed();
    return;
  }
  if (m_environment_clock) {
    m_environment_clock->restore(m_level.environment, m_level.environment_clock);
    if (m_renderer) {
      m_renderer->set_environment_lighting(m_environment_clock->lighting());
    }
  }

  if (m_rain_manager) {

    configure_rain_system();
    m_rain_manager->restore(m_level.weather_runtime);
    if (m_rain) {
      m_rain->set_intensity(m_rain_manager->get_intensity());
    }
  }

  sync_scatter_world_props();

  if (m_camera_controller) {
    m_camera_controller->sync_map_bounds();
  }

  m_runtime.loading = false;
  m_loading_overlay_wait_for_first_frame.store(true, std::memory_order_release);
  m_loading_overlay_frames_remaining = 5;
  m_loading_overlay_last_frame_ms = 0;
  m_loading_overlay_min_duration_ms = 1000;
  m_loading_overlay_timer.restart();
  m_finalize_progress_after_overlay = true;
  emit is_loading_changed();
  qInfo() << "Game load complete, victory/defeat checks re-enabled";
  Game::Audio::play_cue(Game::Audio::Cue::k_state_load_complete);

  m_minimap_view_model->notify_image_changed();

  if (effects.emit_selected_units_changed) {
    emit selected_units_changed();
  }
  if (effects.emit_owner_info_changed) {
    emit owner_info_changed();
  }
}

auto GameEngine::to_runtime_snapshot() const -> Game::Systems::RuntimeSnapshot {
  return m_save_load_coordinator->to_runtime_snapshot(
      {.paused = m_runtime.paused,
       .time_scale = m_runtime.time_scale,
       .local_owner_id = m_runtime.local_owner_id,
       .victory_state = m_runtime.victory_state,
       .cursor_mode = m_runtime.cursor_mode,
       .selected_player_id = m_selected_player_id,
       .follow_selection = m_camera_view_model->following_selection()});
}

void GameEngine::apply_runtime_snapshot(
    const Game::Systems::RuntimeSnapshot& snapshot) {
  bool follow_selection = m_camera_view_model->following_selection();
  m_save_load_coordinator->apply_runtime_snapshot(
      snapshot,
      {.paused = m_runtime.paused,
       .time_scale = m_runtime.time_scale,
       .local_owner_id = m_runtime.local_owner_id,
       .victory_state = m_runtime.victory_state,
       .cursor_mode = m_runtime.cursor_mode,
       .selected_player_id = m_selected_player_id,
       .follow_selection = follow_selection});
  m_camera_view_model->set_following_selection(follow_selection);
  m_runtime.time_scale = App::Core::GameSpeed::sanitize(m_runtime.time_scale);
  emit time_scale_changed();
  if (m_cursor_manager) {
    m_cursor_manager->set_mode(m_runtime.cursor_mode);
  }
  sync_selected_player_state();
}

auto GameEngine::describe_focus_entity(Engine::Core::EntityID id) const
    -> App::Core::FocusTargetInfo {
  App::Core::FocusTargetInfo info;
  if (m_world == nullptr || id == Engine::Core::NULL_ENTITY) {
    return info;
  }
  auto* entity = m_world->get_entity(id);
  const auto* unit = entity != nullptr
                         ? entity->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
  if (unit == nullptr || unit->health <= 0) {
    return info;
  }

  App::World::UnitDescription described;
  if (!App::World::describe_unit(m_world, id, described) || !described.alive) {
    return info;
  }
  QString name = described.name;
  if (described.is_building) {
    const QString pretty = App::Core::building_display_name(unit->spawn_type);
    if (!pretty.isEmpty()) {
      name = pretty;
    }
  }

  info.valid = true;
  info.id = id;
  info.name = name;
  info.nation = described.nation;
  (void)App::World::unit_type_key(m_world, id, info.type_key);
  info.owner_id = unit->owner_id;
  info.is_building = described.is_building;
  info.is_own = unit->owner_id == m_runtime.local_owner_id;
  info.is_enemy =
      !info.is_own &&
      (m_session != nullptr
           ? m_session->owners().are_enemies(m_runtime.local_owner_id, unit->owner_id)
           : true);
  info.health = described.health;
  info.max_health = described.max_health;
  info.health_ratio =
      described.max_health > 0
          ? static_cast<double>(std::clamp(described.health, 0, described.max_health)) /
                static_cast<double>(described.max_health)
          : 0.0;
  const auto activity = App::World::unit_activity(m_world, id);
  info.activity =
      QString::fromUtf8(Game::Systems::activity_kind_id(activity.kind).data());
  info.activity_state =
      QString::fromUtf8(Game::Systems::activity_state_id(activity.state).data());

  std::vector<Engine::Core::EntityID> selection;
  get_selected_unit_ids(selection);
  info.attacked_by_selection =
      App::Core::count_selection_attacking(m_world, selection, id);
  info.attacked_by_local =
      App::Core::count_units_attacking(m_world, id, m_runtime.local_owner_id);
  info.attackers_incoming =
      App::Core::count_enemies_attacking(m_world, id, m_runtime.local_owner_id);
  return info;
}

void GameEngine::sync_focus_targets() {
  QVariantMap inspect;
  QVariantMap target;
  if (m_world != nullptr) {
    auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
    if (selection_system != nullptr) {
      const auto& selection = selection_system->get_selected_units();
      const auto inspected = selection_system->inspected_entity();
      const auto focus = App::Core::resolve_focus_entity(
          m_world, selection, inspected, m_runtime.local_owner_id);
      if (inspected != Engine::Core::NULL_ENTITY && focus != inspected) {
        selection_system->clear_inspected_entity();
      }
      const auto inspect_info = describe_focus_entity(focus);
      if (inspect_info.valid) {
        inspect = App::Core::focus_target_to_variant(inspect_info);
      }
      const auto primary = App::Core::primary_attack_target(m_world, selection);
      const auto target_info = describe_focus_entity(primary);
      if (target_info.valid) {
        target = App::Core::focus_target_to_variant(target_info);
      }
    }
  }
  if (inspect == m_inspect_target && target == m_selection_target) {
    return;
  }
  m_inspect_target = std::move(inspect);
  m_selection_target = std::move(target);
  if (m_activity_view_model != nullptr) {
    m_activity_view_model->set_focus_targets(m_inspect_target, m_selection_target);
  }
}

void GameEngine::sync_target_focus_markers() {
  m_target_focus.clear();
  if (m_world == nullptr || m_level.is_spectator_mode) {
    return;
  }
  auto* selection_system = m_world->get_system<Game::Systems::SelectionSystem>();
  if (selection_system == nullptr) {
    return;
  }
  const auto snapshot = m_visibility_coordinator != nullptr
                            ? m_visibility_coordinator->current_snapshot()
                            : nullptr;
  Game::Systems::TargetFocusRequest request;
  request.world = m_world;
  request.local_owner_id = m_runtime.local_owner_id;
  request.selection = &selection_system->get_selected_units();
  request.inspected = selection_system->inspected_entity();
  request.max_locked_targets = Game::Systems::k_target_focus_max_locked;
  request.max_incoming_attackers = Game::Systems::k_target_focus_max_incoming;
  request.visibility =
      (snapshot != nullptr && snapshot->initialized) ? snapshot.get() : nullptr;
  request.owners = m_session != nullptr ? &m_session->owners() : nullptr;
  m_target_focus = Game::Systems::collect_target_focus_markers(request);
}

void GameEngine::sync_selected_player_state() {
  int const owner_id =
      m_selected_player_id > 0 ? m_selected_player_id : m_runtime.local_owner_id;
  QVariantMap const next_state =
      build_player_state_map(owner_id, m_level.max_troops_per_player);
  if (m_selected_player_state == next_state) {
    return;
  }
  m_selected_player_state = next_state;
  emit selected_player_state_changed();
  emit owner_info_changed();
}

namespace {

constexpr qint64 k_economy_refresh_interval_ms = 250;

} // namespace

auto GameEngine::mission_objective_resources() const -> Game::Systems::ResourceAmounts {
  Game::Systems::ResourceAmounts required;
  if (!m_campaign_manager) {
    return required;
  }
  const auto& mission = m_campaign_manager->current_mission_definition();
  if (!mission.has_value()) {
    return required;
  }
  const auto note = [&required](const std::vector<Game::Mission::Condition>& list) {
    for (const auto& condition : list) {
      if (!condition.resources.has_value()) {
        continue;
      }
      for (const auto type : Game::Systems::k_all_resource_types) {
        required.set(type,
                     std::max(required.get(type), condition.resources->get(type)));
      }
    }
  };
  note(mission->victory_conditions);
  note(mission->optional_objectives);
  return required;
}

void GameEngine::reset_economy_coach() {
  m_economy_coach_baseline = {};
  m_economy_coach_available = false;
  m_economy_resources.clear();
  m_economy_help.clear();
  m_economy_coach.clear();
  if (m_economy_view_model) {
    QMetaObject::invokeMethod(
        m_economy_view_model.get(),
        [view_model = m_economy_view_model.get()]() { view_model->clear(); },
        Qt::QueuedConnection);
  }
}

void GameEngine::sync_economy_state() {
  if (!m_economy_view_model || m_world == nullptr || m_runtime.loading) {
    return;
  }
  if (m_economy_refresh_timer.isValid() &&
      m_economy_refresh_timer.elapsed() < k_economy_refresh_interval_ms) {
    return;
  }
  m_economy_refresh_timer.restart();

  int const owner_id =
      m_selected_player_id > 0 ? m_selected_player_id : m_runtime.local_owner_id;
  auto& nations = m_session->nations();
  const auto* nation = nations.get_nation_for_player(owner_id);
  const App::Core::EconomyOverviewRequest request{
      .world = m_world,
      .nations = &nations,
      .resources = &m_session->economy(),
      .owner_id = owner_id,
      .nation_id = nation != nullptr ? nation->id : nations.default_nation_id(),
      .population_cap = m_level.max_troops_per_player,
      .objective_resources = mission_objective_resources()};

  const bool coach_available = !m_level.is_spectator_mode &&
                               !m_match_setup_view_model->is_campaign_mission() &&
                               owner_id == m_runtime.local_owner_id &&
                               (nation == nullptr || nation->has_economy);
  if (coach_available && !m_economy_coach_baseline.captured) {
    m_economy_coach_baseline = App::Core::capture_economy_coach_baseline(request);
  }

  QVariantList resources = App::Core::build_resource_overview(request);
  QVariantMap help = App::Core::build_production_help(request);
  QVariantMap coach =
      coach_available
          ? App::Core::build_economy_coach_state(request, m_economy_coach_baseline)
          : QVariantMap{};

  const bool resources_changed = resources != m_economy_resources;
  const bool help_changed = help != m_economy_help;
  const bool coach_changed = coach != m_economy_coach;
  const bool availability_changed = coach_available != m_economy_coach_available;
  if (!resources_changed && !help_changed && !coach_changed && !availability_changed) {
    return;
  }
  m_economy_coach_available = coach_available;
  if (resources_changed) {
    m_economy_resources = resources;
  }
  if (help_changed) {
    m_economy_help = help;
  }
  if (coach_changed) {
    m_economy_coach = coach;
  }
  QMetaObject::invokeMethod(
      m_economy_view_model.get(),
      [view_model = m_economy_view_model.get(),
       resources = std::move(resources),
       help = std::move(help),
       coach = std::move(coach),
       resources_changed,
       help_changed,
       coach_changed,
       coach_available]() {
        if (resources_changed) {
          view_model->set_resources(resources);
        }
        if (help_changed) {
          view_model->set_help(help);
        }
        if (coach_changed) {
          view_model->set_coach(coach);
        }
        view_model->set_coach_available(coach_available);
      },
      Qt::QueuedConnection);
}

void GameEngine::sync_scatter_world_props() {
  auto& terrain_service = Game::Map::TerrainService::instance();
  if (m_scatter == nullptr || !terrain_service.is_initialized() ||
      terrain_service.get_height_map() == nullptr) {
    return;
  }

  auto const revision = terrain_service.world_props_revision();
  if (revision == m_last_world_props_revision) {
    return;
  }

  m_scatter->refresh_runtime_world_props(terrain_service.world_props());
  m_last_world_props_revision = revision;
}

auto GameEngine::consume_screenshot_request() -> bool {
  return m_screenshot_requested.exchange(false, std::memory_order_acq_rel);
}

void GameEngine::submit_frame_image(const QImage& image) {
  if (image.isNull()) {
    return;
  }

  QMetaObject::invokeMethod(
      this, [this, image]() { on_frame_image_captured(image); }, Qt::QueuedConnection);
}

void GameEngine::on_frame_image_captured(const QImage& image) {
  const QString slot_name = m_screenshot_target_slot;
  m_screenshot_target_slot.clear();
  if (slot_name.isEmpty() || (m_save_load_service == nullptr) || image.isNull()) {
    return;
  }

  const QByteArray png = Game::Systems::Save::encode_preview(image);
  if (png.isEmpty()) {
    qWarning() << "GameEngine: failed to encode save preview for" << slot_name;
    return;
  }

  m_save_load_service->attach_screenshot(slot_name, png);
}

void GameEngine::exit_game() {
  if (m_save_load_service != nullptr) {
    Game::Systems::SaveLoadService::exit_game();
  }
}

auto GameEngine::get_owner_info() const -> QVariantList {
  QVariantList result;
  const auto& owner_registry = Game::Systems::OwnerRegistry::instance();
  const auto& owners = owner_registry.get_all_owners();

  for (const auto& owner : owners) {
    QVariantMap owner_map;
    owner_map["id"] = owner.owner_id;
    owner_map["name"] = QString::fromStdString(owner.name);
    owner_map["team_id"] = owner.team_id;

    QString type_str;
    switch (owner.type) {
    case Game::Systems::OwnerType::Player:
      type_str = "Player";
      break;
    case Game::Systems::OwnerType::AI:
      type_str = "AI";
      break;
    case Game::Systems::OwnerType::Neutral:
      type_str = "Neutral";
      break;
    }
    owner_map["type"] = type_str;
    owner_map["isLocal"] = (owner.owner_id == m_runtime.local_owner_id);
    owner_map["state"] =
        build_player_state_map(owner.owner_id, m_level.max_troops_per_player);

    result.append(owner_map);
  }

  return result;
}

auto GameEngine::local_player_nation() const -> QString {
  const auto* nation = Game::Systems::NationRegistry::instance().get_nation_for_player(
      m_runtime.local_owner_id);
  if (nation == nullptr) {
    return {};
  }
  return QString::fromStdString(Game::Systems::nation_id_to_string(nation->id));
}

void GameEngine::get_selected_unit_ids(std::vector<Engine::Core::EntityID>& out) const {
  out.clear();
  if (!m_selection_controller) {
    return;
  }
  m_selection_controller->get_selected_unit_ids(out);
}

void GameEngine::on_unit_spawned(const Engine::Core::UnitSpawnedEvent& event) {
  auto& owners = Game::Systems::OwnerRegistry::instance();

  if (event.owner_id == m_runtime.local_owner_id) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.player_barracks_alive = true;
    } else {
      int const production_cost =
          Game::Units::TroopConfig::instance().get_production_cost(event.spawn_type);
      m_entity_cache.player_troop_count += production_cost;
    }
  } else if (owners.is_ai(event.owner_id)) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.enemy_barracks_count++;
      m_entity_cache.enemy_barracks_alive = true;
    }
  }

  auto emit_if_changed = [&] {
    if (m_entity_cache.player_troop_count != m_runtime.last_troop_count) {
      m_runtime.last_troop_count = m_entity_cache.player_troop_count;
      emit troop_count_changed();
    }
  };
  emit_if_changed();
  if (event.owner_id == m_runtime.local_owner_id) {
    const auto troop_type = Game::Units::spawn_typeToTroopType(event.spawn_type);
    if (troop_type.has_value() && Game::Units::is_commander_troop(*troop_type)) {
      m_commander_view_model->notify_availability_changed();
    }
  }
}

void GameEngine::on_unit_died(const Engine::Core::UnitDiedEvent& event) {
  auto& owners = Game::Systems::OwnerRegistry::instance();

  if (event.owner_id == m_runtime.local_owner_id) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.player_barracks_alive = false;
    } else {
      int const production_cost =
          Game::Units::TroopConfig::instance().get_production_cost(event.spawn_type);
      m_entity_cache.player_troop_count -= production_cost;
      m_entity_cache.player_troop_count =
          std::max(0, m_entity_cache.player_troop_count);
    }
  } else if (owners.is_ai(event.owner_id)) {
    if (event.spawn_type == Game::Units::SpawnType::Barracks) {
      m_entity_cache.enemy_barracks_count--;
      m_entity_cache.enemy_barracks_count =
          std::max(0, m_entity_cache.enemy_barracks_count);
      m_entity_cache.enemy_barracks_alive = (m_entity_cache.enemy_barracks_count > 0);
    }
  }
  if (event.owner_id == m_runtime.local_owner_id) {
    const auto troop_type = Game::Units::spawn_typeToTroopType(event.spawn_type);
    if (troop_type.has_value() && Game::Units::is_commander_troop(*troop_type)) {
      if (m_commander_view_model->controlled_commander_id() == event.unit_id) {
        m_commander_view_model->exit_mode();
      }
      m_commander_view_model->notify_availability_changed();
    }
  }
}

float GameEngine::loading_progress() const {
  if (m_loading_progress_tracker) {
    return m_loading_progress_tracker->progress();
  }
  return 0.0F;
}

QString GameEngine::loading_stage_text() const {
  if (m_loading_progress_tracker) {
    auto stage = m_loading_progress_tracker->current_stage();
    auto stage_name = m_loading_progress_tracker->stage_name(stage);
    auto detail = m_loading_progress_tracker->current_detail();
    if (!detail.isEmpty()) {
      return stage_name + " - " + detail;
    }
    return stage_name;
  }
  return {};
}

auto GameEngine::camera_view_model() const -> QObject* {
  return m_camera_view_model.get();
}

auto GameEngine::match_setup_view_model() const -> QObject* {
  return m_match_setup_view_model.get();
}

auto GameEngine::production_view_model() const -> QObject* {
  return m_production_view_model.get();
}

auto GameEngine::orders_view_model() const -> QObject* {
  return m_orders_view_model.get();
}

void GameEngine::launch_match(const App::Core::MatchLaunch& launch) {
  clear_error();
  m_replay_launch = {launch.kind, launch.reference, launch.player_configs};
  start_skirmish_internal(
      launch.map_path, launch.player_configs, launch.set_skirmish_context);
}

auto GameEngine::commander_view_model() const -> QObject* {
  return m_commander_view_model.get();
}

auto GameEngine::minimap_view_model() const -> QObject* {
  return m_minimap_view_model.get();
}

auto GameEngine::save_slots_view_model() const -> QObject* {
  return m_save_slots_view_model.get();
}

auto GameEngine::placement_view_model() const -> QObject* {
  return m_placement_view_model.get();
}

auto GameEngine::wave_view_model() const -> QObject* {
  return m_wave_view_model.get();
}

auto GameEngine::mission_view_model() const -> QObject* {
  return m_mission_view_model.get();
}

auto GameEngine::tutorial_view_model() const -> QObject* {
  return m_tutorial_director.get();
}

void GameEngine::activate_tutorial_if_configured() {
  if (!m_tutorial_director) {
    return;
  }
  const bool tutorial_mission =
      m_campaign_manager != nullptr &&
      m_campaign_manager->current_mission_definition().has_value() &&
      m_campaign_manager->current_mission_definition()->tutorial;
  m_tutorial_notes.reset();
  m_tutorial_observe_accumulator = 0.0F;
  if (tutorial_mission) {
    m_tutorial_director->begin();
  } else {
    m_tutorial_director->end();
  }
}

void GameEngine::update_tutorial(float real_dt) {
  if (!m_tutorial_director || !m_tutorial_director->active()) {
    m_tutorial_notes.reset();
    return;
  }

  constexpr float k_observe_interval = 0.2F;
  m_tutorial_observe_accumulator += std::max(0.0F, real_dt);
  if (m_tutorial_observe_accumulator < k_observe_interval) {
    return;
  }
  const float elapsed = m_tutorial_observe_accumulator;
  m_tutorial_observe_accumulator = 0.0F;
  m_tutorial_director->advance(
      App::Mission::observe_tutorial_frame(
          {.world = m_world,
           .notes = m_tutorial_notes,
           .local_owner_id = m_runtime.local_owner_id,
           .victory_state = m_runtime.victory_state,
           .enemy_troops_defeated = m_enemy_troops_defeated,
           .mission_running = m_runtime.initialized && !is_loading(),
           .placement = m_placement_view_model.get(),
           .wave_status = m_mission_waves.status()}),
      elapsed);
  m_tutorial_notes.reset();
}

auto GameEngine::activity_view_model() const -> QObject* {
  return m_activity_view_model.get();
}

auto GameEngine::economy_view_model() const -> QObject* {
  return m_economy_view_model.get();
}

void GameEngine::publish_client_context() {
  m_client.session = m_session.get();
  m_client.world = m_world;
  m_client.level = &m_level;
  m_client.local_owner_id = m_runtime.local_owner_id;

  m_client.renderer = m_renderer.get();
  m_client.active_camera = m_camera;
  m_client.rts_camera = m_rts_camera.get();
  m_client.commander_camera = m_commander_camera.get();

  m_client.picking = m_picking_service.get();
  m_client.selection = m_selection_controller.get();

  m_client.camera_controller = m_camera_controller.get();
  m_client.minimap = m_minimap_manager.get();
  m_client.visibility = m_visibility_coordinator.get();
  m_client.input = m_input_handler.get();
  m_client.commands = m_command_controller.get();
  m_client.production = m_production_manager.get();
  m_client.cursor = m_cursor_manager.get();

  m_client.campaign = m_campaign_manager.get();
  m_client.map_catalog = m_map_catalog.get();
  m_client.saves = m_save_load_service;

  m_client.viewport = &m_viewport;
  m_client.window = m_window;
}
