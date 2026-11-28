#include "app/viewmodels/commander_view_model.h"

#include <QQuickWindow>

#include <algorithm>
#include <cmath>
#include <optional>
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
#include "game/accessibility/motion_settings.h"
#include "game/audio/audio_cues.h"
#include "game/audio/audio_system.h"
#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/command/command_system.h"
#include "game/commander/commander_entity_access.h"
#include "game/commander/commander_handoff.h"
#include "game/core/component_combat.h"
#include "game/core/component_commander.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/render_bridge/picking_service.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/combat_system/target_rules.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"
#include "render/scene/scene_renderer.h"
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
  return Game::Commander::find_commander(*world, m_context.local_owner_id);
}

auto CommanderViewModel::controlled_commander_entity() const -> Engine::Core::Entity* {
  auto* world = m_context.world;
  if (world == nullptr || m_controlled_commander_id == 0) {
    return nullptr;
  }
  return world->get_entity(m_controlled_commander_id);
}

void CommanderViewModel::toggle_mode() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  const auto action = active() ? Game::Command::CommanderAction::ExitControl
                               : Game::Command::CommanderAction::EnterControl;
  if (queue_action(action)) {
    return;
  }
  if (active()) {
    leave_commander_mode();
  } else {
    enter_mode();
  }
}

void CommanderViewModel::enter_mission_mode() {
  const auto frame_lock = m_host.lock_frame();
  m_skip_entry_dive = true;
  enter_mode();
}

auto CommanderViewModel::capture_view_state() const -> QJsonObject {
  const auto* entity = active() ? controlled_commander_entity() : nullptr;
  const auto* world = m_context.world;
  const auto id = entity != nullptr ? entity->get_id() : Engine::Core::EntityID{0};
  const auto* unit =
      entity != nullptr ? world->try_get<Engine::Core::UnitComponent>(id) : nullptr;
  if (unit == nullptr || unit->health <= 0 || finale_active()) {
    return {{"active", false}};
  }
  QJsonObject view{{"active", true},
                   {"yaw", m_control.view_yaw()},
                   {"pitch", m_control.view_pitch()}};
  if (m_center_first_army_view) {
    view["center_first_army_view"] = true;
  }
  if (const auto* data = world->try_get<Engine::Core::CommanderComponent>(id)) {
    view["close_camera"] = data->close_camera_mode;
  }
  if (const auto* aim = world->try_get<Engine::Core::RpgCommanderAimComponent>(id);
      aim != nullptr && aim->stance_resolved) {
    view["bow"] = aim->stance == Engine::Core::FpvWeaponStance::Bow;
  }
  return view;
}

void CommanderViewModel::restore_view_state(const QJsonObject& state) {
  const auto frame_lock = m_host.lock_frame();
  const auto* commander = find_local_commander();
  if (commander == nullptr) {
    return;
  }
  Game::Commander::end_control(m_context.world, commander->get_id());
  if (state.value("active").toBool()) {
    m_restored_view = state;
    enter_mission_mode();
  }
}

void CommanderViewModel::enter_mode() {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (ignores_player_input() ||
      queue_action(Game::Command::CommanderAction::EnterControl)) {
    return;
  }
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
  if (auto* system = m_context.world->get_system<Game::Command::CommandSystem>()) {
    m_control.bind_runtime(system->commander_runtime(m_context.local_owner_id));
  }
  const QJsonObject restored_view = std::exchange(m_restored_view, {});
  m_control.set_view_yaw(static_cast<float>(restored_view.value("yaw").toDouble(
      effects.commander_view_yaw.value_or(m_control.view_yaw()))));
  m_control.set_view_pitch(static_cast<float>(
      restored_view.value("pitch").toDouble(k_commander_rest_view_pitch_degrees)));
  const auto restored_flag = [&restored_view](const char* key) {
    const auto value = restored_view.value(QLatin1String(key));
    return value.isBool() ? std::optional<bool>(value.toBool()) : std::nullopt;
  };
  Game::Commander::restore_view_preferences(*m_context.world,
                                            m_controlled_commander_id,
                                            restored_flag("close_camera"),
                                            restored_flag("bow"));
  reset_input();
  emit active_camera_requested(commander_camera);

  enter_commander_runtime_mode();

  if (auto* world = m_context.world; world != nullptr) {
    m_control.update_camera_presentation(
        *world, m_controlled_commander_id, *commander_camera, 0.0F);
  }
  const bool animate_entry = !std::exchange(m_skip_entry_dive, false);
  m_center_first_army_view =
      restored_view.isEmpty() ? !animate_entry
                              : restored_view.value("center_first_army_view").toBool();
  if (animate_entry && m_context.rts_camera != nullptr && m_rts_camera_bookmark.valid &&
      !Game::Accessibility::MotionSettings::reduced_motion()) {
    m_entry_dive.begin(m_context.rts_camera->get_position(),
                       m_context.rts_camera->get_target());
    show_entry_dive();
  } else {
    m_entry_dive.end();
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
  if (!m_applying_logged_action && follows_replay()) {
    return;
  }
  if (!finale_active() && queue_action(Game::Command::CommanderAction::ExitControl)) {
    return;
  }
  leave_commander_mode();
}

void CommanderViewModel::leave_commander_mode() {
  const auto frame_lock = m_host.lock_frame();
  const bool was_active = active();
  if (std::exchange(m_center_first_army_view, false) && was_active &&
      m_rts_camera_bookmark.valid) {
    const auto* commander = controlled_commander_entity();
    if (commander != nullptr) {
      const auto id = commander->get_id();
      const auto* unit = m_context.world->try_get<Engine::Core::UnitComponent>(id);
      const auto* transform =
          m_context.world->try_get<Engine::Core::TransformComponent>(id);
      if (unit != nullptr && unit->health > 0 && transform != nullptr) {
        const QVector3D target(
            transform->position.x, transform->position.y, transform->position.z);
        m_rts_camera_bookmark.position += target - m_rts_camera_bookmark.target;
        m_rts_camera_bookmark.target = target;
      }
    }
  }
  clear_finale();
  m_entry_dive.end();
  const auto signals_before = capture_mode_signals();
  const ScopeExit emit_once(
      [this, signals_before]() { emit_mode_signal_changes(signals_before); });
  static_cast<void>(m_mode->begin_exit());
  exit_commander_runtime_mode();
  reset_input();
  m_control.release_pointer();
  const auto effects = m_mode->exit_commander_control_mode(
      {.world = m_context.world,
       .controlled_commander_id = m_controlled_commander_id,
       .rts_follow_selection_snapshot_valid =
           m_rts_follow_selection_snapshot.has_value(),
       .rts_follow_selection_snapshot =
           m_rts_follow_selection_snapshot.value_or(false)});
  enter_rts_runtime_mode();
  m_controlled_commander_id = effects.controlled_commander_id;
  m_control.bind_runtime(nullptr);

  restore_rts_camera();
  emit active_camera_requested(m_context.rts_camera);
  if (effects.restored_follow_selection_enabled.has_value()) {
    m_camera.set_following_selection(*effects.restored_follow_selection_enabled);
  }
  restore_rts_selection();

  m_mode->force_inactive();

  if (was_active) {
    (void)queue_action(Game::Command::CommanderAction::ExitControl);
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
  ++m_overlay_session;
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
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  (void)modifiers;
  m_control.key_down(key);
}

void CommanderViewModel::key_up(int key, int modifiers) {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  (void)modifiers;
  m_control.key_up(key);
}

void CommanderViewModel::reset_input() {
  m_control.reset();
}

void CommanderViewModel::release_input_for_rally() {
  m_control.release_for_rally();
}

void CommanderViewModel::release_input() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  (void)queue_action(Game::Command::CommanderAction::ReleaseInput);
  m_control.release_all_input();
}

void CommanderViewModel::primary_action_down() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  m_control.primary_action_down();
}

void CommanderViewModel::primary_action_up() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  m_control.primary_action_up();
}

void CommanderViewModel::heavy_action() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  m_control.request_heavy_action();
}

void CommanderViewModel::secondary_action_down() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  m_control.secondary_action_down();
}

void CommanderViewModel::secondary_action_up() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  m_control.secondary_action_up();
}

void CommanderViewModel::mouse_move(qreal dx, qreal dy) {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  m_control.mouse_move(dx, dy);
}

void CommanderViewModel::mouse_look_at(qreal sx,
                                       qreal sy,
                                       qreal center_sx,
                                       qreal center_sy) {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  m_control.mouse_look_at(sx, sy, center_sx, center_sy, m_context.window);
}

void CommanderViewModel::aim_view_at_world(float x, float z) {
  if (ignores_player_input() || !active() || m_context.world == nullptr) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  auto const* transform = m_context.world->try_get<Engine::Core::TransformComponent>(
      m_controlled_commander_id);
  if (transform == nullptr) {
    return;
  }
  const float dx = x - transform->position.x;
  const float dz = z - transform->position.z;
  if ((dx * dx) + (dz * dz) < 1.0e-4F) {
    return;
  }
  constexpr float k_radians_to_degrees = 57.29577951308232F;
  m_control.set_view_yaw(std::atan2(dx, dz) * k_radians_to_degrees);
}

auto CommanderViewModel::aim_view_at_nearest_enemy(float max_range,
                                                   bool commanders_only) -> bool {
  if (ignores_player_input() || !active() || m_context.world == nullptr) {
    return false;
  }
  const auto frame_lock = m_host.lock_frame();
  auto& world = *m_context.world;
  auto const* transform =
      world.try_get<Engine::Core::TransformComponent>(m_controlled_commander_id);
  if (transform == nullptr) {
    return false;
  }

  constexpr float k_formation_reach = 14.0F;
  std::vector<Engine::Core::EntityID> nearby;
  world.spatial_index().refresh(world);
  world.spatial_index().for_each_in_radius(
      transform->position.x,
      transform->position.z,
      max_range + k_formation_reach,
      [&nearby, this](const Engine::Core::WorldSpatialIndex::Entry& entry) {
        if (entry.id != m_controlled_commander_id) {
          nearby.push_back(entry.id);
        }
      });

  auto& owners = Game::Session::session_for(world).owners();
  float best_sq = max_range * max_range;
  std::optional<QVector3D> best;
  for (Engine::Core::EntityID const candidate_id : nearby) {
    auto* candidate = world.get_entity(candidate_id);
    if (candidate == nullptr ||
        (commanders_only &&
         world.try_get<Engine::Core::CommanderComponent>(candidate_id) == nullptr)) {
      continue;
    }
    if (Game::Systems::Combat::evaluate_target(
            owners,
            m_context.local_owner_id,
            candidate,
            {.intent = Game::Systems::Combat::EngagementIntent::AutoAcquired,
             .allow_buildings = false}) != Game::Systems::Combat::TargetRefusal::None) {
      continue;
    }
    for (auto const& soldier :
         Game::Systems::RpgCombat::live_soldier_targets(*candidate)) {
      const float dx = soldier.position.x() - transform->position.x;
      const float dz = soldier.position.z() - transform->position.z;
      const float distance_sq = (dx * dx) + (dz * dz);
      if (distance_sq < best_sq) {
        best_sq = distance_sq;
        best = soldier.position;
      }
    }
  }
  if (!best.has_value()) {
    return false;
  }
  aim_view_at_world(best->x(), best->z());
  return true;
}

void CommanderViewModel::center_mouse(qreal center_sx, qreal center_sy) {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  m_control.center_mouse(center_sx, center_sy, m_context.window);
}

void CommanderViewModel::release_pointer() {
  const auto frame_lock = m_host.lock_frame();
  m_control.release_pointer();
}

void CommanderViewModel::trigger_aura() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  auto* world = m_context.world;
  if (world == nullptr) {
    return;
  }
  const int owner = m_context.local_owner_id;

  Engine::Core::Entity* commander_entity = nullptr;
  if (active()) {
    commander_entity = controlled_commander_entity();
  } else if (auto* selection = &Game::Session::session_for(*world).selection()) {
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
  if (active()) {
    auto const* commander_data =
        world->try_get<Engine::Core::CommanderComponent>(commander_entity->get_id());
    if (commander_data == nullptr || !commander_data->can_activate_aura_ability()) {
      m_control.note_refusal("commander.ability_aura");
      return;
    }
  }
  Game::Command::submit(*world,
                        Game::Command::Source::LocalPlayer,
                        owner,
                        Game::Command::UseCommanderAbility{
                            .commander = commander_entity->get_id(),
                            .ability = Game::Command::CommanderAbility::Aura});
}

void CommanderViewModel::trigger_rally() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  if (!active()) {
    return;
  }
  const auto effects = m_mode->rally_where_the_commander_stands(
      {.world = m_context.world,
       .local_commander = find_local_commander(),
       .controlled_commander = controlled_commander_entity(),
       .local_owner_id = m_context.local_owner_id,
       .commander_mode_active = active(),
       .cursor_mode = (m_context.cursor != nullptr ? m_context.cursor->mode()
                                                   : CursorMode::Normal)});
  if (!effects.reset_commander_input) {
    m_control.note_refusal("commander.rally");
    return;
  }
  (void)queue_action(Game::Command::CommanderAction::ReleaseForRally);
  release_input_for_rally();
  m_rally_preview.reset();
}

void CommanderViewModel::dodge() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  if (active()) {
    m_control.request_dodge();
  }
}

void CommanderViewModel::jump() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  if (active()) {
    m_control.request_jump();
  }
}

void CommanderViewModel::cycle_lock_on() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  auto* world = m_context.world;
  if (!active() || world == nullptr) {
    return;
  }
  if (queue_action(Game::Command::CommanderAction::CycleLockOn)) {
    return;
  }
  m_control.cycle_lock_on_target(
      *world, m_controlled_commander_id, m_context.local_owner_id);
}

void CommanderViewModel::toggle_lock_on() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  auto* world = m_context.world;
  if (!active() || world == nullptr) {
    return;
  }
  if (queue_action(Game::Command::CommanderAction::ToggleLockOn)) {
    return;
  }
  m_control.toggle_lock_on_target(
      *world, m_controlled_commander_id, m_context.local_owner_id);
}

void CommanderViewModel::switch_lock_target(int direction) {
  if (ignores_player_input() || direction == 0) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  auto* world = m_context.world;
  if (!active() || world == nullptr) {
    return;
  }
  if (queue_action(direction < 0 ? Game::Command::CommanderAction::SwitchLockLeft
                                 : Game::Command::CommanderAction::SwitchLockRight)) {
    return;
  }
  m_control.switch_lock_on_target(
      *world, m_controlled_commander_id, m_context.local_owner_id, direction);
}

auto CommanderViewModel::lock_active() const -> bool {
  const auto frame_lock = m_host.lock_frame();
  return active() && m_control.locked_target_id() != 0;
}

void CommanderViewModel::special_action() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  if (active()) {
    m_control.special_action();
  }
}

void CommanderViewModel::vanguard_rush() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  if (active()) {
    m_control.request_vanguard_rush();
  }
}

void CommanderViewModel::second_wind() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  if (active()) {
    m_control.request_second_wind();
  }
}

void CommanderViewModel::toggle_camera_mode() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  auto* world = m_context.world;
  if (!active() || world == nullptr) {
    return;
  }
  if (queue_action(Game::Command::CommanderAction::ToggleCameraMode)) {
    return;
  }
  m_control.toggle_close_camera_mode(
      *world, m_controlled_commander_id, m_context.local_owner_id);
}

void CommanderViewModel::toggle_weapon_stance() {
  if (ignores_player_input()) {
    return;
  }
  const auto frame_lock = m_host.lock_frame();
  auto* world = m_context.world;
  if (!active() || world == nullptr) {
    return;
  }
  if (queue_action(Game::Command::CommanderAction::ToggleWeaponStance)) {
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
  if (ignores_player_input()) {
    return;
  }
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
  if (active() && !effects.cursor_mode.has_value()) {
    m_control.note_refusal("commander.rally");
    return;
  }
  if (effects.reset_commander_input) {
    release_input_for_rally();
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
  if (ignores_player_input()) {
    return;
  }
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
    (void)queue_action(Game::Command::CommanderAction::ReleaseForRally);
    release_input_for_rally();
  }
  if (effects.clear_rally_preview) {
    m_rally_preview.reset();
  }
  if (effects.cursor_mode.has_value()) {
    m_host.set_cursor_mode(*effects.cursor_mode);
  }
}

void CommanderViewModel::cancel_flag_rally() {
  if (ignores_player_input()) {
    return;
  }
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
  if (m_finale.active() && !m_finale_body_live) {
    return;
  }
  const auto effects = m_mode->update_commander_control_mode(
      {.world = m_context.world,
       .commander_camera = m_context.commander_camera,
       .commander_control = &m_control,
       .controlled_commander_id = m_controlled_commander_id,
       .local_owner_id = m_context.local_owner_id,
       .dt = dt});
  if (effects.should_exit_commander_mode) {
    if (m_finale.active()) {

      m_finale_body_live = false;
      return;
    }
    leave_commander_mode();
    return;
  }
}

void CommanderViewModel::sample_frame_intent() {
  if (!active()) {
    return;
  }
  m_control.sample_frame_intent(m_context.window);
}

void CommanderViewModel::publish_commander_tick(float dt) {
  if (active() && m_context.world != nullptr) {
    m_control.publish_runtime_tick(*m_context.world, m_controlled_commander_id, dt);
  }
}

void CommanderViewModel::update_camera_presentation(float dt) {
  if (!active() || m_context.world == nullptr ||
      m_context.commander_camera == nullptr) {
    return;
  }
  if (m_finale.active()) {
    m_finale.advance(dt);
    auto pose = m_finale.pose();

    constexpr float k_ground_clearance = 0.9F;
    pose.eye.setY(std::max(
        pose.eye.y(),
        Game::Systems::PickingService::surface_height_at(pose.eye.x(), pose.eye.z()) +
            k_ground_clearance));
    m_context.commander_camera->look_at(
        pose.eye, pose.target, QVector3D(0.0F, 1.0F, 0.0F));
    if (m_finale.take_hand_back()) {
      QMetaObject::invokeMethod(
          this, [this]() { finish_finale(); }, Qt::QueuedConnection);
    }
    return;
  }
  m_control.update_camera_presentation(
      *m_context.world, m_controlled_commander_id, *m_context.commander_camera, dt);
  if (m_entry_dive.active()) {
    m_entry_dive.advance(dt);
    show_entry_dive();
  }
}

void CommanderViewModel::show_entry_dive() {
  auto* camera = m_context.commander_camera;
  if (camera == nullptr || !m_entry_dive.active()) {
    return;
  }
  auto const shown = m_entry_dive.blend(
      {.eye = camera->get_position(), .target = camera->get_target()});
  camera->look_at(shown.eye, shown.target, QVector3D(0.0F, 1.0F, 0.0F));
}

void CommanderViewModel::restore_direct_control_if_ready() {
  const auto effects = m_mode->restore_controlled_commander_direct_control_if_ready(
      {.world = m_context.world,
       .controlled_commander_id = m_controlled_commander_id,
       .commander_mode_active = active(),
       .placing_commander_rally = is_placing_rally()});
  if (effects.reset_commander_input) {
    release_input_for_rally();
  }
}

auto CommanderViewModel::follows_replay() const -> bool {
  auto* world = m_context.world;
  if (world == nullptr) {
    return false;
  }
  auto* session = Game::Session::SessionContext::for_world(*world);
  return session != nullptr && session->log_follower();
}

auto CommanderViewModel::ignores_player_input() const -> bool {
  return !m_applying_logged_action && (finale_active() || follows_replay());
}

auto CommanderViewModel::finale() const -> QString {
  switch (static_cast<App::Core::CommanderFinale::Kind>(
      m_finale_kind.load(std::memory_order_acquire))) {
  case App::Core::CommanderFinale::Kind::Fallen:
    return QStringLiteral("fallen");
  case App::Core::CommanderFinale::Kind::Victory:
    return QStringLiteral("victory");
  case App::Core::CommanderFinale::Kind::Defeat:
    return QStringLiteral("defeat");
  case App::Core::CommanderFinale::Kind::None:
    break;
  }
  return {};
}

void CommanderViewModel::announce_finale_change() {

  QMetaObject::invokeMethod(
      this, [this]() { emit finale_changed(); }, Qt::QueuedConnection);
}

auto CommanderViewModel::begin_finale(App::Core::CommanderFinale::Kind kind) -> bool {
  const auto frame_lock = m_host.lock_frame();
  auto* camera = m_context.commander_camera;
  if (!active() || camera == nullptr || m_context.world == nullptr ||
      kind == App::Core::CommanderFinale::Kind::None ||
      Game::Accessibility::MotionSettings::reduced_motion()) {
    return false;
  }
  if (m_finale.active()) {
    return true;
  }
  auto const* transform = m_context.world->try_get<Engine::Core::TransformComponent>(
      m_controlled_commander_id);
  QVector3D ground = camera->get_target();
  if (transform != nullptr) {
    ground = QVector3D(transform->position.x,
                       Game::Systems::PickingService::surface_height_at(
                           transform->position.x, transform->position.z),
                       transform->position.z);
  }
  (void)queue_action(Game::Command::CommanderAction::ReleaseInput);
  m_control.release_all_input();
  m_finale.begin(kind, ground, camera->get_position(), camera->get_target());
  m_finale_body_live = kind != App::Core::CommanderFinale::Kind::Fallen;
  m_finale_kind.store(static_cast<int>(kind), std::memory_order_release);
  announce_finale_change();
  return true;
}

void CommanderViewModel::clear_finale() {
  if (!m_finale.active() && !finale_active()) {
    return;
  }
  m_finale.end();
  m_finale_body_live = false;
  m_finale_kind.store(0, std::memory_order_release);
  announce_finale_change();
}

void CommanderViewModel::conclude_for_outcome(const QString& victory_state) {
  const auto frame_lock = m_host.lock_frame();
  if (!active() || follows_replay()) {
    return;
  }
  if (m_finale.active()) {

    return;
  }
  using Kind = App::Core::CommanderFinale::Kind;
  Kind kind = Kind::None;
  if (victory_state == QLatin1String("victory")) {
    kind = Kind::Victory;
  } else if (victory_state == QLatin1String("defeat")) {
    auto const* unit = m_context.world != nullptr
                           ? m_context.world->try_get<Engine::Core::UnitComponent>(
                                 m_controlled_commander_id)
                           : nullptr;
    kind = unit != nullptr && unit->health > 0 ? Kind::Defeat : Kind::Fallen;
  }
  if (!begin_finale(kind)) {
    leave_commander_mode();
  }
}

void CommanderViewModel::controlled_commander_fell() {
  const auto frame_lock = m_host.lock_frame();
  if (!active()) {
    return;
  }
  if (follows_replay() || !begin_finale(App::Core::CommanderFinale::Kind::Fallen)) {
    leave_commander_mode();
  }
}

void CommanderViewModel::finish_finale() {
  const auto frame_lock = m_host.lock_frame();
  if (!m_finale.active()) {
    return;
  }
  if (active()) {
    leave_commander_mode();
  } else {
    clear_finale();
  }
}

auto CommanderViewModel::queue_action(Game::Command::CommanderAction action) -> bool {
  auto* world = m_context.world;
  if (m_applying_logged_action || world == nullptr ||
      world->get_system<Game::Command::CommandSystem>() == nullptr) {
    return false;
  }
  auto* session = Game::Session::SessionContext::for_world(*world);
  return session != nullptr &&
         session->commands().log().submit(Game::Command::Source::LocalPlayer,
                                          m_context.local_owner_id,
                                          Game::Command::CommanderActionRecord{
                                              .action = action,
                                              .view_yaw = m_control.view_yaw(),
                                              .view_pitch = m_control.view_pitch()});
}

void CommanderViewModel::apply_logged_actions() {
  auto* world = m_context.world;
  if (world == nullptr) {
    return;
  }
  auto* session = Game::Session::SessionContext::for_world(*world);
  if (session == nullptr) {
    return;
  }
  const auto due = session->commands().log().entries_at(session->clock().tick());
  m_applying_logged_action = true;
  const ScopeExit restore([this]() { m_applying_logged_action = false; });
  for (const auto& entry : due) {
    if (entry->owner == m_context.local_owner_id) {
      if (const auto* record =
              std::get_if<Game::Command::CommanderActionRecord>(&entry->payload)) {
        apply_logged_action(*record);
      }
    }
  }
}

void CommanderViewModel::apply_logged_action(
    const Game::Command::CommanderActionRecord& record) {
  using Game::Command::CommanderAction;
  switch (record.action) {
  case CommanderAction::EnterControl:
    if (!active()) {
      enter_mode();
    }
    break;
  case CommanderAction::ExitControl:
    if (active()) {
      exit_mode();
    }
    break;
  case CommanderAction::ToggleControl:
    toggle_mode();
    break;
  case CommanderAction::ReleaseForRally:
    release_for_rally();
    break;
  default:
    break;
  }
}

void CommanderViewModel::release_for_rally() {
  const auto frame_lock = m_host.lock_frame();
  if (!active()) {
    return;
  }
  release_input_for_rally();
  m_rally_preview.reset();
}

auto CommanderViewModel::time_effect_scale(float scaled_dt, bool paused) -> float {

  static_cast<void>(scaled_dt);
  static_cast<void>(paused);

  return finale_active() ? App::Core::CommanderFinale::k_time_scale : 1.0F;
}

auto CommanderViewModel::should_render_selected_entity(Engine::Core::EntityID id) const
    -> bool {
  return !active() || id != m_controlled_commander_id;
}

auto CommanderViewModel::overlay_frame() const
    -> std::optional<App::Core::CommanderOverlayFrame> {
  if (!active() || m_controlled_commander_id == 0) {
    return std::nullopt;
  }
  return App::Core::CommanderOverlayFrame{.commander_id = m_controlled_commander_id,
                                          .locked_target_id =
                                              m_control.locked_target_id(),
                                          .session = m_overlay_session};
}

void CommanderViewModel::reset_for_new_match() {
  m_skip_entry_dive = false;
  m_center_first_army_view = false;
  m_restored_view = {};
  const auto signals_before = capture_mode_signals();
  const ScopeExit emit_once(
      [this, signals_before]() { emit_mode_signal_changes(signals_before); });
  clear_finale();
  m_mode->force_inactive();
  enter_rts_runtime_mode();
  ++m_overlay_session;
  m_saved_rts_selection_ids.clear();
  m_rts_follow_selection_snapshot.reset();
  m_rally_preview.reset();
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
  input.view_yaw = m_control.view_yaw();
  input.rally_placing = is_placing_rally();
  input.refusal_sequence = m_control.last_refusal().sequence;
  input.refusal_action = m_control.last_refusal().action;
  m_status.publish(App::Core::build_controlled_commander_status(input));
}

auto CommanderViewModel::classify_hit(const Engine::Core::CombatHitEvent& event) const
    -> HitRouting {
  if (m_context.world == nullptr || m_controlled_commander_id == 0) {
    return HitRouting::Rts;
  }
  if (event.attacker_id == m_controlled_commander_id) {
    return HitRouting::CommanderBurst;
  }
  const auto* commander = m_context.world->try_get<Engine::Core::TransformComponent>(
      m_controlled_commander_id);
  const auto* target =
      m_context.world->try_get<Engine::Core::TransformComponent>(event.target_id);
  if (commander == nullptr || target == nullptr) {
    return HitRouting::Suppressed;
  }
  const float dx = target->position.x - commander->position.x;
  const float dz = target->position.z - commander->position.z;
  return dx * dx + dz * dz <= k_hit_tick_radius * k_hit_tick_radius
             ? HitRouting::Rts
             : HitRouting::Suppressed;
}

} // namespace App::ViewModels
