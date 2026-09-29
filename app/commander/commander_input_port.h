#pragma once

#include <QVector3D>

#include <cstdint>
#include <mutex>
#include <optional>

#include "app/commander/commander_input_snapshot.h"
#include "app/commander/commander_latency_probe.h"
#include "app/commander/commander_presentation_trace.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Core {

struct CommanderHeldInput {
  bool forward = false;
  bool backward = false;
  bool left = false;
  bool right = false;
  bool turn_left = false;
  bool turn_right = false;
  bool run = false;
  bool primary_action = false;
  bool heavy_action_requested = false;
  bool secondary_action = false;
  bool dodge_requested = false;
  bool jump_requested = false;
  bool special_action_requested = false;
  bool shield_bash_requested = false;
  bool vanguard_rush_requested = false;
  bool second_wind_requested = false;
};

enum class EdgeOutcome : std::uint8_t {
  None,
  Consumed,
  Refused
};

class CommanderInputPort {
public:
  void set_latency_probe(CommanderLatencyProbe* probe) { m_latency_probe = probe; }

  void key_down(int key);
  void key_up(int key);
  void primary_action_down();
  void primary_action_up();
  void request_heavy_action();
  void secondary_action_down();
  void secondary_action_up();
  void request_dodge();
  void request_dodge(const QVector3D& world_direction);
  void request_jump();
  void special_action();
  void request_vanguard_rush();
  void request_second_wind();

  [[nodiscard]] auto held() -> CommanderHeldInput& { return m_input; }
  [[nodiscard]] auto held() const -> const CommanderHeldInput& { return m_input; }
  [[nodiscard]] auto locked_copy() const -> CommanderHeldInput;
  void release_buttons();

  [[nodiscard]] auto tick() -> CommanderInputSnapshot& { return m_tick_input; }
  [[nodiscard]] auto tick() const -> const CommanderInputSnapshot& {
    return m_tick_input;
  }
  [[nodiscard]] auto edges() const -> const CommanderInputTrace& { return m_edges; }

  void capture_tick();
  [[nodiscard]] auto exchange_recorded(Engine::Core::World& world,
                                       Engine::Core::EntityID commander_id,
                                       float view_yaw) -> std::optional<float>;
  void consume_press_edges(bool primary_consumed);
  void settle_primary_press();
  void clear_jump_edge();
  void clear_dodge_edge();
  void clear_ability_edges();
  void release_for_rally();
  void release_all();

  void record_jump(EdgeOutcome outcome);
  void record_dodge(EdgeOutcome outcome);

private:
  [[nodiscard]] auto take_snapshot() -> CommanderInputSnapshot;
  void discard_edges(CommanderInputSnapshot& snapshot);
  void note_input() const;

  CommanderLatencyProbe* m_latency_probe = nullptr;
  CommanderHeldInput m_input;
  bool m_primary_press_pending = false;
  bool m_has_requested_dodge_direction = false;
  QVector3D m_requested_dodge_direction{0.0F, 0.0F, 0.0F};
  std::uint64_t m_input_snapshot_sequence = 0;
  bool m_carried_primary_press = false;
  CommanderInputSnapshot m_tick_input;
  CommanderInputTrace m_edges;
  mutable std::mutex m_input_mutex;
};

} // namespace App::Core
