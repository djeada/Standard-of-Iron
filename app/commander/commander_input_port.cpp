#include "app/commander/commander_input_port.h"

#include <Qt>

#include "game/accessibility/commander_input_settings.h"
#include "game/command/replay.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"

namespace App::Core {

void CommanderInputPort::note_input() const {
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_input();
  }
}

void CommanderInputPort::key_down(int key) {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  note_input();
  switch (key) {
  case Qt::Key_W:
    m_input.forward = true;
    break;
  case Qt::Key_S:
    m_input.backward = true;
    break;
  case Qt::Key_A:
    m_input.left = true;
    break;
  case Qt::Key_D:
    m_input.right = true;
    break;
  case Qt::Key_Q:
    m_input.turn_left = true;
    break;
  case Qt::Key_E:
    m_input.turn_right = true;
    break;
  case Qt::Key_Shift:
    m_input.run = true;
    break;
  default:
    break;
  }
}

void CommanderInputPort::key_up(int key) {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  switch (key) {
  case Qt::Key_W:
    m_input.forward = false;
    break;
  case Qt::Key_S:
    m_input.backward = false;
    break;
  case Qt::Key_A:
    m_input.left = false;
    break;
  case Qt::Key_D:
    m_input.right = false;
    break;
  case Qt::Key_Q:
    m_input.turn_left = false;
    break;
  case Qt::Key_E:
    m_input.turn_right = false;
    break;
  case Qt::Key_Shift:
    m_input.run = false;
    break;
  default:
    break;
  }
}

void CommanderInputPort::primary_action_down() {
  note_input();
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  ++m_edges.primary_press_sequence;
  m_input.primary_action = true;
  m_primary_press_pending = true;
}

void CommanderInputPort::primary_action_up() {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  if (m_input.primary_action) {
    ++m_edges.primary_release_sequence;
  }
  m_input.primary_action = false;
}

void CommanderInputPort::request_heavy_action() {
  note_input();
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  m_input.heavy_action_requested = true;
}

void CommanderInputPort::secondary_action_down() {
  note_input();
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  if (Game::Accessibility::CommanderInput::guard_is_toggle()) {
    if (m_input.secondary_action) {
      ++m_edges.guard_release_sequence;
      m_input.secondary_action = false;
    } else {
      ++m_edges.guard_press_sequence;
      m_input.secondary_action = true;
    }
    return;
  }
  if (!m_input.secondary_action) {
    ++m_edges.guard_press_sequence;
  }
  m_input.secondary_action = true;
}

void CommanderInputPort::secondary_action_up() {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  if (Game::Accessibility::CommanderInput::guard_is_toggle()) {
    return;
  }
  if (m_input.secondary_action) {
    ++m_edges.guard_release_sequence;
  }
  m_input.secondary_action = false;
}

void CommanderInputPort::request_dodge() {
  note_input();
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  ++m_edges.dodge_request_sequence;
  m_has_requested_dodge_direction = false;
  m_input.dodge_requested = true;
}

void CommanderInputPort::request_dodge(const QVector3D& world_direction) {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  ++m_edges.dodge_request_sequence;
  m_requested_dodge_direction =
      QVector3D(world_direction.x(), 0.0F, world_direction.z());
  m_has_requested_dodge_direction =
      m_requested_dodge_direction.lengthSquared() > 0.0001F;
  m_input.dodge_requested = true;
}

void CommanderInputPort::request_jump() {
  note_input();
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  ++m_edges.jump_request_sequence;
  m_input.jump_requested = true;
}

void CommanderInputPort::special_action() {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  if (m_input.secondary_action) {
    m_input.shield_bash_requested = true;
  } else {
    m_input.special_action_requested = true;
  }
}

void CommanderInputPort::request_vanguard_rush() {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  m_input.vanguard_rush_requested = true;
}

void CommanderInputPort::request_second_wind() {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  m_input.second_wind_requested = true;
}

auto CommanderInputPort::locked_copy() const -> CommanderHeldInput {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  return m_input;
}

auto CommanderInputPort::take_snapshot() -> CommanderInputSnapshot {
  const std::lock_guard<std::mutex> guard(m_input_mutex);

  CommanderInputSnapshot snapshot;
  snapshot.forward = m_input.forward;
  snapshot.backward = m_input.backward;
  snapshot.left = m_input.left;
  snapshot.right = m_input.right;
  snapshot.turn_left = m_input.turn_left;
  snapshot.turn_right = m_input.turn_right;
  snapshot.run = m_input.run;
  snapshot.primary_held = m_input.primary_action;
  snapshot.guard_held = m_input.secondary_action;

  snapshot.primary_pressed = m_primary_press_pending;
  snapshot.heavy_pressed = m_input.heavy_action_requested;
  snapshot.dodge_pressed = m_input.dodge_requested;
  snapshot.jump_pressed = m_input.jump_requested;
  snapshot.special_pressed = m_input.special_action_requested;
  snapshot.shield_bash_pressed = m_input.shield_bash_requested;
  snapshot.vanguard_rush_pressed = m_input.vanguard_rush_requested;
  snapshot.second_wind_pressed = m_input.second_wind_requested;

  snapshot.has_dodge_direction = m_has_requested_dodge_direction;
  snapshot.dodge_direction = m_requested_dodge_direction;

  m_primary_press_pending = false;
  m_input.heavy_action_requested = false;
  m_input.dodge_requested = false;
  m_input.jump_requested = false;
  m_input.special_action_requested = false;
  m_input.shield_bash_requested = false;
  m_input.vanguard_rush_requested = false;
  m_input.second_wind_requested = false;
  m_has_requested_dodge_direction = false;
  m_requested_dodge_direction = QVector3D(0.0F, 0.0F, 0.0F);

  snapshot.sequence = ++m_input_snapshot_sequence;
  return snapshot;
}

void CommanderInputPort::capture_tick() {
  m_tick_input = take_snapshot();
  m_tick_input.primary_pressed =
      m_tick_input.primary_pressed || m_carried_primary_press;
  m_carried_primary_press = false;
}

auto CommanderInputPort::exchange_recorded(Engine::Core::World& world,
                                           Engine::Core::EntityID commander_id,
                                           float view_yaw) -> std::optional<float> {
  auto* session = Game::Session::SessionContext::for_world(world);
  if (session == nullptr) {
    return std::nullopt;
  }

  const std::uint64_t tick = session->clock().tick();

  if (auto* player = session->replay_player()) {
    if (const auto* recorded = player->commander_input(tick)) {
      m_tick_input = CommanderInputSnapshot::from_record(*recorded);
      return recorded->view_yaw;
    }
  }

  if (auto* recorder = session->replay_recorder()) {
    recorder->record_commander_input(tick,
                                     m_tick_input.to_record(commander_id, view_yaw));
  }
  return std::nullopt;
}

void CommanderInputPort::discard_edges(CommanderInputSnapshot& snapshot) {
  if (m_carried_primary_press) {
    ++m_edges.primary_dropped_sequence;
    m_carried_primary_press = false;
  }
  if (snapshot.primary_pressed) {
    ++m_edges.primary_dropped_sequence;
    snapshot.primary_pressed = false;
  }
  snapshot.heavy_pressed = false;
  if (snapshot.dodge_pressed) {
    ++m_edges.dodge_refused_sequence;
    snapshot.dodge_pressed = false;
  }
  if (snapshot.jump_pressed) {
    ++m_edges.jump_refused_sequence;
    snapshot.jump_pressed = false;
  }
  snapshot.special_pressed = false;
  snapshot.shield_bash_pressed = false;
  snapshot.vanguard_rush_pressed = false;
  snapshot.second_wind_pressed = false;
  snapshot.has_dodge_direction = false;
  snapshot.dodge_direction = QVector3D(0.0F, 0.0F, 0.0F);
}

void CommanderInputPort::consume_press_edges(bool primary_consumed) {
  if (primary_consumed) {
    ++m_edges.primary_consumed_sequence;
  }
  m_tick_input.primary_pressed = false;
  m_tick_input.heavy_pressed = false;
  m_tick_input.special_pressed = false;
}

void CommanderInputPort::settle_primary_press() {
  if (!m_tick_input.primary_held) {
    if (m_tick_input.primary_pressed) {
      ++m_edges.primary_dropped_sequence;
    }
    m_tick_input.primary_pressed = false;
  } else {
    m_carried_primary_press = m_tick_input.primary_pressed;
  }
  m_tick_input.heavy_pressed = false;
  m_tick_input.special_pressed = false;
}

void CommanderInputPort::clear_jump_edge() {
  m_tick_input.jump_pressed = false;
}

void CommanderInputPort::clear_dodge_edge() {
  m_tick_input.dodge_pressed = false;
  m_tick_input.has_dodge_direction = false;
  m_tick_input.dodge_direction = QVector3D(0.0F, 0.0F, 0.0F);
}

void CommanderInputPort::clear_ability_edges() {
  m_tick_input.shield_bash_pressed = false;
  m_tick_input.vanguard_rush_pressed = false;
  m_tick_input.second_wind_pressed = false;
}

void CommanderInputPort::release_for_rally() {
  discard_edges(m_tick_input);
  if (m_tick_input.primary_held) {
    ++m_edges.primary_release_sequence;
  }
  if (m_tick_input.guard_held) {
    ++m_edges.guard_release_sequence;
  }
  release_buttons();
  m_tick_input = {};
}

void CommanderInputPort::release_buttons() {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  m_input.primary_action = false;
  m_input.secondary_action = false;
}

void CommanderInputPort::release_all() {
  auto pending = take_snapshot();
  discard_edges(pending);
  discard_edges(m_tick_input);
  if (pending.primary_held) {
    ++m_edges.primary_release_sequence;
  }
  if (pending.guard_held) {
    ++m_edges.guard_release_sequence;
  }
  {
    const std::lock_guard<std::mutex> guard(m_input_mutex);
    m_input = {};
  }
  m_tick_input = {};
}

void CommanderInputPort::record_jump(EdgeOutcome outcome) {
  if (outcome == EdgeOutcome::Consumed) {
    ++m_edges.jump_consumed_sequence;
  } else if (outcome == EdgeOutcome::Refused) {
    ++m_edges.jump_refused_sequence;
  }
}

void CommanderInputPort::record_dodge(EdgeOutcome outcome) {
  if (outcome == EdgeOutcome::Consumed) {
    ++m_edges.dodge_consumed_sequence;
  } else if (outcome == EdgeOutcome::Refused) {
    ++m_edges.dodge_refused_sequence;
  }
}

} // namespace App::Core
