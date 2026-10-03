#include "app/core/game_engine.h"

#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QImage>
#include <QJsonArray>
#include <QOpenGLContext>
#include <QPointer>
#include <QQuickWindow>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/audio/audio_coordinator.h"
#include "app/audio/audio_resource_loader.h"
#include "app/core/audio_services.h"
#include "app/core/frame_ui_coordinator.h"
#include "app/core/game_speed.h"
#include "app/core/match_presentation_sync.h"
#include "app/core/order_feedback_presenter.h"
#include "app/core/presentation_frame.h"
#include "app/economy/economy_read_model.h"
#include "app/economy/production_manager.h"
#include "app/input/cursor_manager.h"
#include "app/input/hover_tracker.h"
#include "app/input/rts_camera_controller.h"
#include "app/mission/commander_message_runtime.h"
#include "app/mission/mission_runtime.h"
#include "app/mission/tutorial_runtime.h"
#include "app/models/loading_tips.h"
#include "app/models/selected_units_model.h"
#include "app/orders/action_vfx.h"
#include "app/orders/command_controller.h"
#include "app/orders/order_feedback.h"
#include "app/persistence/game_state_restorer.h"
#include "app/persistence/save_load_coordinator.h"
#include "app/persistence/save_slot_controller.h"
#include "app/session/environment_runtime.h"
#include "app/session/loading_progress_tracker.h"
#include "app/session/renderer_bootstrap.h"
#include "app/session/replay_coordinator.h"
#include "app/session/skirmish_runtime_coordinator.h"
#include "app/session/world_bootstrap.h"
#include "app/utils/engine_view_helpers.h"
#include "app/viewmodels/activity_view_model.h"
#include "app/viewmodels/camera_view_model.h"
#include "app/viewmodels/commander_message_view_model.h"
#include "app/viewmodels/commander_view_model.h"
#include "app/viewmodels/economy_view_model.h"
#include "app/viewmodels/match_setup_view_model.h"
#include "app/viewmodels/minimap_view_model.h"
#include "app/viewmodels/mission_view_model.h"
#include "app/viewmodels/orders_view_model.h"
#include "app/viewmodels/placement_view_model.h"
#include "app/viewmodels/production_view_model.h"
#include "app/viewmodels/save_slots_view_model.h"
#include "app/viewmodels/wave_view_model.h"
#include "app/world/ally_announcements.h"
#include "app/world/battle_stats.h"
#include "app/world/focus_tracker.h"
#include "app/world/minimap_events.h"
#include "app/world/minimap_manager.h"
#include "app/world/selection_query_service.h"
#include "app/world/targeting_presentation.h"
#include "app/world/visibility_coordinator.h"
#include "game/audio/audio_cues.h"
#include "game/audio/audio_system.h"
#include "game/audio/cue_trace.h"
#include "game/core/component_gameplay.h"
#include "game/core/startup_profiler.h"
#include "game/core/world.h"
#include "game/map/map_catalog.h"
#include "game/map/map_context.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/difficulty_forces.h"
#include "game/render_bridge/camera_service.h"
#include "game/render_bridge/picking_service.h"
#include "game/render_bridge/selection_controller.h"
#include "game/session/selection_service.h"
#include "game/session/selection_utils.h"
#include "game/session/session_context.h"
#include "game/session/session_snapshot.h"
#include "game/session/simulation_clock.h"
#include "game/systems/ai_system.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/persistence/save_load_service.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/troop_count_registry.h"
#include "game/systems/victory_service.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"
#include "render/camera_visibility.h"
#include "render/entity/building_archetype_library.h"
#include "render/geom/projectile_renderer.h"
#include "render/gl/shared_geometry_cache.h"
#include "render/ground/ambient_fog_renderer.h"
#include "render/ground/fog_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/terrain_feature_manager.h"
#include "render/ground/terrain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/ground/terrain_surface_manager.h"
#include "render/profiling/frame_profile.h"
#include "render/profiling/performance_report.h"
#include "render/scene_renderer.h"
#include "render/terrain_scene_proxy.h"

namespace {

constexpr auto k_render_frame_wait_budget = std::chrono::milliseconds(2000);
constexpr auto k_render_frame_wait_poll = std::chrono::microseconds(250);

} // namespace

GameEngine::GameEngine(QObject* parent)
    : QObject(parent)
    , m_save_load_service(Game::Systems::SaveLoadService::instance())
    , m_selected_units_model(new SelectedUnitsModel(m_client, this)) {
  build_client_and_view_models();
  build_services_and_controllers();
}

GameEngine::~GameEngine() {
  Game::Session::SessionSnapshot::forget_contributor("victory");
  stop_simulation_thread();
  Game::Systems::PickingService::unbind_surface(m_world);

  if (m_saves) {
    m_saves->shutdown();
  }
  if (m_audio) {
    m_audio->shutdown(m_level.map_path.toStdString());
  }
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
  m_environment->release();

  m_renderer.reset();
  m_resources.reset();
  m_runtime.initialized = false;

  qInfo() << "OpenGL resources cleaned up";
}

bool GameEngine::release_self_test_mission_ready() const {
  return m_runtime.initialized && !is_loading() &&
         m_match_setup_view_model->is_mission_match() && m_world != nullptr &&
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
  if (m_loading_overlay.active()) {
    pending << m_loading_overlay.describe(
        m_renderer != nullptr, m_renderer && m_renderer->resources() != nullptr);
  }
  if (!m_match_setup_view_model->is_mission_match()) {
    pending << QStringLiteral("mission context is not an authored mission");
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
  sync_render_camera();
  if (m_renderer != nullptr) {
    m_renderer->set_camera(&m_render_camera);
    if (m_viewport.width > 0 && m_viewport.height > 0) {
      m_renderer->set_viewport(m_viewport.width, m_viewport.height);
    }
  }
  Render::GL::CameraVisibility::instance().set_camera(&m_render_camera);
}

void GameEngine::sync_render_camera() {
  if (m_camera == nullptr) {
    return;
  }
  m_render_camera = *m_camera;

  const QVector3D listener_position = m_camera->get_target();
  Game::Audio::CueTrace::instance().set_listener(
      {.x = listener_position.x(),
       .y = listener_position.y(),
       .z = listener_position.z(),
       .mode = m_commander_view_model->active() ? "commander" : "rts"});

  const QVector3D listener_right = m_camera->get_right_vector();
  AudioSystem::get_instance().set_listener({.position = {listener_position.x(),
                                                         listener_position.y(),
                                                         listener_position.z()},
                                            .right_x = listener_right.x(),
                                            .right_z = listener_right.z(),
                                            .valid = true});
}

void GameEngine::capture_render_selection() {
  auto* selection_system =
      m_world != nullptr ? &Game::Session::session_for(*m_world).selection() : nullptr;
  if (selection_system == nullptr) {
    return;
  }
  const auto& sel = selection_system->get_selected_units();
  m_scratch_selected_ids.clear();
  m_scratch_selected_ids.reserve(sel.size());
  for (const auto id : sel) {
    if (!m_commander_view_model->should_render_selected_entity(id)) {
      continue;
    }
    m_scratch_selected_ids.push_back(id);
  }
  if (m_scratch_selected_ids != m_selected_render_ids) {
    m_selected_render_ids = m_scratch_selected_ids;
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
  if (!m_renderer || (m_camera == nullptr)) {
    return;
  }
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
  return m_battle_stats->enemy_troops_defeated();
}

auto GameEngine::selected_player_state() const -> QVariantMap {
  return m_economy->selected_player_state();
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
                         .rain_manager = m_environment->rain_manager(),
                         .weather_audio = m_environment->weather_audio(),
                         .environment_clock = m_environment->clock()};
}

auto GameEngine::get_player_stats(int owner_id) -> QVariantMap {
  const std::lock_guard<std::recursive_mutex> frame_lock(m_lifecycle.frame_mutex());
  return App::World::BattleStats::player_stats(*m_session, owner_id);
}

void GameEngine::note_dropped_simulation_ticks(std::uint64_t dropped, float real_dt) {
  m_dropped_tick_report_cooldown =
      std::max(0.0F, m_dropped_tick_report_cooldown - std::max(real_dt, 0.0F));
  if (dropped == 0) {
    return;
  }

  m_dropped_simulation_ticks += dropped;
  Render::Profiling::global_profile().dropped_sim_ticks.store(
      m_dropped_simulation_ticks, std::memory_order_relaxed);
  if (m_dropped_tick_report_cooldown > 0.0F) {
    return;
  }

  m_dropped_tick_report_cooldown = k_dropped_tick_report_interval;
  qWarning() << "Simulation could not keep up at" << m_runtime.time_scale
             << "x speed: dropped" << dropped << "tick(s) this frame,"
             << m_dropped_simulation_ticks << "since the mission started";
}

auto GameEngine::simulation_profile_report() -> QJsonObject {
  const auto frame_lock = lock_frame();
  if (m_world == nullptr || !m_world->system_profiler().enabled()) {
    return {};
  }
  return Render::Profiling::system_profiler_json(m_world->system_profiler());
}

void GameEngine::update_active_runtime_simulation(float dt) {
  if (m_world == nullptr) {
    return;
  }

  static const bool profile_simulation =
      qEnvironmentVariableIntValue("SOI_PROFILE_SIMULATION") != 0;
  if (profile_simulation) {
    m_world->system_profiler().set_enabled(true);
  }
  if (m_commander_view_model->active()) {
    m_commander_view_model->update_control_mode(dt);
    m_world->update(dt);
    m_commander_view_model->restore_direct_control_if_ready();
    return;
  }

  m_world->update(dt);
  m_replay->finish_verification_if_done(m_session.get());
}

auto GameEngine::world_freeze_refused_message() -> QString {
  return tr("The previous frame is still running; the match could not be "
            "changed. Please try again.");
}

GameEngine::WorldFreeze::WorldFreeze(GameEngine& engine)
    : m_engine(engine) {
  const auto result = m_engine.m_lifecycle.barrier().try_freeze(
      k_render_frame_wait_budget, k_render_frame_wait_poll);
  if (result != App::Core::FrameBarrier::FreezeResult::Acquired) {
    qWarning() << "GameEngine: the"
               << (result == App::Core::FrameBarrier::FreezeResult::RenderBusy
                       ? "render"
                       : "simulation")
               << "thread did not acknowledge the world freeze within"
               << static_cast<int>(k_render_frame_wait_budget.count())
               << "ms; refusing the world transition and keeping the current world";
    return;
  }
  m_acquired = true;
}

GameEngine::WorldFreeze::~WorldFreeze() {
  if (!m_acquired) {
    return;
  }
  m_engine.m_lifecycle.barrier().release_freeze();
}

auto GameEngine::try_begin_render_frame() -> bool {
  return m_lifecycle.barrier().try_begin_render();
}

void GameEngine::end_render_frame() {
  m_lifecycle.barrier().end_render();
}

auto GameEngine::try_begin_simulation_tick() -> bool {
  return m_lifecycle.barrier().try_begin_simulation();
}

void GameEngine::end_simulation_tick() {
  m_lifecycle.barrier().end_simulation();
}

void GameEngine::start_simulation_thread() {
  m_lifecycle.start([this](float dt) { run_simulation_tick(dt); });
}

void GameEngine::stop_simulation_thread() {
  m_lifecycle.stop();
}

void GameEngine::run_simulation_tick(float dt) {
  simulate(dt);
  update_presentation(dt);
  m_saves->drain_pending_capture();
}

void GameEngine::film_step(float dt) {
  const std::lock_guard<std::recursive_mutex> frame_lock(m_lifecycle.frame_mutex());
  if (!try_begin_simulation_tick()) {
    return;
  }
  simulate(dt);

  if (m_world != nullptr && !m_runtime.loading) {
    Engine::Core::publish_creature_presentations(*m_world);
  }
  update_presentation(dt);
  m_saves->drain_pending_capture();
  end_simulation_tick();
}

void GameEngine::simulate(float dt) {
  if (m_runtime.loading) {
    return;
  }

  const bool overlay_up = m_loading_overlay.waiting_for_first_frame();

  float simulation_time_scale = 0.0F;
  if (!m_runtime.paused && !overlay_up) {
    simulation_time_scale =
        m_runtime.time_scale * m_commander_view_model->time_effect_scale(
                                   dt * m_runtime.time_scale, m_runtime.paused);
  }
  m_simulation_time_scale.store(simulation_time_scale, std::memory_order_release);

  m_mission->publish_deadline(mission_binding());

  update_commander_messages(
      m_runtime.victory_state.isEmpty() ? dt * simulation_time_scale : dt);

  RuntimeFrameState frame_state{.simulation_time_scale = simulation_time_scale};
  m_frame_orchestrator.advance_simulation(
      scene_context(), frame_state, dt, [this](float step_dt) {
        update_mission_waves(step_dt);
        update_mission_stages(step_dt);
        update_active_runtime_simulation(step_dt);
      });
  note_dropped_simulation_ticks(frame_state.dropped_simulation_ticks, dt);
}

void GameEngine::update_presentation(float dt) {
  if (m_runtime.loading) {
    return;
  }

  m_order_feedback->update_markers(dt);
  announce_player_defeats(dt);
  m_ally_announcements->announce_all(m_session.get(), m_runtime.local_owner_id);
  m_activity_view_model->advance_feedback(dt);

  advance_frame_orchestrator(dt);
  update_control_presentation(dt);
  {
    Render::Profiling::AccumulatorScope const sync_scope(
        &Render::Profiling::global_profile().view_model_sync_us);
    publish_frame_snapshots();
    m_minimap_events->publish_overlays(dt);
    m_mission->flush_announcements(dt);
    sync_render_camera();
    capture_render_selection();
    sync_scatter_world_props();
    sync_selected_player_state();
    sync_economy_state();
    sync_target_presentation(dt);
    update_tutorial(dt);
  }

  publish_presentation_frame();
}

void GameEngine::advance_frame_orchestrator(float dt) {
  const float simulation_time_scale =
      m_simulation_time_scale.load(std::memory_order_acquire);

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
      (!m_runtime.paused && !m_runtime.loading) ? m_environment->ambient() : nullptr,
      m_runtime.victory_state,
      dt,
      callbacks,
      {});
  m_runtime.selection_refresh_counter = frame_state.selection_refresh_counter;
  m_runtime.minimap_unit_update_accumulator =
      frame_state.minimap_unit_update_accumulator;
}

void GameEngine::update_control_presentation(float dt) {
  if (m_commander_view_model->active()) {
    m_commander_view_model->sample_frame_intent();
    m_commander_view_model->update_camera_presentation(dt);
  } else {
    m_camera_view_model->update_follow();
  }
}

void GameEngine::sync_target_presentation(float dt) {
  const auto targeting = targeting_inputs();
  m_targeting->sync_attack_targeting(targeting);
  m_targeting->sync_interaction_targeting(dt, targeting);
  m_targeting->sync_attack_range_rings(targeting);
  const auto focus = focus_inputs();
  m_focus->sync_focus_targets(focus);
  m_focus->sync_target_focus_markers(focus);
}

auto GameEngine::targeting_inputs() -> App::World::TargetingInputs {
  return {.attack = {.world = m_world,
                     .hover = m_hover_tracker.get(),
                     .cursor = m_cursor_manager.get(),
                     .camera = m_camera,
                     .local_owner_id = m_runtime.local_owner_id,
                     .spectator_mode = m_level.is_spectator_mode},
          .session = m_session.get(),
          .production = m_production_manager.get(),
          .activity = m_activity_view_model.get(),
          .commander_active = m_commander_view_model->active(),
          .cursor_screen = QPointF(m_runtime.last_cursor_x, m_runtime.last_cursor_y),
          .screen_to_ground = [this](const QPointF& screen, QVector3D& ground) {
            return screen_to_ground(screen, ground);
          }};
}

auto GameEngine::focus_inputs() -> App::World::FocusInputs {
  return {.world = m_world,
          .session = m_session.get(),
          .visibility = m_visibility_coordinator.get(),
          .activity = m_activity_view_model.get(),
          .local_owner_id = m_runtime.local_owner_id,
          .spectator_mode = m_level.is_spectator_mode};
}

void GameEngine::publish_presentation_frame() {
  auto frame = std::make_shared<App::Core::PresentationFrame>();
  frame->has_camera = m_camera != nullptr;
  if (frame->has_camera) {
    frame->camera = m_render_camera;
  }
  frame->selected_ids = m_selected_render_ids;
  frame->attack_targeting = m_targeting->attack_targeting();
  frame->interaction_targeting = m_targeting->interaction_targeting();
  frame->attack_range_rings = m_targeting->attack_range_rings();
  frame->order_markers = m_order_feedback->markers();
  frame->target_focus = m_focus->markers();
  frame->objective_marker = m_mission->stages().active_target();
  frame->commander_rally_preview_pos = m_commander_view_model->rally_preview_position();
  frame->local_owner_id = m_runtime.local_owner_id;
  frame->spectator_mode = m_level.is_spectator_mode;

  std::atomic_store_explicit(
      &m_presentation_frame,
      std::shared_ptr<const App::Core::PresentationFrame>(std::move(frame)),
      std::memory_order_release);
}

void GameEngine::announce_player_defeats(float dt) {
  if (m_world == nullptr || m_level.is_spectator_mode) {
    return;
  }

  m_battle_stats->announce_defeats(
      *m_world,
      m_runtime.local_owner_id,
      dt,
      [this](const auto& defeat) {
        emit player_defeated(App::World::format_defeat_announcement(defeat),
                             defeat.ally,
                             defeat.owner_id);
      },
      [this](int owner_id) {
        return m_mission->waves().owner_has_unspawned_waves(owner_id);
      });
}

void GameEngine::publish_frame_snapshots() {
  m_camera_view_model->publish_frame();
  m_commander_view_model->publish_frame();
  m_production_view_model->publish_frame();
  m_orders_view_model->publish_frame();
  m_placement_view_model->publish_frame();
}

void GameEngine::update(float dt) {
  const std::lock_guard<std::recursive_mutex> frame_lock(m_lifecycle.frame_mutex());
  simulate(dt);
  update_presentation(dt);
}

void GameEngine::render(int pixel_width, int pixel_height) {
  if (!m_renderer || !m_world || !m_runtime.initialized || m_runtime.loading) {
    return;
  }

  const auto frame_started = std::chrono::steady_clock::now();

  if (pixel_width > 0 && pixel_height > 0) {
    m_viewport.width = pixel_width;
    m_viewport.height = pixel_height;
  }

  const auto presentation =
      std::atomic_load_explicit(&m_presentation_frame, std::memory_order_acquire);
  if (presentation == nullptr) {
    return;
  }

  apply_presentation_camera(*presentation);

  m_renderer->set_world_view(Render::WorldView::of(*m_session));
  m_renderer->set_loading_overlay_active(m_loading_overlay.active());

  if (m_loading_overlay.active()) {
    prewarm_overlay_gpu_resources();
  }
  m_renderer->begin_frame();

  if (m_terrain_scene) {
    m_terrain_scene->submit(*m_renderer, m_renderer->resources());
    if (m_loading_overlay.active() && m_scatter != nullptr) {

      (void)m_scatter->prewarm_gpu_resources();
    }
  }

  if (m_hover_tracker) {
    m_renderer->set_hovered_entity_id(m_hover_tracker->get_last_hovered_entity());
  }
  m_renderer->set_local_owner_id(presentation->local_owner_id);
  m_renderer->set_order_marker_spectator_mode(presentation->spectator_mode);

  m_renderer->render_world(m_world);
  render_effects_pass(*presentation);
  m_renderer->end_frame();

  if (auto& profiler = Engine::Core::StartupProfiler::instance(); profiler.active()) {
    profiler.record_playable_frame(std::chrono::duration<double, std::milli>(
                                       std::chrono::steady_clock::now() - frame_started)
                                       .count());
  }
  update_loading_overlay();
  update_cursor_position();
}

void GameEngine::apply_presentation_camera(
    const App::Core::PresentationFrame& presentation) {
  if (presentation.has_camera) {
    m_render_camera = presentation.camera;
  }
  if (m_viewport.width > 0 && m_viewport.height > 0) {
    const float aspect =
        static_cast<float>(m_viewport.width) / static_cast<float>(m_viewport.height);
    m_render_camera.set_perspective(m_render_camera.get_fov(),
                                    aspect,
                                    m_render_camera.get_near(),
                                    m_render_camera.get_far());
  }
  if (m_drawn_selected_ids != presentation.selected_ids) {
    m_drawn_selected_ids = presentation.selected_ids;
    m_renderer->set_selected_entities(m_drawn_selected_ids);
  }

  m_renderer->set_camera(&m_render_camera);
  Render::GL::CameraVisibility::instance().set_camera(&m_render_camera);
  if (m_viewport.width > 0 && m_viewport.height > 0) {
    m_renderer->set_viewport(m_viewport.width, m_viewport.height);
  }
}

void GameEngine::prewarm_overlay_gpu_resources() {
  (void)m_renderer->rigged_mesh_cache().prewarm_gpu_resources();
  if (auto* backend = m_renderer->backend(); backend != nullptr) {
    (void)backend->prewarm_static_meshes(Render::GL::requested_building_meshes());
  }
  (void)Render::GL::prewarm_projectile_geometry();
  (void)Render::GL::SharedGeometryCache::instance().prewarm_gpu_resources();
  if (m_fog != nullptr) {
    (void)m_fog->prewarm_gpu_resources();
  }
  if (m_features != nullptr) {
    (void)m_features->prewarm_gpu_resources();
  }
  if (m_surface != nullptr && m_surface->terrain() != nullptr) {
    (void)m_surface->terrain()->prewarm_gpu_resources();
  }
}

void GameEngine::render_effects_pass(const App::Core::PresentationFrame& presentation) {
  const std::shared_ptr<Engine::Core::World> effects_snapshot =
      m_world->acquire_render_snapshot();
  if (effects_snapshot == nullptr) {
    return;
  }
  App::Core::FrameUiCoordinator::render_effects(
      {.renderer = m_renderer.get(),
       .command_controller = m_command_controller.get(),
       .local_owner_id = presentation.local_owner_id,
       .commander_rally_preview_pos = presentation.commander_rally_preview_pos,
       .attack_targeting = &presentation.attack_targeting,
       .attack_range_rings = &presentation.attack_range_rings,
       .order_markers = &presentation.order_markers,
       .target_focus = &presentation.target_focus,
       .interaction_targeting = &presentation.interaction_targeting,
       .objective_marker = presentation.objective_marker,
       .effects = &effects_snapshot->render_effects_frame(),
       .snapshot = effects_snapshot.get(),
       .session = m_session.get()},
      [this]() { m_commander_view_model->render_effects(); });
}

void GameEngine::set_input_viewport_size(qreal width, qreal height) {
  if (width > 0.0 && height > 0.0) {
    m_viewport.input_width = width;
    m_viewport.input_height = height;
  }
}

void GameEngine::update_loading_overlay() {
  if (!m_loading_overlay.waiting_for_first_frame()) {
    return;
  }

  if (QThread::currentThread() != thread()) {
    QMetaObject::invokeMethod(
        this, [this]() { update_loading_overlay(); }, Qt::QueuedConnection);
    return;
  }

  const bool renderer_ready = m_renderer && (m_renderer->resources() != nullptr);
  const auto release = m_loading_overlay.poll(
      renderer_ready, [this]() { return mission_startup_pending_components(); });
  if (!release.released) {
    return;
  }

  Engine::Core::StartupProfiler::instance().mark_overlay_released();
  if (Engine::Core::StartupProfiler::reporting_enabled()) {
    constexpr int k_startup_report_delay_ms = 5200;
    QTimer::singleShot(k_startup_report_delay_ms, this, []() {
      Engine::Core::StartupProfiler::instance().log_report();
    });
  }
  if (release.finalize_progress && m_loading_progress_tracker) {
    m_loading_progress_tracker->set_stage(
        LoadingProgressTracker::LoadingStage::COMPLETED);
  }
  emit is_loading_changed();

  if (release.show_objectives) {
    m_match_setup_view_model->notify_current_mission_changed();
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

void GameEngine::sync_selection_flags() {
  if (!m_world) {
    return;
  }
  auto* selection_system = &Game::Session::session_for(*m_world).selection();
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

void GameEngine::report_late_command_rejection(const Game::Command::Command& command,
                                               Game::Command::Rejection reason) {
  const auto outcome =
      App::Core::OrderFeedbackPresenter::late_rejection(command, reason);
  if (!outcome.has_value()) {
    return;
  }
  QMetaObject::invokeMethod(
      this,
      [this, outcome = *outcome]() { handle_order_feedback(outcome); },
      Qt::QueuedConnection);
}

void GameEngine::handle_order_feedback(const App::Core::OrderOutcome& outcome) {
  const auto announcement = m_order_feedback->present(outcome);
  if (!announcement.has_value()) {
    return;
  }
  emit order_feedback(announcement->kind,
                      announcement->accepted,
                      announcement->message,
                      announcement->failure);
}

auto GameEngine::selected_units_model() -> QAbstractItemModel* {
  return m_selected_units_model;
}

auto GameEngine::audio_system() -> QObject* {
  return m_audio->proxy();
}

void GameEngine::set_audio_frontend_context(const QString& context) {
  m_audio->set_frontend_context(context);
}

void GameEngine::set_paused(bool paused) {
  if (m_runtime.paused == paused) {
    return;
  }
  m_runtime.paused = paused;
  m_tutorial->notes().speed_changed = true;
}

void GameEngine::set_game_speed(float speed) {
  const float sanitized = App::Core::GameSpeed::sanitize(speed);
  if (qFuzzyCompare(m_runtime.time_scale, sanitized)) {
    return;
  }
  m_runtime.time_scale = sanitized;
  m_tutorial->notes().speed_changed = true;
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
  m_replay->set_record_path(path);
}

void GameEngine::set_replay_verify_exit(bool enabled) {
  m_replay->set_verify_exit(enabled);
}

auto GameEngine::replay_playing() const -> bool {
  return m_session != nullptr && m_session->replay_player() != nullptr;
}

auto GameEngine::start_replay(const QString& path) -> bool {
  auto* setup = m_match_setup_view_model.get();
  const auto result = m_replay->begin_playback(
      path,
      {.campaign_mission =
           [setup](const QString& reference, const QString& difficulty) {
             setup->start_campaign_mission(reference, difficulty, false);
           },
       .mission_file =
           [setup](const QString& reference, const QString& difficulty) {
             setup->start_mission_file(reference, difficulty, false);
           },
       .skirmish =
           [setup](const QString& reference, const QVariantList& player_configs) {
             setup->start_skirmish(reference, player_configs);
           }});
  switch (result.failure) {
  case App::Session::ReplayPlaybackResult::Failure::None:
    return true;
  case App::Session::ReplayPlaybackResult::Failure::LoadFailed:
    set_error(tr("Cannot play replay: %1").arg(result.detail));
    return false;
  case App::Session::ReplayPlaybackResult::Failure::UnknownKind:
    set_error(tr("Cannot play replay: unknown launch kind '%1'").arg(result.detail));
    return false;
  }
  return false;
}

void GameEngine::launch_match(const App::Core::MatchLaunch& launch) {
  clear_error();
  set_game_speed(App::Core::GameSpeed::k_default);
  m_replay->note_launch(
      {launch.kind, launch.reference, launch.player_configs, launch.difficulty});
  start_skirmish_internal(
      launch.map_path, launch.player_configs, launch.set_skirmish_context);
}

void GameEngine::start_skirmish_internal(const QString& map_path,
                                         const QVariantList& player_configs,
                                         bool set_skirmish_context) {

  auto world_freeze = std::make_shared<WorldFreeze>(*this);
  if (!world_freeze->acquired()) {
    set_error(world_freeze_refused_message());
    return;
  }

  clear_error();
  reset_preload_interaction_state();
  reset_mission_runtime_state();

  m_level.map_path = map_path;
  m_level.map_name = map_path;

  if (m_campaign_manager && set_skirmish_context) {
    m_campaign_manager->set_skirmish_context(map_path);
  }

  reset_match_outcome();

  if (!m_runtime.initialized) {
    ensure_initialized();
  }

  if (!m_world || !m_renderer || (m_camera == nullptr) || !m_skirmish_runtime) {
    set_error(tr("Cannot start skirmish: renderer not initialized"));
    return;
  }

  begin_match_loading(map_path);
  QTimer::singleShot(50, this, [this, map_path, player_configs, world_freeze]() {
    complete_match_load(map_path, player_configs);
  });
}

void GameEngine::reset_match_outcome() {
  if (!m_runtime.victory_state.isEmpty()) {
    m_runtime.victory_state = "";
    m_runtime.defeat_reason.clear();
    emit victory_state_changed();
  }
  if (m_victory_service) {
    m_victory_service->reset();
  }
  if (m_battle_stats->reset()) {
    emit enemy_troops_defeated_changed();
  }
}

void GameEngine::begin_match_loading(const QString& map_path) {
  m_loading_overlay.begin();
  m_runtime.loading = true;
  const auto hints = App::Core::SkirmishRuntimeCoordinator::loading_tip_hints(
      m_campaign_manager.get());
  LoadingTips::instance()->prefer_for_load(
      map_path, hints.mission_id, hints.mission_has_undead);
  emit is_loading_changed();

  if (m_loading_progress_tracker) {
    m_loading_progress_tracker->start_loading();
  }

  Engine::Core::StartupProfiler::instance().begin_run(map_path.toStdString());
  Game::Map::MapContextStore::reset_statistics();

  QCoreApplication::processEvents(QEventLoop::AllEvents);
  if (m_release_self_test_mode) {

    qInfo() << "SOI_AUDIO_SELF_TEST: mission preload skipped after manifest "
               "validation";
  } else {
    const Engine::Core::ScopedStartupPhase phase("audio.mission_preload");
    AudioResourceLoader::load_audio_resources(AudioLoadPolicy::Mission);
  }
}

void GameEngine::fail_loading(const QString& error) {
  set_error(error);
  m_runtime.loading = false;
  m_loading_overlay.abort();
  emit is_loading_changed();
}

void GameEngine::complete_match_load(const QString& map_path,
                                     const QVariantList& player_configs) {
  if (!m_world || !m_renderer || (m_camera == nullptr) || !m_skirmish_runtime) {
    set_error(tr("Cannot start skirmish: renderer not initialized"));
    m_runtime.loading = false;
    emit is_loading_changed();
    return;
  }

  if (m_hover_tracker) {
    m_hover_tracker->update_hover(-1, -1, *m_world, *m_camera, 0, 0);
  }

  const auto load_effects = load_match_world(map_path, player_configs);

  if (load_effects.selected_player_changed) {
    m_selected_player_id = load_effects.updated_player_id;
    emit selected_player_id_changed();
  }

  if (!load_effects.success) {
    fail_loading(load_effects.error);
    return;
  }

  m_runtime.local_owner_id = load_effects.updated_player_id;
  publish_client_context();
  m_audio->coordinator().configure_audio_manifest_mappings(m_runtime.local_owner_id);

  configure_loaded_match(
      map_path, player_configs, load_effects.resolved_player_configs);
  finalize_match_load();
}

auto GameEngine::load_match_world(const QString& map_path,
                                  const QVariantList& player_configs)
    -> App::Core::PerformSkirmishLoadEffects {
  const bool is_campaign_mission =
      m_campaign_manager && m_campaign_manager->current_mission_context().has_mission();
  const bool allow_default_player_barracks = !is_campaign_mission;
  const Engine::Core::ScopedStartupPhase world_phase("world.load");
  return m_skirmish_runtime->perform_load({*m_world,
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
                                           is_campaign_mission,
                                           m_loading_progress_tracker.get(),
                                           [this]() {
                                             emit owner_info_changed();
                                           }});
}

void GameEngine::configure_loaded_match(const QString& map_path,
                                        const QVariantList& player_configs,
                                        const QVariantList& resolved_player_configs) {
  m_mission->set_difficulty(App::Mission::MissionRuntime::resolve_difficulty(
      m_campaign_manager.get(),
      resolved_player_configs.isEmpty() ? player_configs : resolved_player_configs));
  const auto& difficulty = m_mission->difficulty();
  App::Core::SkirmishRuntimeCoordinator::apply_difficulty_forces(
      *m_world, difficulty, m_runtime.local_owner_id);
  {
    const Engine::Core::ScopedStartupPhase phase("audio.mission_ambience");
    m_audio->coordinator().apply_mission_ambience(
        current_mission_definition(), map_path, m_runtime.local_owner_id);
  }

  {
    const Engine::Core::ScopedStartupPhase phase("mission.commander_setup");
    apply_skirmish_commander_setup(player_configs);
  }
  {
    const Engine::Core::ScopedStartupPhase phase("mission.setup");
    apply_mission_setup();
  }
  m_skirmish_runtime->initialize_player_resources({*m_session,
                                                   m_level,
                                                   m_runtime.local_owner_id,
                                                   authored_mission_definition(),
                                                   &difficulty});
  configure_mission_victory_conditions();

  m_mission->publish_stages(mission_binding());
  publish_wave_status();
  m_environment->configure_rain(m_level, m_rain.get());
  m_environment->reset_clock(m_level);

  App::Core::SkirmishRuntimeCoordinator::prepare_ai_state(m_world, m_session.get());
  App::Core::SkirmishRuntimeCoordinator::record_startup_counters(*m_world);
}

void GameEngine::finalize_match_load() {
  const auto finalize_effects =
      m_skirmish_runtime->finalize_load({m_runtime.loading,
                                         m_loading_overlay,
                                         m_match_setup_view_model->is_mission_match()});

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
    m_environment->reset_ambient();
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
  m_replay->arm_for_started_match(m_session.get());
  m_tutorial->activate_if_configured(m_campaign_manager.get());
}

auto GameEngine::current_mission_definition() const
    -> const Game::Mission::MissionDefinition* {
  if (m_campaign_manager &&
      m_campaign_manager->current_mission_definition().has_value()) {
    return &*m_campaign_manager->current_mission_definition();
  }
  return nullptr;
}

auto GameEngine::authored_mission_definition() const
    -> const Game::Mission::MissionDefinition* {
  if (m_campaign_manager &&
      m_campaign_manager->current_mission_context().has_mission()) {
    return current_mission_definition();
  }
  return nullptr;
}

auto GameEngine::mission_binding() -> App::Mission::MissionBinding {
  return {.world = m_world,
          .session = m_session.get(),
          .campaign = m_campaign_manager.get(),
          .level = &m_level,
          .victory_service = m_victory_service.get(),
          .minimap = m_minimap_manager.get(),
          .local_owner_id = m_runtime.local_owner_id};
}

auto GameEngine::commander_binding() -> App::Mission::CommanderMessageBinding {
  return {.world = m_world,
          .session = m_session.get(),
          .campaign = m_campaign_manager.get(),
          .level = &m_level,
          .local_owner_id = m_runtime.local_owner_id};
}

void GameEngine::apply_mission_setup() {
  if (!m_world || !m_campaign_manager || !m_skirmish_runtime) {
    return;
  }

  const auto effects = m_mission->bind_setup(mission_binding(), m_selected_player_id);
  if (!effects.has_value()) {
    return;
  }
  configure_mission_stages();

  publish_wave_status();
  m_commander_messages->arm_start_cue();
  if (effects->rebuild_entity_cache) {
    GameStateRestorer::rebuild_entity_cache(
        m_world, m_entity_cache, m_runtime.local_owner_id);
  }
  if (effects->selected_player_changed) {
    emit selected_player_id_changed();
  }
  if (effects->center_camera_on_local_forces) {
    m_skirmish_runtime->center_camera_on_local_forces(
        {m_world, m_camera, m_runtime.local_owner_id});
  }
  if (effects->troop_count_changed) {
    emit troop_count_changed();
  }
  if (effects->owner_info_changed) {
    emit owner_info_changed();
  }
}

void GameEngine::apply_skirmish_commander_setup(const QVariantList& player_configs) {
  m_mission->apply_skirmish_commander_setup(mission_binding(), player_configs);
}

auto GameEngine::mission_startup_pending_components() const -> QStringList {
  QStringList pending;
  if (m_scatter != nullptr && !m_scatter->is_gpu_ready()) {
    pending << QStringLiteral("terrain scatter");
  }
  if (m_renderer && m_renderer->has_pending_template_prewarm()) {
    pending << QStringLiteral("unit templates");
  }
  if (AudioSystem::get_instance().has_pending_mission_decodes()) {
    pending << QStringLiteral("mission audio");
  }
  if (m_world != nullptr) {
    if (const auto* ai_system = m_world->get_system<Game::Systems::AISystem>();
        ai_system != nullptr && !ai_system->initial_decisions_ready()) {
      pending << QStringLiteral("AI initial decisions");
    }
  }
  return pending;
}

void GameEngine::configure_mission_victory_conditions() {
  if (!m_victory_service) {
    return;
  }

  const bool has_mission_rules = authored_mission_definition() != nullptr;
  if (has_mission_rules) {
    m_campaign_manager->configure_mission_victory_conditions(m_victory_service.get(),
                                                             m_runtime.local_owner_id);
  } else {
    const Game::Map::MapContext map_context =
        Game::Map::MapContextStore::acquire(m_level.map_path);
    m_victory_service->configure(map_context.valid() ? map_context.definition()->victory
                                                     : Game::Map::VictoryConfig(),
                                 m_runtime.local_owner_id);
  }

  m_victory_service->set_spectator_mode(m_level.is_spectator_mode);
}

void GameEngine::reset_preload_interaction_state() {
  m_commander_view_model->reset_for_new_match();
  m_activity_view_model->clear_feedback();
  if (m_command_controller) {
    m_command_controller->reset_transient_state();
  }

  if (m_production_manager) {
    m_production_manager->reset_transient_state();
  }

  if (m_world) {
    if (auto* selection_system = &Game::Session::session_for(*m_world).selection()) {
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
  m_runtime.selection_refresh_counter = 0;
  m_runtime.minimap_unit_update_accumulator = 0.0F;

  emit selected_units_changed();
}

void GameEngine::reset_mission_runtime_state() {
  m_tutorial->end_match();
  m_runtime.minimap_unit_update_accumulator = 0.0F;
  m_minimap_events->reset();
  m_mission->reset();
  m_commander_messages->clear();
  m_targeting->reset_interaction();
  m_session->economy().clear();
  sync_selected_player_state();
  m_economy->reset();
  m_audio->coordinator().stop_mission_ambience();
  AudioSystem::get_instance().stop_music();
  AudioResourceLoader::unload_audio_resources(AudioLoadPolicy::Mission);
  AudioResourceLoader::unload_audio_resources(AudioLoadPolicy::Lazy);

  Game::Audio::CueTrace::instance().write_requested_summary(
      m_level.map_path.toStdString());
  Game::Audio::CueTrace::instance().reset();
}

void GameEngine::update_mission_waves(float dt) {
  if (!m_world || !m_mission || !m_runtime.victory_state.isEmpty()) {
    return;
  }
  const auto result = m_mission->advance_waves(
      mission_binding(), dt, m_tutorial->holds_mission_clock(), [this]() {
        sync_selected_player_state();
      });
  if (result.owner_info_changed) {
    emit owner_info_changed();
  }
}

void GameEngine::update_mission_stages(float delta_time) {
  m_mission->advance_stages(mission_binding(), delta_time);
}

void GameEngine::configure_mission_stages() {
  if (m_mission->configure_stages(mission_binding())) {
    m_commander_messages->configure(commander_binding());
  }
}

void GameEngine::publish_wave_status() {
  m_mission->publish_wave_status(mission_binding());
}

void GameEngine::restore_mission_waves(const QJsonObject& wave_state) {
  m_mission->restore_waves(mission_binding(), wave_state);
  configure_mission_stages();
  publish_wave_status();
}

void GameEngine::restore_mission_stages(const QJsonObject& stage_state) {
  m_mission->restore_stages(mission_binding(), stage_state);
}

void GameEngine::restore_commander_message_state(const QJsonObject& state) {
  m_commander_messages->restore(commander_binding(), state);
}

void GameEngine::restore_tutorial_state(const QJsonObject& state) {
  m_tutorial->restore(m_campaign_manager.get(),
                      state,
                      m_mission->waves().director().cleared_wave_count());
}

void GameEngine::restore_battle_stats(const QJsonObject& state) {
  if (m_battle_stats->restore(state, m_session.get())) {
    emit enemy_troops_defeated_changed();
  }
}

void GameEngine::update_commander_messages(float delta_time) {
  m_commander_messages->update(
      commander_binding(),
      {.may_release_start_cue =
           !(is_loading() || m_runtime.paused || !m_runtime.initialized),
       .match_decided = !m_runtime.victory_state.isEmpty()},
      delta_time);
}

void GameEngine::update_tutorial(float real_dt) {
  m_tutorial->update(real_dt,
                     {.world = m_world,
                      .session = m_session.get(),
                      .minimap = m_minimap_manager.get(),
                      .placement = m_placement_view_model.get(),
                      .waves = &m_mission->waves(),
                      .victory_state = m_runtime.victory_state,
                      .local_owner_id = m_runtime.local_owner_id,
                      .enemy_units_defeated = m_battle_stats->enemy_units_defeated(),
                      .mission_running = m_runtime.initialized && !is_loading()});
}

void GameEngine::open_settings() {
  qInfo() << "Open settings requested";
}

auto GameEngine::last_save_capture_us() const -> std::uint64_t {
  return m_saves != nullptr ? m_saves->last_capture_us() : 0U;
}

auto GameEngine::capture_save_to_slot(const QString& slot_name,
                                      Game::Systems::Save::SlotKind kind,
                                      int autosave_retention)
    -> App::Core::SaveToSlotEffects {
  const Game::Systems::RuntimeSnapshot runtime_snapshot = to_runtime_snapshot();
  Game::Systems::LevelSnapshot level_snapshot = m_level;
  m_environment->capture_into(level_snapshot);
  std::optional<Game::Mission::MissionContext> mission_context;
  QString mission_title;
  if (m_campaign_manager) {
    mission_context = m_campaign_manager->current_mission_context();
    if (const auto* definition = current_mission_definition(); definition != nullptr) {
      mission_title = definition->title;
    }
  }

  return m_save_load_coordinator->begin_save_to_slot(
      {.world = *m_world,
       .save_load_service = *m_save_load_service,
       .camera = m_camera,
       .level = level_snapshot,
       .runtime_snapshot = runtime_snapshot,
       .slot = slot_name,
       .title = slot_name,
       .map_name = m_level.map_name,
       .mission_context = std::move(mission_context),
       .difficulty = &m_mission->difficulty(),
       .mission_title = mission_title,
       .kind = kind,
       .play_time_seconds = m_mission->waves().elapsed(),
       .autosave_retention = autosave_retention,
       .mission_wave_state = m_mission->waves().director().serialize(),
       .mission_stage_state = m_mission->serialize_stages(),
       .commander_message_state = m_commander_messages->serialize(),
       .tutorial_state = m_tutorial->serialize(),
       .battle_stats = m_battle_stats->serialize(m_session.get())});
}

auto GameEngine::consume_screenshot_request() -> bool {
  return m_saves->consume_screenshot_request();
}

void GameEngine::submit_frame_image(const QImage& image) {
  if (image.isNull()) {
    return;
  }

  QMetaObject::invokeMethod(
      this,
      [this, image]() { m_saves->attach_screenshot(image); },
      Qt::QueuedConnection);
}

void GameEngine::end_match_after_failed_load() {

  if (m_victory_service) {
    m_victory_service->reset();
  }
  if (m_world) {
    m_world->clear();
  }
  m_entity_cache.reset();
  m_level = Game::Systems::LevelSnapshot{};
  m_runtime.initialized = false;
  if (!m_runtime.victory_state.isEmpty()) {
    m_runtime.victory_state.clear();
    m_runtime.defeat_reason.clear();

    emit victory_state_changed();
  }
  reset_mission_runtime_state();
  if (m_campaign_manager) {
    m_campaign_manager->restore_mission_context(Game::Mission::MissionContext{});
  }
  m_saves->stop_autosave_timer();
  emit troop_count_changed();
  emit selected_units_changed();
  emit match_ended();
}

void GameEngine::load_game_from_slot(const QString& slot_name) {
  if ((m_save_load_service == nullptr) || !m_world) {
    set_error(tr("Load: not initialized"));
    return;
  }

  const WorldFreeze world_freeze(*this);
  if (!world_freeze.acquired()) {
    set_error(world_freeze_refused_message());
    return;
  }

  if (!m_runtime.initialized) {
    ensure_initialized();
  }
  if (!m_runtime.initialized) {
    set_error(tr("Load: not initialized"));
    return;
  }

  if (m_commander_view_model->active()) {
    m_commander_view_model->exit_mode();
  }

  reset_preload_interaction_state();
  begin_load_transition();

  Game::Systems::RuntimeSnapshot runtime_snapshot = to_runtime_snapshot();
  const App::Core::LoadFromSlotEffects effects =
      m_save_load_coordinator->load_from_slot(
          load_request(slot_name, runtime_snapshot));
  if (!effects.success) {
    fail_loading(effects.error);
    if (effects.world_discarded) {

      end_match_after_failed_load();
    }
    return;
  }
  finish_successful_load(slot_name, effects);
}

void GameEngine::begin_load_transition() {
  reset_mission_runtime_state();
  if (m_battle_stats->reset()) {
    emit enemy_troops_defeated_changed();
  }

  m_loading_overlay.begin();
  m_runtime.loading = true;
  LoadingTips::instance()->set_preferred_tags({});
  emit is_loading_changed();
}

void GameEngine::finish_successful_load(const QString& slot_name,
                                        const App::Core::LoadFromSlotEffects& effects) {
  m_mission->set_difficulty(effects.match_difficulty);
  if (m_world != nullptr) {
    (void)Game::Mission::apply_undead_wave_difficulty(*m_world,
                                                      m_mission->difficulty());
  }
  if (!effects.warning.isEmpty()) {

    emit m_save_slots_view_model->save_completed(slot_name, false, effects.warning);
  }
  restore_environment_after_load();
  emit victory_state_changed();
  release_loading_overlay_after_load();

  m_minimap_view_model->notify_image_changed();

  if (effects.emit_selected_units_changed) {
    emit selected_units_changed();
  }
  if (effects.emit_owner_info_changed) {
    emit owner_info_changed();
  }
}

auto GameEngine::load_request(const QString& slot_name,
                              Game::Systems::RuntimeSnapshot& runtime_snapshot)
    -> App::Core::LoadFromSlotContext {
  return {
      .world = *m_world,
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
      .audio_coordinator = &m_audio->coordinator(),
      .victory_service = m_victory_service.get(),
      .configure_victory = [this]() { configure_mission_victory_conditions(); },
      .emit_troop_count_changed = [this]() { emit troop_count_changed(); },
      .restore_mission_waves =
          [this](const QJsonObject& wave_state) { restore_mission_waves(wave_state); },
      .restore_mission_stages =
          [this](const QJsonObject& stage_state) {
            restore_mission_stages(stage_state);
          },
      .restore_commander_messages =
          [this](const QJsonObject& message_state) {
            restore_commander_message_state(message_state);
          },
      .restore_tutorial =
          [this](const QJsonObject& tutorial_state) {
            restore_tutorial_state(tutorial_state);
          },
      .restore_battle_stats =
          [this](const QJsonObject& stats) {
            restore_battle_stats(stats);
          }};
}

void GameEngine::restore_environment_after_load() {
  m_environment->restore_after_load(m_level, m_renderer.get(), m_rain.get());

  sync_scatter_world_props();

  if (m_camera_controller) {
    m_camera_controller->sync_map_bounds();
  }

  m_audio->coordinator().apply_mission_ambience(
      current_mission_definition(), m_level.map_path, m_runtime.local_owner_id);
}

void GameEngine::release_loading_overlay_after_load() {
  m_runtime.loading = false;
  m_loading_overlay.arm_after_load();
  emit is_loading_changed();
  qInfo() << "Game load complete, victory/defeat checks re-enabled";
  Game::Audio::play_cue(Game::Audio::Cue::k_state_load_complete);
}

auto GameEngine::to_runtime_snapshot() const -> Game::Systems::RuntimeSnapshot {
  return m_save_load_coordinator->to_runtime_snapshot(
      {.session = *m_session,
       .paused = m_runtime.paused,
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
      {.session = *m_session,
       .paused = m_runtime.paused,
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

void GameEngine::sync_selected_player_state() {
  int const owner_id =
      m_selected_player_id > 0 ? m_selected_player_id : m_runtime.local_owner_id;
  if (!m_economy->sync_selected_player_state(
          *m_session, owner_id, m_level.max_troops_per_player)) {
    return;
  }
  emit selected_player_state_changed();
  emit owner_info_changed();
}

void GameEngine::sync_economy_state() {
  m_economy->sync({.world = m_world,
                   .session = m_session.get(),
                   .campaign = m_campaign_manager.get(),
                   .selected_player_id = m_selected_player_id,
                   .local_owner_id = m_runtime.local_owner_id,
                   .manpower_cap = m_level.max_troops_per_player,
                   .spectator_mode = m_level.is_spectator_mode,
                   .mission_match = m_match_setup_view_model->is_mission_match(),
                   .loading = m_runtime.loading});
}

void GameEngine::sync_scatter_world_props() {
  m_environment->sync_scatter_world_props(*m_session, m_scatter.get());
}

void GameEngine::exit_game() {
  qInfo() << "Exit game requested";
  QCoreApplication::quit();
}

auto GameEngine::get_owner_info() const -> QVariantList {
  const std::lock_guard<std::recursive_mutex> frame_lock(m_lifecycle.frame_mutex());
  return App::Core::EconomyReadModel::build_owner_info(
      *m_session, m_runtime.local_owner_id, m_level.max_troops_per_player);
}

auto GameEngine::local_player_nation() const -> QString {
  const std::lock_guard<std::recursive_mutex> frame_lock(m_lifecycle.frame_mutex());

  const auto* nation =
      m_session->nations().get_nation_for_player(m_runtime.local_owner_id);
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
  m_entity_cache.apply_spawn(event, m_runtime.local_owner_id, m_session->owners());

  if (m_entity_cache.player_troop_count != m_runtime.last_troop_count) {
    m_runtime.last_troop_count = m_entity_cache.player_troop_count;
    emit troop_count_changed();
  }
  if (event.owner_id == m_runtime.local_owner_id) {
    const auto troop_type = Game::Units::spawn_typeToTroopType(event.spawn_type);
    if (troop_type.has_value() && Game::Units::is_commander_troop(*troop_type)) {
      m_commander_view_model->notify_availability_changed();
    }
  }
}

void GameEngine::on_unit_died(const Engine::Core::UnitDiedEvent& event) {
  m_entity_cache.apply_death(event, m_runtime.local_owner_id, m_session->owners());

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

auto GameEngine::commander_message_speakers() const -> const QStringList& {
  return m_commander_messages->speaker_ids();
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

auto GameEngine::commander_message_view_model() const -> QObject* {
  return m_commander_message_view_model.get();
}

auto GameEngine::mission_view_model() const -> QObject* {
  return m_mission_view_model.get();
}

auto GameEngine::tutorial_view_model() const -> QObject* {
  return m_tutorial->director();
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
  m_client.feedback = &m_player_feedback;

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
