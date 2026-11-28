#pragma once

#include "game/command/commander_input.h"
#include "game/commander/commander_abilities.h"
#include "game/commander/commander_body_facing.h"
#include "game/commander/commander_defence.h"
#include "game/commander/commander_input_port.h"
#include "game/commander/commander_locomotion.h"
#include "game/commander/commander_lunge.h"
#include "game/commander/commander_motor.h"
#include "game/commander/commander_strike.h"
#include "game/commander/commander_targeting.h"
#include "game/core/player_feedback.h"

namespace Game::Commander {

class CommanderRuntime {
public:
  void reset();
  void release_all_input();
  void release_for_rally();

  [[nodiscard]] auto advance(Engine::Core::World& world,
                             Engine::Core::EntityID commander_id,
                             int owner,
                             const Game::Command::CommanderInputFrame& frame,
                             float dt) -> bool;

  void set_latency_probe(CommanderLatencyProbe* probe) { m_latency_probe = probe; }
  void set_feedback_bus(Game::Core::PlayerFeedbackBus* bus) { m_feedback = bus; }
  void set_view(float yaw, float pitch);
  [[nodiscard]] auto view_yaw() const -> float { return m_view_yaw; }
  [[nodiscard]] auto view_pitch() const -> float { return m_view_pitch; }
  [[nodiscard]] auto locomotion() const -> const CommanderLocomotion& {
    return m_locomotion;
  }
  [[nodiscard]] auto targeting() const -> const CommanderTargeting& {
    return m_targeting;
  }
  [[nodiscard]] auto take_impact_kick() -> float;
  [[nodiscard]] auto input_edges() const -> const CommanderInputTrace& {
    return m_input_port.edges();
  }

  void set_trace_enabled(bool enabled);
  [[nodiscard]] auto trace() const -> const CommanderPresentationTrace& {
    return m_trace;
  }
  [[nodiscard]] auto capture_checkpoint() const -> QJsonObject;
  auto restore_checkpoint(const QJsonObject& state) -> bool;

  [[nodiscard]] static auto controlled_commander(Engine::Core::World& world,
                                                 Engine::Core::EntityID commander_id,
                                                 int owner) -> Engine::Core::Entity*;
  [[nodiscard]] auto find_primary_target(Engine::Core::World& world,
                                         Engine::Core::EntityID commander_id,
                                         int owner,
                                         float extra_reach) -> Engine::Core::EntityID;

  void toggle_close_camera_mode(Engine::Core::World& world,
                                Engine::Core::EntityID commander_id,
                                int owner) const;
  void toggle_weapon_stance(Engine::Core::World& world,
                            Engine::Core::EntityID commander_id,
                            int owner);
  void cycle_lock_on_target(Engine::Core::World& world,
                            Engine::Core::EntityID commander_id,
                            int owner);
  void toggle_lock_on_target(Engine::Core::World& world,
                             Engine::Core::EntityID commander_id,
                             int owner);
  void switch_lock_on_target(Engine::Core::World& world,
                             Engine::Core::EntityID commander_id,
                             int owner,
                             int direction);
  void note_refusal(const char* action);
  [[nodiscard]] auto last_refusal() const -> const CommanderRefusal& {
    return m_abilities.last_refusal();
  }

private:
  template <typename Runtime>
  static auto checkpoint_fields(Runtime& runtime);

  struct TickFacts {
    const Engine::Core::RpgCommanderActionComponent* active_action{nullptr};
    bool attack_animation_active{false};
    bool drawing_bow{false};

    float stagger_remaining{0.0F};

    [[nodiscard]] auto staggered() const -> bool { return stagger_remaining > 0.0F; }

    [[nodiscard]] auto recovering_soon() const -> bool {
      return staggered() && stagger_remaining <= k_commander_movement_buffer_seconds;
    }
  };

  struct MotionOutcome {
    MotorReport report;
    QVector3D previous_position;
    bool jump_active{false};
    int forward_axis{0};
    int right_axis{0};
  };

  struct DirectControlSync {
    Engine::Core::RpgCommanderAimComponent* aim{nullptr};
    float traced_stamina{-1.0F};
  };

  struct QueueOutcome {
    Engine::Core::CombatIntentQueueComponent* intents{nullptr};
    float dodge_grace_remaining{0.0F};
  };

  void
  steer_view(Engine::Core::World& world, Engine::Core::Entity& commander, float dt);
  [[nodiscard]] static auto
  resolve_handles(Engine::Core::Entity& commander,
                  Engine::Core::TransformComponent& transform,
                  Engine::Core::UnitComponent& unit) -> CommanderHandles;
  [[nodiscard]] static auto
  read_tick_facts(const Engine::Core::World& world,
                  const Engine::Core::Entity& commander) -> TickFacts;

  [[nodiscard]] auto squared_up(const TickFacts& facts) const -> bool;
  [[nodiscard]] auto advance_jump_stage(const CommanderHandles& body,
                                        const TickFacts& facts,
                                        float dt) -> bool;
  [[nodiscard]] auto start_dodge_stage(Engine::Core::World& world,
                                       const CommanderHandles& body,
                                       const TickFacts& facts,
                                       const MotionOutcome& motion) -> MoveBasis;
  [[nodiscard]] auto advance_combat(Engine::Core::World& world,
                                    const CommanderHandles& body,
                                    const TickFacts& facts,
                                    const MotionOutcome& motion,
                                    Engine::Core::EntityID commander_id,
                                    int owner,
                                    float dt) -> bool;
  void queue_pressed_intents(const CommanderHandles& body,
                             Engine::Core::CombatIntentQueueComponent& intents,
                             const Engine::Core::RpgCommanderAimComponent* aim);
  [[nodiscard]] auto
  dispatch_front_intent(Engine::Core::World& world,
                        const CommanderHandles& body,
                        Engine::Core::CombatIntentQueueComponent& intents,
                        Engine::Core::EntityID commander_id,
                        int owner) -> bool;
  void advance_swing(const CommanderHandles& body, float dt);
  [[nodiscard]] auto advance_motion(Engine::Core::World& world,
                                    const CommanderHandles& body,
                                    const TickFacts& facts,
                                    float dt) -> MotionOutcome;
  void face_body(const CommanderHandles& body,
                 const TickFacts& facts,
                 const MotionOutcome& motion,
                 float dt);
  [[nodiscard]] auto
  sync_direct_control(const CommanderHandles& body,
                      const MotionOutcome& motion) -> DirectControlSync;
  void activate_abilities(Engine::Core::World& world,
                          const CommanderHandles& body,
                          Engine::Core::EntityID commander_id,
                          int owner);
  [[nodiscard]] auto advance_strike(Engine::Core::World& world,
                                    const CommanderHandles& body,
                                    const TickFacts& facts,
                                    Engine::Core::RpgCommanderAimComponent* aim,
                                    Engine::Core::EntityID commander_id,
                                    int owner,
                                    float dt) -> std::optional<QueueOutcome>;
  void advance_targeting(Engine::Core::World& world,
                         const CommanderHandles& body,
                         Engine::Core::RpgCommanderAimComponent* aim,
                         Engine::Core::EntityID commander_id,
                         int owner,
                         float dt);
  void record_trace(const CommanderHandles& body,
                    const TickFacts& facts,
                    const MotionOutcome& motion,
                    const QueueOutcome& queue,
                    float traced_stamina,
                    float dt);
  void follow_body_while_rallying(float body_yaw, float dt);

  [[nodiscard]] auto hold_for_rally(const CommanderHandles& body, float dt) -> bool;
  void set_view_yaw(float yaw);
  void set_view_pitch(float pitch);

  CommanderLatencyProbe* m_latency_probe = nullptr;
  Game::Core::PlayerFeedbackBus* m_feedback = nullptr;
  float m_view_yaw = 0.0F;
  float m_view_pitch = 0.0F;
  float m_previous_yaw = 0.0F;
  float m_previous_pitch = 0.0F;
  float m_camera_fov = 68.0F;
  float m_impact_kick = 0.0F;
  float m_rally_view_yaw = 0.0F;
  float m_rally_look_idle = 0.0F;
  bool m_rally_view_valid = false;
  bool m_cancel_held_draw = false;

  CommanderInputPort m_input_port;
  std::optional<std::pair<QVector3D, QVector3D>> m_tick_aim_ray;
  CommanderMotor m_motor;
  CommanderLocomotion m_locomotion;
  CommanderBodyFacing m_body_facing;
  CommanderLunge m_lunge;
  CommanderStrike m_strike;
  CommanderTargeting m_targeting;
  CommanderDefence m_defence;
  CommanderAbilities m_abilities;
  bool m_trace_enabled = false;
  CommanderPresentationTrace m_trace;
};
} // namespace Game::Commander
