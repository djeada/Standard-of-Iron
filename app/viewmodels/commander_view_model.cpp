#include "app/viewmodels/commander_view_model.h"

#include <QQuickWindow>

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

#include "app/commander/commander_mode_coordinator.h"
#include "app/commander/commander_status_builder.h"
#include "app/core/client_context.h"
#include "app/input/cursor_manager.h"
#include "app/input/input_command_handler.h"
#include "app/viewmodels/camera_view_model.h"
#include "app/viewmodels/placement_view_model.h"
#include "app/world/unit_queries.h"
#include "game/audio/audio_cues.h"
#include "game/audio/audio_system.h"
#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/session_context.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/nav_grid.h"
#include "game/systems/selection_system.h"
#include "render/scene_renderer.h"
#include "scene/camera.h"

namespace App::ViewModels {
namespace {

template <typename Callback>
class ScopeExit {
public:
  explicit ScopeExit(Callback callback)
      : m_callback(std::move(callback)) {}
  ScopeExit(const ScopeExit&) = delete;
  ScopeExit(ScopeExit&&) = delete;
  auto operator=(const ScopeExit&) -> ScopeExit& = delete;
  auto operator=(ScopeExit&&) -> ScopeExit& = delete;
  ~ScopeExit() { m_callback(); }

private:
  Callback m_callback;
};

} // namespace

CommanderViewModel::CommanderViewModel(const App::Core::ClientContext& context,
                                       App::Core::ClientHost& host,
                                       CameraViewModel& camera,
                                       PlacementViewModel& placement,
                                       QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_host(host)
    , m_camera(camera)
    , m_placement(placement)
    , m_mode(std::make_unique<App::Core::CommanderModeCoordinator>()) {
}

CommanderViewModel::~CommanderViewModel() = default;

auto CommanderViewModel::control_mode() const -> QString {
  return active() ? QStringLiteral("commander") : QStringLiteral("rts");
}

auto CommanderViewModel::game_mode() const -> QString {
  return rpg_mode() ? QStringLiteral("rpg") : QStringLiteral("rts");
}

auto CommanderViewModel::mode_state() const -> QString {
  return QString::fromLatin1(App::Core::to_string(m_mode->state()));
}

auto CommanderViewModel::capture_mode_signals() const -> ModeSignalBatch {
  return {.control_mode = control_mode(),
          .game_mode = game_mode(),
          .mode_state = mode_state()};
}

void CommanderViewModel::emit_mode_signal_changes(const ModeSignalBatch& before) {
  const auto after = capture_mode_signals();
  if (after.game_mode != before.game_mode) {
    emit game_mode_changed();
  }
  if (after.mode_state != before.mode_state) {
    emit mode_state_changed();
  }
  if (after.control_mode != before.control_mode) {
    emit control_mode_changed();
  }
}

void CommanderViewModel::bookmark_rts_camera() {
  if (m_context.rts_camera == nullptr) {
    return;
  }
  m_rts_camera_bookmark = App::Core::RtsCameraBookmark::capture(*m_context.rts_camera);
}

void CommanderViewModel::restore_rts_camera() {
  if (m_context.rts_camera == nullptr || !m_rts_camera_bookmark.valid) {
    return;
  }
  m_rts_camera_bookmark.restore(*m_context.rts_camera);
}

auto CommanderViewModel::available() const -> bool {
  const auto frame_lock = m_host.lock_frame();
  return find_local_commander() != nullptr;
}

auto CommanderViewModel::find_local_commander() const -> Engine::Core::Entity* {
  auto* world = m_context.world;
  if (world == nullptr) {
    return nullptr;
  }
  const int owner = m_context.local_owner_id;
  for (auto* entity :
       world->collect_entities_with<Engine::Core::CommanderComponent>()) {
    if (entity == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr) {
      continue;
    }
    if (unit->owner_id == owner && unit->health > 0) {
      return entity;
    }
  }
  return nullptr;
}

auto CommanderViewModel::controlled_commander_entity() const -> Engine::Core::Entity* {
  auto* world = m_context.world;
  if (world == nullptr || m_controlled_commander_id == 0) {
    return nullptr;
  }
  return world->get_entity(m_controlled_commander_id);
}

void CommanderViewModel::toggle_mode() {
  const auto frame_lock = m_host.lock_frame();
  if (active()) {
    exit_mode();
    return;
  }
  enter_mode();
}

void CommanderViewModel::enter_mode() {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  auto* commander = find_local_commander();
  auto* commander_camera = m_context.commander_camera;
  if ((m_context.level != nullptr && m_context.level->is_spectator_mode) ||
      commander == nullptr || commander_camera == nullptr) {
    return;
  }
  if (!m_mode->begin_enter()) {
    return;
  }
  m_control.set_feedback_bus(m_context.feedback);

  const auto signals_before = capture_mode_signals();
  const ScopeExit emit_once(
      [this, signals_before]() { emit_mode_signal_changes(signals_before); });

  cancel_active_placement();
  m_host.set_cursor_mode(CursorMode::Normal);

  bookmark_rts_camera();
  store_rts_selection();
  const auto effects = m_mode->enter_commander_control_mode(
      {.world = m_context.world,
       .commander = commander,
       .commander_camera = commander_camera,
       .commander_control = &m_control,
       .local_owner_id = m_context.local_owner_id,
       .is_spectator_mode =
           (m_context.level != nullptr && m_context.level->is_spectator_mode),
       .follow_selection_enabled = m_camera.following_selection()});
  if (!effects.entered) {
    m_mode->abort_transition();
    return;
  }

  if (effects.save_follow_selection_snapshot) {
    m_rts_follow_selection_snapshot = effects.saved_follow_selection_enabled;
  }
  m_camera.set_following_selection(false);

  m_controlled_commander_id = effects.controlled_commander_id;
  if (effects.commander_view_yaw.has_value()) {
    m_control.set_view_yaw(*effects.commander_view_yaw);
  }
  m_control.set_view_pitch(k_commander_rest_view_pitch_degrees);
  reset_input();
  emit active_camera_requested(commander_camera);

  enter_commander_runtime_mode();

  if (auto* world = m_context.world; world != nullptr) {
    (void)m_control.update(*world,
                           m_controlled_commander_id,
                           m_context.local_owner_id,
                           *commander_camera,
                           0.0F);
  }
  select_controlled_commander();
  m_mode->complete_transition();
  if (m_context.feedback != nullptr) {
    App::Core::PlayerFeedbackEvent event;
    event.type = App::Core::PlayerFeedbackType::CommanderModeEntered;
    event.entity = m_controlled_commander_id;
    m_context.feedback->publish(std::move(event));
  }
  Game::Audio::play_cue(Game::Audio::Cue::k_state_commander_enter);
}

void CommanderViewModel::exit_mode() {
  const auto frame_lock = m_host.lock_frame();
  const bool was_active = active();
  const auto signals_before = capture_mode_signals();
  const ScopeExit emit_once(
      [this, signals_before]() { emit_mode_signal_changes(signals_before); });
  static_cast<void>(m_mode->begin_exit());
  exit_commander_runtime_mode();
  reset_input();
  const auto effects = m_mode->exit_commander_control_mode(
      {.world = m_context.world,
       .controlled_commander_id = m_controlled_commander_id,
       .rts_follow_selection_snapshot_valid =
           m_rts_follow_selection_snapshot.has_value(),
       .rts_follow_selection_snapshot =
           m_rts_follow_selection_snapshot.value_or(false)});
  enter_rts_runtime_mode();
  m_controlled_commander_id = effects.controlled_commander_id;

  restore_rts_camera();
  emit active_camera_requested(m_context.rts_camera);
  if (effects.restored_follow_selection_enabled.has_value()) {
    m_camera.set_following_selection(*effects.restored_follow_selection_enabled);
  }
  restore_rts_selection();

  m_mode->force_inactive();

  if (was_active) {
    if (m_context.feedback != nullptr) {
      App::Core::PlayerFeedbackEvent event;
      event.type = App::Core::PlayerFeedbackType::CommanderModeExited;
      m_context.feedback->publish(std::move(event));
    }
    Game::Audio::play_cue(Game::Audio::Cue::k_state_commander_exit);
  }
}

void CommanderViewModel::enter_rts_runtime_mode() {
  m_control_mode = PlayerControlMode::Rts;
  m_game_mode = GameMode::Rts;
}

void CommanderViewModel::enter_commander_runtime_mode() {
  m_control_mode = PlayerControlMode::Commander;
  m_game_mode = GameMode::Rpg;
}

void CommanderViewModel::cancel_active_placement() {
  if (m_placement.is_placing_formation()) {
    m_placement.on_formation_cancel();
  }
  if (m_placement.is_placing_construction()) {
    m_placement.on_construction_cancel();
  }
}

void CommanderViewModel::exit_commander_runtime_mode() {
  m_hit_stop_timer = 0.0F;
  m_telegraphs.clear();
}

void CommanderViewModel::store_rts_selection() {
  m_saved_rts_selection_ids =
      m_mode->store_rts_selection({.selection_controller = m_context.selection})
          .saved_rts_selection_ids;
}

void CommanderViewModel::select_controlled_commander() {
  m_mode->select_controlled_commander(
      {.selection_controller = m_context.selection,
       .controlled_commander_id = m_controlled_commander_id,
       .local_owner_id = m_context.local_owner_id});
}

void CommanderViewModel::restore_rts_selection() {
  const auto effects = m_mode->restore_rts_selection(
      {.world = m_context.world,
       .local_owner_id = m_context.local_owner_id,
       .saved_rts_selection_ids = &m_saved_rts_selection_ids});
  if (effects.sync_selection_flags || effects.emit_selected_units_changed) {
    emit rts_selection_restored();
  }
  if (effects.clear_saved_rts_selection_ids) {
    m_saved_rts_selection_ids.clear();
  }
}

void CommanderViewModel::key_down(int key, int modifiers) {
  const auto frame_lock = m_host.lock_frame();
  (void)modifiers;
  m_control.key_down(key);
}

void CommanderViewModel::key_up(int key, int modifiers) {
  const auto frame_lock = m_host.lock_frame();
  (void)modifiers;
  m_control.key_up(key);
}

void CommanderViewModel::reset_input() {
  m_control.reset();
}

void CommanderViewModel::release_input() {
  const auto frame_lock = m_host.lock_frame();
  m_control.release_all_input();
}

void CommanderViewModel::primary_action_down() {
  const auto frame_lock = m_host.lock_frame();
  m_control.primary_action_down();
}

void CommanderViewModel::primary_action_up() {
  const auto frame_lock = m_host.lock_frame();
  m_control.primary_action_up();
}

void CommanderViewModel::secondary_action_down() {
  const auto frame_lock = m_host.lock_frame();
  m_control.secondary_action_down();
}

void CommanderViewModel::secondary_action_up() {
  const auto frame_lock = m_host.lock_frame();
  m_control.secondary_action_up();
}

void CommanderViewModel::mouse_move(qreal dx, qreal dy) {
  const auto frame_lock = m_host.lock_frame();
  m_control.mouse_move(dx, dy);
}

void CommanderViewModel::mouse_look_at(qreal sx,
                                       qreal sy,
                                       qreal center_sx,
                                       qreal center_sy) {
  const auto frame_lock = m_host.lock_frame();
  m_control.mouse_look_at(sx, sy, center_sx, center_sy, m_context.window);
}

void CommanderViewModel::center_mouse(qreal center_sx, qreal center_sy) {
  const auto frame_lock = m_host.lock_frame();
  m_control.center_mouse(center_sx, center_sy, m_context.window);
}

void CommanderViewModel::trigger_aura() {
  const auto frame_lock = m_host.lock_frame();
  auto* world = m_context.world;
  if (world == nullptr) {
    return;
  }
  const int owner = m_context.local_owner_id;

  Engine::Core::Entity* commander_entity = nullptr;
  if (active()) {
    commander_entity = controlled_commander_entity();
  } else if (auto* selection = world->get_system<Game::Systems::SelectionSystem>()) {
    for (const auto entity_id : selection->get_selected_units()) {
      auto* candidate = world->get_entity(entity_id);
      if (candidate == nullptr) {
        continue;
      }
      const auto* unit = candidate->get_component<Engine::Core::UnitComponent>();
      if (unit != nullptr && unit->owner_id == owner && unit->health > 0 &&
          candidate->get_component<Engine::Core::CommanderComponent>() != nullptr) {
        commander_entity = candidate;
        break;
      }
    }
  }

  if (commander_entity == nullptr) {
    return;
  }
  Game::Command::submit(*world,
                        Game::Command::Source::LocalPlayer,
                        owner,
                        Game::Command::UseCommanderAbility{
                            .commander = commander_entity->get_id(),
                            .ability = Game::Command::CommanderAbility::Aura});
}

void CommanderViewModel::trigger_rally() {
  const auto frame_lock = m_host.lock_frame();
  if (!active()) {
    return;
  }
  start_flag_rally();
}

void CommanderViewModel::dodge() {
  const auto frame_lock = m_host.lock_frame();
  if (active()) {
    m_control.request_dodge();
  }
}

void CommanderViewModel::jump() {
  const auto frame_lock = m_host.lock_frame();
  if (active()) {
    m_control.request_jump();
  }
}

void CommanderViewModel::cycle_lock_on() {
  const auto frame_lock = m_host.lock_frame();
  auto* world = m_context.world;
  if (!active() || world == nullptr) {
    return;
  }
  m_control.cycle_lock_on_target(
      *world, m_controlled_commander_id, m_context.local_owner_id);
}

void CommanderViewModel::special_action() {
  const auto frame_lock = m_host.lock_frame();
  if (active()) {
    m_control.special_action();
  }
}

void CommanderViewModel::vanguard_rush() {
  const auto frame_lock = m_host.lock_frame();
  if (active()) {
    m_control.request_vanguard_rush();
  }
}

void CommanderViewModel::second_wind() {
  const auto frame_lock = m_host.lock_frame();
  if (active()) {
    m_control.request_second_wind();
  }
}

void CommanderViewModel::toggle_camera_mode() {
  const auto frame_lock = m_host.lock_frame();
  auto* world = m_context.world;
  if (!active() || world == nullptr) {
    return;
  }
  m_control.toggle_close_camera_mode(
      *world, m_controlled_commander_id, m_context.local_owner_id);
}

void CommanderViewModel::toggle_weapon_stance() {
  const auto frame_lock = m_host.lock_frame();
  auto* world = m_context.world;
  if (!active() || world == nullptr) {
    return;
  }
  m_control.toggle_weapon_stance(
      *world, m_controlled_commander_id, m_context.local_owner_id);
}

auto CommanderViewModel::is_placing_rally() const -> bool {
  return (m_context.cursor != nullptr
              ? m_context.cursor->mode()
              : CursorMode::Normal) == CursorMode::PlaceCommanderRally;
}

void CommanderViewModel::start_flag_rally() {
  const auto frame_lock = m_host.lock_frame();
  const auto effects = m_mode->begin_commander_flag_rally(
      {.world = m_context.world,
       .local_commander = find_local_commander(),
       .controlled_commander = controlled_commander_entity(),
       .local_owner_id = m_context.local_owner_id,
       .commander_mode_active = active(),
       .cursor_mode = (m_context.cursor != nullptr ? m_context.cursor->mode()
                                                   : CursorMode::Normal)});
  if (effects.should_exit_commander_mode) {
    exit_mode();
    return;
  }
  if (effects.reset_commander_input) {
    reset_input();
  }
  if (effects.clear_rally_preview) {
    m_rally_preview.reset();
  }
  if (effects.cursor_mode.has_value()) {
    m_host.set_cursor_mode(*effects.cursor_mode);
  }
  if (effects.seed_preview_from_view_center) {
    seed_flag_rally_preview_from_view_center();
  }
}

void CommanderViewModel::confirm_flag_rally(qreal sx, qreal sy) {
  const auto frame_lock = m_host.lock_frame();
  const auto& viewport = *m_context.viewport;
  const auto effects = m_mode->confirm_commander_flag_rally(
      {.world = m_context.world,
       .local_commander = find_local_commander(),
       .controlled_commander = controlled_commander_entity(),
       .picking_service = m_context.picking,
       .camera = m_context.active_camera,
       .viewport_width = viewport.width,
       .viewport_height = viewport.height,
       .screen_x = sx,
       .screen_y = sy,
       .local_owner_id = m_context.local_owner_id,
       .commander_mode_active = active(),
       .cursor_mode = (m_context.cursor != nullptr ? m_context.cursor->mode()
                                                   : CursorMode::Normal)});
  if (effects.reset_commander_input) {
    reset_input();
  }
  if (effects.clear_rally_preview) {
    m_rally_preview.reset();
  }
  if (effects.cursor_mode.has_value()) {
    m_host.set_cursor_mode(*effects.cursor_mode);
  }
}

void CommanderViewModel::cancel_flag_rally() {
  const auto frame_lock = m_host.lock_frame();
  const auto effects = m_mode->cancel_commander_flag_rally(
      (m_context.cursor != nullptr ? m_context.cursor->mode() : CursorMode::Normal));
  if (effects.clear_rally_preview) {
    m_rally_preview.reset();
  }
  if (effects.cursor_mode.has_value()) {
    m_host.set_cursor_mode(*effects.cursor_mode);
  }
}

void CommanderViewModel::begin_barracks_rally() {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  const auto effects = m_mode->begin_barracks_rally_placement(
      {.world = m_context.world, .local_owner_id = m_context.local_owner_id});
  if (effects.clear_rally_preview) {
    m_rally_preview.reset();
  }
  if (effects.cursor_mode.has_value()) {
    m_host.set_cursor_mode(*effects.cursor_mode);
  }
  if (effects.rally_preview.has_value()) {
    m_rally_preview = effects.rally_preview;
  }
}

void CommanderViewModel::confirm_barracks_rally(qreal sx, qreal sy) {
  const auto frame_lock = m_host.lock_frame();
  const auto effects = m_mode->confirm_barracks_rally_placement(
      {.world = m_context.world,
       .production_manager = m_context.production,
       .viewport = &*m_context.viewport,
       .local_owner_id = m_context.local_owner_id,
       .screen_x = sx,
       .screen_y = sy,
       .cursor_mode = (m_context.cursor != nullptr ? m_context.cursor->mode()
                                                   : CursorMode::Normal)});
  if (effects.clear_rally_preview) {
    m_rally_preview.reset();
  }
  if (effects.cursor_mode.has_value()) {
    m_host.set_cursor_mode(*effects.cursor_mode);
  }
}

void CommanderViewModel::cancel_barracks_rally() {
  const auto frame_lock = m_host.lock_frame();
  const auto effects = m_mode->cancel_barracks_rally_placement(
      (m_context.cursor != nullptr ? m_context.cursor->mode() : CursorMode::Normal));
  if (effects.clear_rally_preview) {
    m_rally_preview.reset();
  }
  if (effects.cursor_mode.has_value()) {
    m_host.set_cursor_mode(*effects.cursor_mode);
  }
}

void CommanderViewModel::seed_barracks_rally_preview_from_selection() {
  if (auto preview = m_mode->seed_barracks_rally_preview_from_selection(
          {.world = m_context.world, .local_owner_id = m_context.local_owner_id});
      preview.has_value()) {
    m_rally_preview = *preview;
  }
}

void CommanderViewModel::update_rally_preview_at(qreal sx, qreal sy) {
  auto* picking = m_context.picking;
  auto* camera = m_context.active_camera;
  auto* cursor = m_context.cursor;
  if (picking == nullptr || camera == nullptr || cursor == nullptr ||
      m_context.viewport == nullptr) {
    return;
  }

  const int width = m_context.viewport->width;
  const int height = m_context.viewport->height;
  QVector3D hit;
  if (cursor->mode() == CursorMode::PlaceCommanderRally &&
      picking->screen_to_ground(QPointF(sx, sy), *camera, width, height, hit)) {
    m_rally_preview = Game::Systems::NavGrid::snap_to_walkable_ground(hit);
  } else if (cursor->mode() == CursorMode::PlaceBarracksRally &&
             picking->screen_to_surface(m_context.session->terrain(),
                                        QPointF(sx, sy),
                                        *camera,
                                        width,
                                        height,
                                        hit)) {
    m_rally_preview = hit;
  }
}

void CommanderViewModel::seed_flag_rally_preview_from_view_center() {
  auto* picking = m_context.picking;
  auto* camera = m_context.active_camera;
  const auto& viewport = *m_context.viewport;
  if (picking == nullptr || camera == nullptr || viewport.width <= 0 ||
      viewport.height <= 0) {
    return;
  }

  QVector3D hit;
  if (!picking->screen_to_ground(QPointF(viewport.width * 0.5, viewport.height * 0.5),
                                 *camera,
                                 viewport.width,
                                 viewport.height,
                                 hit)) {
    return;
  }
  m_rally_preview = Game::Systems::NavGrid::snap_to_walkable_ground(hit);
}

void CommanderViewModel::update_control_mode(float dt) {
  const auto effects = m_mode->update_commander_control_mode(
      {.world = m_context.world,
       .commander_camera = m_context.commander_camera,
       .commander_control = &m_control,
       .controlled_commander_id = m_controlled_commander_id,
       .local_owner_id = m_context.local_owner_id,
       .dt = dt});
  if (effects.should_exit_commander_mode) {
    exit_mode();
    return;
  }
  if (effects.hit_stop_duration.has_value()) {
    m_hit_stop_timer = *effects.hit_stop_duration;
    m_hit_stop_total = *effects.hit_stop_duration;
  }
}

void CommanderViewModel::sample_frame_intent() {
  if (!active()) {
    return;
  }
  m_control.sample_frame_intent(m_context.window);
}

void CommanderViewModel::update_camera_presentation(float dt) {
  if (!active() || m_context.world == nullptr ||
      m_context.commander_camera == nullptr) {
    return;
  }
  m_control.update_camera_presentation(
      *m_context.world, m_controlled_commander_id, *m_context.commander_camera, dt);
}

void CommanderViewModel::restore_direct_control_if_ready() {
  const auto effects = m_mode->restore_controlled_commander_direct_control_if_ready(
      {.world = m_context.world,
       .controlled_commander_id = m_controlled_commander_id,
       .commander_mode_active = active(),
       .placing_commander_rally = is_placing_rally()});
  if (effects.reset_commander_input) {
    reset_input();
  }
}

auto CommanderViewModel::time_effect_scale(float scaled_dt, bool paused) -> float {
  if (!active()) {
    return 1.0F;
  }
  if (!paused && m_hit_stop_timer > 0.0F) {
    m_hit_stop_timer -= scaled_dt;
    if (m_hit_stop_timer < 0.0F) {
      m_hit_stop_timer = 0.0F;
    } else {
      const float progress =
          1.0F - std::clamp(m_hit_stop_timer / m_hit_stop_total, 0.0F, 1.0F);
      return progress < 0.5F ? 0.04F : (0.04F + 0.96F * (progress - 0.5F) * 2.0F);
    }
  }
  return 1.0F;
}

auto CommanderViewModel::should_render_selected_entity(Engine::Core::EntityID id) const
    -> bool {
  return !active() || id != m_controlled_commander_id;
}

void CommanderViewModel::render_effects() {
  auto* world = m_context.world;
  auto* renderer = m_context.renderer;
  if (!active() || world == nullptr || renderer == nullptr ||
      m_controlled_commander_id == 0) {
    return;
  }

  Engine::Core::EntityID locked_target_id = m_control.locked_target_id();
  if (auto* commander = world->get_entity(m_controlled_commander_id)) {
    if (const auto* rpg_targets =
            commander->get_component<Engine::Core::RpgCommanderTargetComponent>()) {
      locked_target_id = rpg_targets->explicit_lock_target_id;
    }
  }

  m_telegraphs.render(renderer,
                      world,
                      m_controlled_commander_id,
                      locked_target_id,
                      renderer->get_animation_time());
}

void CommanderViewModel::reset_for_new_match() {
  const auto signals_before = capture_mode_signals();
  const ScopeExit emit_once(
      [this, signals_before]() { emit_mode_signal_changes(signals_before); });
  m_mode->force_inactive();
  enter_rts_runtime_mode();
  m_saved_rts_selection_ids.clear();
  m_rts_follow_selection_snapshot.reset();
  m_rally_preview.reset();
  {
    const std::lock_guard<std::mutex> damage_lock(m_damage_events_mutex);
    m_damage_events.clear();
  }
}

auto CommanderViewModel::status() const -> QVariantMap {
  const auto snapshot = m_status.read();
  return snapshot ? *snapshot : QVariantMap{};
}

void CommanderViewModel::publish_frame() {
  if (!active()) {
    if (!m_status_published_empty) {
      m_status.publish(QVariantMap{});
      m_status_published_empty = true;
    }
    return;
  }
  m_status_published_empty = false;
  App::Core::CommanderStatusInput input;
  input.world = m_context.world;
  input.controlled_commander_id = m_controlled_commander_id;
  input.dodge_active = m_control.is_dodge_rolling();
  input.locked_target_id = m_control.locked_target_id();
  input.rally_placing = is_placing_rally();
  m_status.publish(App::Core::build_controlled_commander_status(input));
}

auto CommanderViewModel::record_rpg_hit(const Engine::Core::CombatHitEvent& event)
    -> bool {
  auto* world = m_context.world;
  if (world == nullptr || m_controlled_commander_id == 0) {
    return false;
  }
  if (event.attacker_id != m_controlled_commander_id) {
    return true;
  }
  auto* target = world->get_entity(event.target_id);
  if (target == nullptr) {
    return true;
  }
  const auto* transform = target->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return true;
  }

  float max_health = 0.0F;
  if (const auto* unit = target->get_component<Engine::Core::UnitComponent>();
      unit != nullptr && unit->max_health > 0) {
    max_health = static_cast<float>(unit->max_health);
  }

  const float damage_ratio =
      max_health > 0.0F
          ? std::clamp(static_cast<float>(event.damage) / max_health, 0.0F, 1.5F)
          : 0.0F;
  const int lane = static_cast<int>(m_damage_event_sequence % 5U) - 2;
  ++m_damage_event_sequence;

  const std::lock_guard<std::mutex> damage_lock(m_damage_events_mutex);
  if (static_cast<int>(m_damage_events.size()) >= k_max_damage_events) {
    m_damage_events.erase(m_damage_events.begin());
  }
  m_damage_events.push_back({transform->position.x,
                             transform->position.y + 1.8F,
                             transform->position.z,
                             event.damage,
                             damage_ratio,
                             lane,
                             event.is_killing_blow});
  return true;
}

auto CommanderViewModel::pop_damage_events() -> QVariantList {
  std::vector<DamageEvent> events;
  {
    const std::lock_guard<std::mutex> damage_lock(m_damage_events_mutex);
    events.swap(m_damage_events);
  }

  QVariantList list;
  list.reserve(static_cast<int>(events.size()));
  for (const auto& event : events) {
    QVariantMap entry;
    entry["x"] = static_cast<double>(event.wx);
    entry["y"] = static_cast<double>(event.wy);
    entry["z"] = static_cast<double>(event.wz);
    entry["damage"] = event.damage;
    entry["damageRatio"] = static_cast<double>(event.damage_ratio);
    entry["lane"] = event.lane;
    entry["killingBlow"] = event.killing_blow;
    list.append(entry);
  }
  return list;
}

} // namespace App::ViewModels
