#include "arena_viewport.h"

#include <QDebug>
#include <QImage>
#include <QOpenGLContext>
#include <QOpenGLDebugLogger>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QPainter>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <vector>

#include "app/commander/commander_control_controller.h"
#include "app/session/renderer_bootstrap.h"
#include "app/session/world_bootstrap.h"
#include "arena_scenario.h"
#include "arena_viewport_internal.h"
#include "game/accessibility/motion_settings.h"
#include "game/command/command_queue.h"
#include "game/core/world.h"
#include "game/map/map_transformer.h"
#include "game/render_bridge/camera_service.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system.h"
#include "game/systems/arrow_system.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/target_focus.h"
#include "game/units/factory.h"
#include "game/units/troop_type.h"
#include "game/units/unit.h"
#include "render/camera_visibility.h"
#include "render/entity/combat_dust_renderer.h"
#include "render/entity/commander_aura_renderer.h"
#include "render/entity/healer_aura_renderer.h"
#include "render/entity/healing_beam_renderer.h"
#include "render/entity/healing_waves_renderer.h"
#include "render/entity/production_completion_renderer.h"
#include "render/geom/arrow.h"
#include "render/geom/projectile_renderer.h"
#include "render/geom/range_rings.h"
#include "render/geom/target_focus_rings.h"
#include "render/humanoid/runtime/runtime_stats.h"
#include "render/profiling/frame_continuity_analyzer.h"
#include "render/profiling/frame_profile.h"
#include "render/terrain_scene_proxy.h"
#include "scene/camera.h"
#include "terrain_alignment.h"
#include "ui/icon_art.h"

using namespace arena_viewport_internal;

namespace {

constexpr int k_interactive_frame_interval_ms = 8;

} // namespace

namespace arena_viewport_internal {

void sync_camera_map_bounds(Render::GL::Camera* camera,
                            const Game::Map::VisibilityService& visibility) {
  if (camera == nullptr) {
    return;
  }
  if (!visibility.is_initialized()) {
    camera->clear_map_bounds();
    return;
  }
  camera->set_map_bounds({.tile_size = visibility.get_tile_size(),
                          .width = visibility.get_width(),
                          .height = visibility.get_height()});
}

} // namespace arena_viewport_internal

ArenaViewport::ArenaViewport(Game::Session::SessionContext& session, QWidget* parent)
    : QOpenGLWidget(parent)
    , m_session(session)
    , m_spawn_nation_id(Game::Systems::NationID::RomanRepublic)
    , m_spawn_unit_type(Game::Units::TroopType::Swordsman) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setMinimumSize(800, 600);

  auto rendering = RendererBootstrap::initialize_rendering();
  m_world = std::make_unique<Engine::Core::World>();
  m_feedback.set_world(m_world.get());
  m_renderer = std::move(rendering.renderer);
  m_camera = std::move(rendering.camera);
  m_terrain_scene = std::move(rendering.terrain_scene);
  m_surface = std::move(rendering.surface);
  m_features = std::move(rendering.features);
  m_scatter = std::move(rendering.scatter);
  m_fog = std::move(rendering.fog);
  m_boundary_fog = std::move(rendering.boundary_fog);
  m_ambient_fog = std::move(rendering.ambient_fog);
  m_rain = std::move(rendering.rain);
  m_camera_service = std::make_unique<Game::Systems::CameraService>(
      m_session.visibility(), m_session.terrain());
  m_picking_service = std::make_unique<Game::Systems::PickingService>();
  m_rpg_commander_controller = std::make_unique<CommanderControlController>();
  m_rpg_telegraphs = std::make_unique<Render::GL::RpgTelegraphRenderer>();
  set_force_full_creature_lod(true);

  RendererBootstrap::initialize_world_systems(*m_world);
  m_session.visibility().initialize(
      k_terrain_width, k_terrain_height, k_terrain_tile_size);
  apply_initial_visibility();
  sync_camera_map_bounds(m_camera.get(), m_session.visibility());
  configure_runtime();
  regenerate_terrain();
  reset_camera();

  m_frame_clock.start();
  m_frame_timer.setTimerType(Qt::PreciseTimer);
  m_frame_timer.setInterval(k_interactive_frame_interval_ms);
  connect(&m_frame_timer, &QTimer::timeout, this, [this]() {
    if (m_batch_fixed_step > 0.0F) {

      if (context() != nullptr && context()->isValid()) {
        m_batch_frame_in_progress = true;
        makeCurrent();
        paintGL();
        doneCurrent();
        m_batch_frame_in_progress = false;
      }
    } else {
      update();
    }
  });
  m_frame_timer.start();
}

ArenaViewport::~ArenaViewport() {
  m_frame_timer.stop();
  m_units.clear();
  Render::GL::CameraVisibility::instance().clear_camera();
  m_session.terrain().clear();

  if (context() != nullptr) {
    makeCurrent();
    m_capture_target.reset();
    m_capture_preview_resolve.reset();
    m_terrain_scene.reset();
    m_scatter.reset();
    m_features.reset();
    m_surface.reset();
    m_fog.reset();
    m_boundary_fog.reset();
    m_ambient_fog.reset();
    m_rain.reset();
    if (m_renderer != nullptr) {
      m_renderer->shutdown();
      m_renderer.reset();
    }
    doneCurrent();
  }
}

void ArenaViewport::configure_runtime() {
  Game::Systems::initialize_default_content(m_session.nations());
  Game::Systems::NavGrid::initialize(k_terrain_width, k_terrain_height);

  m_unit_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::register_built_in_units(*m_unit_factory);
  Game::Map::MapTransformer::setFactoryRegistry(m_unit_factory);

  setup_default_players();
  sync_spawn_selection_defaults();
}

void ArenaViewport::setup_default_players() {
  auto& owners = m_session.owners();
  owners.clear();
  owners.register_owner_with_id(
      k_local_owner_id, Game::Systems::OwnerType::Player, "Arena Player");
  owners.register_owner_with_id(
      k_enemy_owner_id, Game::Systems::OwnerType::AI, "Arena Opponent");
  owners.set_owner_team(k_local_owner_id, 1);
  owners.set_owner_team(k_enemy_owner_id, 2);
  owners.set_local_player_id(k_local_owner_id);

  auto& nations = m_session.nations();
  nations.clear_player_assignments();
  nations.set_player_nation(k_local_owner_id, Game::Systems::NationID::RomanRepublic);
  nations.set_player_nation(k_enemy_owner_id, Game::Systems::NationID::Carthage);

  if (m_world != nullptr) {
    if (auto* ai_system = m_world->get_system<Game::Systems::AISystem>()) {
      ai_system->reinitialize();
    }
  }
}

void ArenaViewport::initializeGL() {
  if (m_gl_initialized || m_renderer == nullptr || m_camera == nullptr) {
    return;
  }

  QString error;
  m_gl_initialized = App::Core::WorldBootstrap::initialize(
      *m_renderer,
      *m_camera,
      m_terrain_scene != nullptr ? m_terrain_scene->ground() : nullptr,
      &error);

  if (!m_gl_initialized) {
    qWarning() << "ArenaViewport:" << error;
    return;
  }

  if (qEnvironmentVariableIsSet("SOI_GL_DEBUG")) {
    auto* logger = new QOpenGLDebugLogger(this);
    if (logger->initialize()) {
      connect(logger,
              &QOpenGLDebugLogger::messageLogged,
              this,
              [](const QOpenGLDebugMessage& message) {
                if (message.severity() != QOpenGLDebugMessage::NotificationSeverity) {
                  qWarning().noquote() << "GL debug:" << message.message();
                }
              });
      logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
    } else {
      qWarning() << "SOI_GL_DEBUG: no GL_KHR_debug on this context";
    }
  }

  configure_rendering_from_terrain();
  set_wireframe_enabled(m_wireframe_enabled);
  m_frame_clock.restart();
}

void ArenaViewport::resizeGL(int width, int height) {
  if (m_renderer != nullptr && width > 0 && height > 0) {
    m_renderer->set_viewport(width, height);
  }
}

namespace {
constexpr float k_max_simulation_substep = 1.0F / 30.0F;
} // namespace

void ArenaViewport::paintGL() {
  if (!m_gl_initialized || m_renderer == nullptr || m_camera == nullptr ||
      m_world == nullptr) {
    return;
  }

  QElapsedTimer frame_work_clock;
  frame_work_clock.start();
  qint64 timing_checkpoint_ns = 0;
  auto elapsed_phase_ms = [&]() {
    qint64 const now_ns = frame_work_clock.nsecsElapsed();
    double const elapsed =
        static_cast<double>(now_ns - timing_checkpoint_ns) / 1'000'000.0;
    timing_checkpoint_ns = now_ns;
    return elapsed;
  };

  float real_dt = m_batch_fixed_step > 0.0F ? m_batch_fixed_step : 1.0F / 60.0F;
  if (m_batch_fixed_step <= 0.0F && m_frame_clock.isValid()) {
    real_dt =
        std::clamp(static_cast<float>(m_frame_clock.restart()) / 1000.0F, 0.0F, 0.1F);
  }
  bool const sampled_frame = m_batch_fixed_step <= 0.0F || m_batch_frame_in_progress;
  if (sampled_frame && m_batch_fixed_step > 0.0F) {
    if (float const hitch_ms = take_due_presentation_hitch(); hitch_ms > 0.0F) {
      real_dt = hitch_ms / 1000.0F;
    }
  }
  m_fps = real_dt > 0.0F ? (m_fps * 0.9F + (1.0F / real_dt) * 0.1F) : m_fps;
  float const simulation_dt = (m_paused || !sampled_frame) ? 0.0F : real_dt;
  m_environment_clock.update(simulation_dt, m_paused);
  m_environment_hour = m_environment_clock.hour();
  m_time_of_day = Game::Map::time_of_day_for_hour(m_environment_hour);
  m_renderer->set_environment_lighting(active_lighting());

  sanitize_selection();
  apply_keyboard_camera_controls(real_dt);
  m_camera->update(real_dt);
  if (m_capture_orbit_ready && m_capture_orbit_speed != 0.0F) {
    m_capture_orbit_yaw += m_capture_orbit_speed * real_dt;
    m_camera->set_rts_view(m_capture_orbit_center,
                           m_capture_orbit_view.distance,
                           m_capture_orbit_view.angle,
                           m_capture_orbit_view.yaw + m_capture_orbit_yaw);
  }

  if (m_frame_hook && sampled_frame) {
    m_frame_hook(scenario_elapsed_seconds());
  }
  apply_cinematic_view();

  if (!m_paused) {
    step_world(simulation_dt);
  }
  m_feedback.advance(simulation_dt);

  std::erase_if(m_units, [this](const std::unique_ptr<Game::Units::Unit>& unit) {
    return unit == nullptr || m_world->get_entity(unit->id()) == nullptr;
  });
  update_active_scenario(simulation_dt);
  update_fog_of_war(simulation_dt);
  apply_cinematic_view();
  apply_capture_stabilization(simulation_dt);
  Arena::ArenaRenderedFrameTimings timings;
  timings.simulation_ms = elapsed_phase_ms();

  if (m_batch_render_suppressed && sampled_frame && !m_capture_active) {
    m_renderer->update_animation_time(simulation_dt);
    if (m_scenario_runner != nullptr) {
      publish_animation_clock();
    }
    return;
  }

  bool const capture_frame = m_capture_active && sampled_frame &&
                             static_cast<bool>(m_capture_sink) &&
                             ensure_capture_target();
  if (capture_frame) {
    m_capture_target->bind();
    m_renderer->set_viewport(m_capture_width, m_capture_height);
  } else if (width() > 0 && height() > 0) {
    m_renderer->set_viewport(width(), height());
  }

  if (m_flame_card_active) {
    render_flame_card(capture_frame ? m_capture_width : width(),
                      capture_frame ? m_capture_height : height());
    if (capture_frame) {
      deliver_capture_frame();
      render_capture_variants(true);
    }
    ++m_flame_card_frame;
    if (m_scenario_runner != nullptr && sampled_frame) {
      publish_animation_clock();
      publish_commander_presentation_trace();
      m_scenario_runner->observe_rendered_frame(timings);
    }
    return;
  }
  Render::GL::CameraVisibility::instance().set_camera(m_camera.get());

  update_selected_entities();
  sync_selection_summary();
  apply_attack_scrub_override();
  m_renderer->set_hovered_entity_id(m_hovered_entity_id);
  m_renderer->set_local_owner_id(k_local_owner_id);
  m_renderer->update_animation_time(simulation_dt);

  m_renderer->set_order_marker_all_owners(m_capture_gameplay_ui_all_owners);

  m_renderer->set_rpg_lens_detached(m_cinematic_view_valid);
  m_renderer->set_cinematic_mode(
      !m_capture_gameplay_ui &&
      (m_clean_capture || m_promo_mode ||
       (m_scenario_runner != nullptr &&
        m_scenario_runner->definition().suppress_ui_overlays)));

  m_renderer->set_world_view(Render::WorldView::of(m_session));
  m_renderer->begin_frame();
  submit_terrain_layers();
  timings.terrain_submit_ms = elapsed_phase_ms();
  m_renderer->render_world(m_world.get());
  timings.world_submit_ms = elapsed_phase_ms();
  if (auto* res = m_renderer->resources(); res != nullptr) {
    submit_world_effects(res);
  }
  timings.effects_submit_ms = elapsed_phase_ms();
  m_renderer->end_frame();
  timings.render_execute_ms = elapsed_phase_ms();
  if (sampled_frame) {
    sample_frame_continuity();
  }
  record_render_profile(timings);

  bool const capture_keeps_overlays =
      m_scenario_runner != nullptr &&
      m_scenario_runner->definition().capture_ui_overlays;
  bool const suppress_ui_overlays =
      m_clean_capture || m_terrain_review_mode || m_promo_mode ||
      (capture_frame && !capture_keeps_overlays) ||
      (m_scenario_runner != nullptr &&
       m_scenario_runner->definition().suppress_ui_overlays);
  if (!suppress_ui_overlays) {
    paint_ui_overlays();
  }
  timings.overlays_ms = elapsed_phase_ms();

  if (capture_frame) {
    deliver_capture_frame();
    render_capture_variants(false);
  }

  if (m_scenario_runner != nullptr && sampled_frame) {
    timings.total_ms =
        static_cast<double>(frame_work_clock.nsecsElapsed()) / 1'000'000.0;
    publish_scenario_frame(timings);
  }
  emit frame_rendered();
}

void ArenaViewport::step_world(float simulation_dt) {
  const Game::Command::ScopedImmediateDispatch immediate_orders;
  update_rpg_scenario_controller(simulation_dt);

  int const substeps = std::max(
      1, static_cast<int>(std::ceil(simulation_dt / k_max_simulation_substep - 1e-4F)));
  float const substep = simulation_dt / static_cast<float>(substeps);
  for (int step = 0; step < substeps; ++step) {
    m_world->update(substep);
  }
}

void ArenaViewport::submit_terrain_layers() {
  if (m_terrain_scene != nullptr) {
    Render::GL::TerrainSceneSubmitOptions terrain_options;
    const bool include_review_content =
        !m_terrain_review_mode || m_terrain_review_content_enabled;
    terrain_options.include_scatters =
        include_review_content &&
        (m_scenario_runner == nullptr ||
         !m_scenario_runner->definition().suppress_terrain_scatter);
    terrain_options.include_features =
        include_review_content &&
        (m_scenario_runner == nullptr ||
         !m_scenario_runner->definition().suppress_terrain_features);
    terrain_options.include_environment = !m_terrain_review_mode;
    m_terrain_scene->submit(*m_renderer, m_renderer->resources(), terrain_options);
  }
  if (m_fog_of_war_enabled && m_fog != nullptr) {
    m_fog->submit(*m_renderer, m_renderer->resources());
  }
}

void ArenaViewport::submit_world_effects(Render::GL::ResourceManager* res) {
  if (auto* arrow_system = m_world->get_system<Game::Systems::ArrowSystem>()) {
    Render::GL::render_arrows(m_renderer.get(), res, arrow_system->arrows());
  }
  {
    Render::GL::ProjectileViewContext view;
    view.local_owner_id = k_local_owner_id;
    view.reduced_effects = Game::Accessibility::MotionSettings::reduced_motion();
    Render::GL::render_projectiles(
        m_renderer.get(), res, m_world->render_effects_frame(), &view);
    Render::GL::render_rockfall(
        m_renderer.get(), m_world->render_effects_frame(), &view);
    Render::GL::render_rafts(m_renderer.get(), m_world->render_effects_frame());
  }
  {
    const auto& beams = m_world->render_effects_frame().healing_beams;
    Render::GL::render_healing_beams(m_renderer.get(), res, beams);
    Render::GL::render_healing_waves(m_renderer.get(), res, beams);
  }
  Render::GL::render_production_completions(
      m_renderer.get(),
      m_world.get(),
      k_local_owner_id,
      Game::Accessibility::MotionSettings::reduced_motion());
  Render::GL::render_healer_auras(m_renderer.get(), res, m_world.get());
  Render::GL::render_commander_auras(m_renderer.get(), res, m_world.get());

  if (m_scenario_runner == nullptr ||
      !m_scenario_runner->definition().suppress_combat_dust) {
    Render::GL::render_combat_dust(m_renderer.get(), res, m_world.get());
  }
  Render::GL::render_blood_stains(m_renderer.get(), res, m_world.get());
  render_attack_range_rings(res);
  render_target_focus_rings(res);
  const bool cinematic_capture = m_clean_capture || m_promo_mode ||
                                 (m_scenario_runner != nullptr &&
                                  m_scenario_runner->definition().suppress_ui_overlays);
  if (m_rpg_commander_id != 0 && m_rpg_telegraphs != nullptr && !cinematic_capture) {
    Engine::Core::EntityID locked_target_id = 0;
    if (auto* commander = m_world->get_entity(m_rpg_commander_id)) {
      if (auto const* targets =
              commander->get_component<Engine::Core::RpgCommanderTargetComponent>()) {
        locked_target_id = targets->explicit_lock_target_id;
      }
    }
    m_rpg_telegraphs->render(m_renderer.get(),
                             m_world.get(),
                             m_rpg_commander_id,
                             locked_target_id,
                             m_renderer->get_animation_time());
  }
}

void ArenaViewport::record_render_profile(
    Arena::ArenaRenderedFrameTimings& timings) const {
  auto const& render_profile = Render::Profiling::global_profile();
  timings.humanoid_preparation_ms =
      static_cast<double>(render_profile.humanoid_preparation_us) / 1000.0;
  timings.animation_sampling_ms =
      static_cast<double>(render_profile.animation_input_sampling_us) / 1000.0;
  timings.bpat_playback_ms =
      static_cast<double>(render_profile.bpat_playback_us) / 1000.0;
  timings.layout_generation_ms =
      static_cast<double>(render_profile.soldier_layout_generation_us) / 1000.0;
  timings.visible_soldiers = Render::GL::get_humanoid_render_stats().soldiers_rendered;
  timings.draw_calls = m_renderer->last_draw_command_count();
  auto const playback_stats = m_renderer->last_playback_stats();
  timings.prepared_batches = playback_stats.prepared_batches;
  timings.rigged_commands = playback_stats.rigged_commands;
  timings.rigged_instanced_draws = playback_stats.rigged_instanced_draws;
  timings.rigged_instanced_instances = playback_stats.rigged_instanced_instances;
  timings.rigged_single_draws = playback_stats.rigged_single_draws;
  timings.shadow_rigged_instanced_draws = playback_stats.shadow_rigged_instanced_draws;
  timings.shadow_rigged_instanced_instances =
      playback_stats.shadow_rigged_instanced_instances;
  timings.shadow_rigged_single_draws = playback_stats.shadow_rigged_single_draws;
  timings.gpu_shadow_ms = playback_stats.gpu_shadow_ms;
  timings.gpu_color_ms = playback_stats.gpu_color_ms;
  timings.gpu_wait_ms = playback_stats.gpu_wait_ms;
}

void ArenaViewport::paint_ui_overlays() {
  if (QOpenGLContext::currentContext() != nullptr) {
    auto* gl = QOpenGLContext::currentContext()->functions();
    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    gl->glUseProgram(0);
    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    gl->glDisable(GL_DEPTH_TEST);
    gl->glDisable(GL_CULL_FACE);
    gl->glDisable(GL_SCISSOR_TEST);
    gl->glDepthMask(GL_TRUE);
    gl->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    draw_debug_overlay(painter);
  }

  {
    QPainter stats_painter(this);
    stats_painter.setRenderHint(QPainter::Antialiasing, false);
    draw_stats_overlay(stats_painter);
    draw_rpg_hud(stats_painter);
    if (m_controls_overlay_visible) {
      draw_controls_overlay(stats_painter);
    }
  }
}

void ArenaViewport::deliver_capture_frame() {
  m_capture_camera = m_flame_card_active
                         ? Arena::Promo::CameraSample{}
                         : sample_capture_camera(m_capture_width, m_capture_height);
  stamp_capture_alpha_opaque();
  QImage const captured = m_capture_target->toImage();
  m_capture_target->release();
  if (context() != nullptr) {
    context()->functions()->glBindFramebuffer(GL_FRAMEBUFFER,
                                              defaultFramebufferObject());
  }
  if (!captured.isNull()) {
    m_capture_sink(captured);
  }
  present_capture_preview();
}

void ArenaViewport::publish_scenario_frame(
    const Arena::ArenaRenderedFrameTimings& timings) {
  publish_animation_clock();
  publish_commander_presentation_trace();
  m_scenario_runner->observe_rendered_frame(timings);
  std::size_t const issue_revision = m_scenario_runner->issue_revision();
  if (issue_revision > m_last_scenario_issue_revision) {
    auto const& issues = m_scenario_runner->report().issues;
    m_last_scenario_issue_revision = issue_revision;
    emit scenario_issue_detected(m_scenario_runner->definition().id,
                                 issues.empty() ? QString{} : issues.back().message);
  }
  if (m_scenario_runner->finished() && !m_scenario_finished_emitted) {
    m_scenario_finished_emitted = true;
    auto const& report = m_scenario_runner->report();
    emit scenario_finished(report.scenario_id, report.passed(), report.summary());
  }
}

void ArenaViewport::render_attack_range_rings(Render::GL::ResourceManager* resources) {
  m_attack_range_rings.clear();
  auto* selection = selection_system();
  if (selection == nullptr || m_world == nullptr || m_renderer == nullptr) {
    return;
  }

  const auto& selected = selection->get_selected_units();
  Game::Systems::AttackRangeRingRequest request;
  request.world = m_world.get();
  request.local_owner_id = k_local_owner_id;
  request.selection = selected;
  request.max_rings = Game::Systems::k_attack_range_max_rings;
  if (std::find(selected.begin(), selected.end(), m_hovered_entity_id) !=
      selected.end()) {
    request.focus_entity_id = m_hovered_entity_id;
  }
  m_attack_range_rings = Game::Systems::collect_attack_range_rings(request);
  Render::GL::render_attack_range_rings(
      m_renderer.get(), resources, m_attack_range_rings);
}

void ArenaViewport::render_target_focus_rings(Render::GL::ResourceManager* resources) {
  auto* selection = selection_system();
  if (selection == nullptr || m_world == nullptr || m_renderer == nullptr) {
    return;
  }
  const bool cinematic_capture = m_clean_capture || m_promo_mode ||
                                 (m_scenario_runner != nullptr &&
                                  m_scenario_runner->definition().suppress_ui_overlays);
  if (cinematic_capture) {
    return;
  }
  Game::Systems::TargetFocusRequest request;
  request.world = m_world.get();
  request.local_owner_id = k_local_owner_id;
  request.selection = &selection->get_selected_units();
  request.inspected = selection->inspected_entity();
  const auto markers = Game::Systems::collect_target_focus_markers(request);
  if (markers.empty()) {
    return;
  }
  std::vector<Render::GL::TargetFocusVisual> visuals;
  visuals.reserve(markers.size());
  for (const auto& marker : markers) {
    Render::GL::TargetFocusVisualRole role =
        Render::GL::TargetFocusVisualRole::LockedTarget;
    switch (marker.role) {
    case Game::Systems::TargetFocusRole::Inspected:
      role = Render::GL::TargetFocusVisualRole::Inspected;
      break;
    case Game::Systems::TargetFocusRole::LockedTarget:
      role = Render::GL::TargetFocusVisualRole::LockedTarget;
      break;
    case Game::Systems::TargetFocusRole::IncomingAttacker:
      role = Render::GL::TargetFocusVisualRole::IncomingAttacker;
      break;
    }
    visuals.push_back(
        {.position = QVector3D(marker.world_x, marker.world_y, marker.world_z),
         .radius = marker.radius,
         .role = role,
         .hostile = marker.hostile,
         .is_building = marker.is_building,
         .weight = marker.weight});
  }
  Render::GL::render_target_focus_rings(m_renderer.get(), resources, visuals);
}

void ArenaViewport::set_batch_render_suppressed(bool suppressed) {
  m_batch_render_suppressed = suppressed;
}

void ArenaViewport::set_batch_fixed_step(float seconds) {
  m_batch_fixed_step = std::max(0.0F, seconds);

  m_frame_timer.setTimerType(Qt::PreciseTimer);

  int const interval_ms =
      m_promo_mode
          ? 0
          : (m_batch_fixed_step > 0.0F
                 ? std::max(1,
                            static_cast<int>(std::lround(m_batch_fixed_step * 1000.0F)))
                 : k_interactive_frame_interval_ms);
  m_frame_timer.setInterval(interval_ms);
}

void ArenaViewport::pause_simulation(bool paused) {
  if (m_paused == paused) {
    return;
  }
  m_paused = paused;
  emit paused_changed(m_paused);
  update();
}

void ArenaViewport::reset_camera() {
  if (m_camera == nullptr) {
    return;
  }
  m_camera->set_rts_view({0.0F, 0.0F, 0.0F}, 42.0F, 45.0F, 225.0F);
  apply_scenario_camera_projection(42.0F);
  update();
}

void ArenaViewport::apply_scenario_camera_projection(float distance) {
  if (m_camera == nullptr) {
    return;
  }
  const float world_span =
      static_cast<float>(m_terrain_grid_extent) * k_terrain_tile_size;
  const float far_plane = std::max({Render::GL::CameraDefaults::k_default_far_plane,
                                    (distance * 2.5F) + (world_span * 1.2F)});
  const float aspect = height() > 0
                           ? static_cast<float>(width()) / static_cast<float>(height())
                           : 16.0F / 9.0F;
  m_camera->set_perspective(
      m_camera->get_fov(), aspect, m_camera->get_near(), far_plane);
}

void ArenaViewport::align_units_to_terrain() {
  if (m_world == nullptr) {
    return;
  }

  for (const auto& unit : m_units) {
    if (unit == nullptr) {
      continue;
    }
    if (auto* entity = m_world->get_entity(unit->id()); entity != nullptr) {
      Arena::align_entity_to_ground(*entity, m_session.terrain());
    }
  }
}
