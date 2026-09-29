#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>

#include "app/commander/commander_input_snapshot.h"
#include "app/commander/commander_latency_probe.h"
#include "app/commander/commander_targeting.h"
#include "app/core/player_feedback.h"

namespace Engine::Core {
class World;
class Entity;
class TransformComponent;
class CommanderComponent;
class CombatIntentQueueComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Core {

struct IntentGate {
  bool dodge_clear{true};
  bool guarding{false};
  bool bow_stance{false};
  bool jump_followup_pending{false};
};

struct PrimaryRequestContext {
  Engine::Core::World& world;
  Engine::Core::EntityID commander_id{0};
  int local_owner_id{0};
  float view_yaw{0.0F};
  int move_right_axis{0};
  int move_forward_axis{0};
  QVector3D commander_position{0.0F, 0.0F, 0.0F};
};

struct StrikeSinks {
  PlayerFeedbackBus* feedback{nullptr};
  CommanderLatencyProbe* probe{nullptr};
};

struct MeleeAdvanceInput {
  float aim_delta_x{0.0F};
  float aim_delta_y{0.0F};
  float view_pitch{0.0F};
  int move_right_axis{0};
  int move_forward_axis{0};
  bool primary_held{false};
  bool guard_held{false};
  float dt{0.0F};
};

class CommanderStrike {
public:
  [[nodiscard]] auto primary_held_duration() const -> float {
    return m_primary_held_duration;
  }

  void advance_hold_timers(Engine::Core::CommanderComponent* commander_data,
                           bool primary_held,
                           bool attack_animation_active,
                           float dt);

  [[nodiscard]] static auto open_queue(Engine::Core::Entity& commander, float dt)
      -> Engine::Core::CombatIntentQueueComponent*;

  [[nodiscard]] auto queue_intents(Engine::Core::Entity& commander,
                                   Engine::Core::CombatIntentQueueComponent& intents,
                                   const CommanderInputSnapshot& tick,
                                   const IntentGate& gate) -> bool;

  [[nodiscard]] auto dispatch_pending(const PrimaryRequestContext& context,
                                      CommanderTargeting& targeting,
                                      Engine::Core::Entity& commander,
                                      Engine::Core::CombatIntentQueueComponent& intents,
                                      const StrikeSinks& sinks) -> bool;

  [[nodiscard]] auto request_primary(const PrimaryRequestContext& context,
                                     CommanderTargeting& targeting) -> bool;

  void advance_melee(Engine::Core::Entity& commander, const MeleeAdvanceInput& input);

  [[nodiscard]] auto
  observe_hits(Engine::Core::Entity& commander) -> std::optional<float>;

  void release_input() { m_primary_held_duration = 0.0F; }
  void reset();

private:
  void queue_held_continuation(Engine::Core::Entity& commander,
                               Engine::Core::CombatIntentQueueComponent& intents,
                               const CommanderInputSnapshot& tick,
                               const IntentGate& gate);

  float m_combo_miss_timer = 0.0F;
  float m_primary_held_duration = 0.0F;
  float m_held_restart_delay = 0.0F;
  std::uint8_t m_observed_action_hit_count = 0;
};

} // namespace App::Core
