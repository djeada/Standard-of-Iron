#pragma once

#include <QPoint>
#include <QVector3D>
#include <Qt>

#include <cstdint>
#include <optional>
#include <utility>

#include "app/commander/commander_abilities.h"
#include "app/commander/commander_body_facing.h"
#include "app/commander/commander_camera_rig.h"
#include "app/commander/commander_defence.h"
#include "app/commander/commander_frame_intent.h"
#include "app/commander/commander_input_port.h"
#include "app/commander/commander_input_snapshot.h"
#include "app/commander/commander_latency_probe.h"
#include "app/commander/commander_locomotion.h"
#include "app/commander/commander_look.h"
#include "app/commander/commander_lunge.h"
#include "app/commander/commander_motor.h"
#include "app/commander/commander_presentation.h"
#include "app/commander/commander_presentation_trace.h"
#include "app/commander/commander_strike.h"
#include "app/commander/commander_targeting.h"
#include "app/core/player_feedback.h"
#include "game/core/component.h"

class QQuickWindow;

namespace Engine::Core {
class Entity;
class CommanderComponent;
class World;
class TransformComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

inline constexpr float k_commander_rest_view_pitch_degrees = -6.0F;

class CommanderControlController {
public:
  using InputState = App::Core::CommanderHeldInput;

  // Full reset for entering or leaving direct control: view, camera and
  // presentation start over.
  void reset();
  // A rally hands the body to RTS movement and back while the commander view
  // stays on screen: input and action state are dropped, but the camera rig,
  // look and presented pose carry on, so neither handoff cuts the camera.
  void release_for_rally();
  void release_all_input();
  void set_view_yaw(float yaw);
  void set_view_pitch(float pitch);
  [[nodiscard]] float view_yaw() const;
  [[nodiscard]] float view_pitch() const;
  [[nodiscard]] InputState& input();
  [[nodiscard]] const InputState& input() const;

  void key_down(int key);
  [[nodiscard]] auto queued_intent_count(Engine::Core::World& world,
                                         Engine::Core::EntityID commander_id,
                                         int local_owner_id) const -> int;
  void key_up(int key);
  void primary_action_down();
  void primary_action_up();
  void request_heavy_action();
  void secondary_action_down();
  void secondary_action_up();
  void mouse_move(qreal dx, qreal dy);
  void mouse_look_at(
      qreal sx, qreal sy, qreal center_sx, qreal center_sy, QQuickWindow* window);
  void center_mouse(qreal center_sx, qreal center_sy, QQuickWindow* window);
  void poll_mouse_look(QQuickWindow* window);
  void set_latency_probe(App::Core::CommanderLatencyProbe* probe);
  void set_feedback_bus(App::Core::PlayerFeedbackBus* bus) { m_feedback = bus; }
  auto sample_frame_intent(QQuickWindow* window) -> CommanderFrameIntent;
  [[nodiscard]] auto frame_intent() const -> const CommanderFrameIntent& {
    return m_look.frame_intent();
  }
  void request_dodge();
  void request_dodge(const QVector3D& world_direction);
  void request_jump();
  void special_action();
  void request_vanguard_rush();
  void request_second_wind();
  void toggle_close_camera_mode(Engine::Core::World& world,
                                Engine::Core::EntityID commander_id,
                                int local_owner_id) const;
  void toggle_weapon_stance(Engine::Core::World& world,
                            Engine::Core::EntityID commander_id,
                            int local_owner_id);
  void cycle_lock_on_target(Engine::Core::World& world,
                            Engine::Core::EntityID commander_id,
                            int local_owner_id);
  void note_refusal(const char* action);
  [[nodiscard]] auto last_refusal() const -> const App::Core::CommanderRefusal& {
    return m_abilities.last_refusal();
  }
  [[nodiscard]] Engine::Core::EntityID locked_target_id() const;
  [[nodiscard]] Engine::Core::EntityID focus_target_id() const;
  [[nodiscard]] bool is_dodge_rolling() const {
    return m_locomotion.dodge_state() == DodgeState::Rolling;
  }
  [[nodiscard]] Engine::Core::Entity*
  controlled_commander(Engine::Core::World& world,
                       Engine::Core::EntityID commander_id,
                       int local_owner_id) const;

  [[nodiscard]] Engine::Core::EntityID
  find_primary_target(Engine::Core::World& world,
                      Engine::Core::EntityID commander_id,
                      int local_owner_id,
                      float extra_reach = 0.0F);
  [[nodiscard]] bool update(Engine::Core::World& world,
                            Engine::Core::EntityID commander_id,
                            int local_owner_id,
                            Render::GL::Camera& camera,
                            float dt);
  [[nodiscard]] bool update_simulation(Engine::Core::World& world,
                                       Engine::Core::EntityID commander_id,
                                       int local_owner_id,
                                       float dt);
  void update_camera_presentation(Engine::Core::World& world,
                                  Engine::Core::EntityID commander_id,
                                  Render::GL::Camera& camera,
                                  float dt);
  void snap_presentation_pose();
  [[nodiscard]] auto
  presentation_pose() const -> const Engine::Core::PresentationPose& {
    return m_presentation.pose();
  }

  void set_presentation_trace_enabled(bool enabled) {
    m_trace_enabled = enabled;
    if (!enabled) {
      m_trace = {};
    }
  }
  [[nodiscard]] auto presentation_trace_enabled() const -> bool {
    return m_trace_enabled;
  }
  [[nodiscard]] auto
  presentation_trace() const -> const App::Core::CommanderPresentationTrace& {
    return m_trace;
  }
  [[nodiscard]] auto input_edges() const -> const App::Core::CommanderInputTrace& {
    return m_input_port.edges();
  }
  [[nodiscard]] auto camera_trace() const -> const App::Core::CommanderCameraTrace& {
    return m_camera_rig.trace();
  }

private:
  struct TickFacts {
    const Engine::Core::RpgCommanderActionComponent* active_action{nullptr};
    bool attack_animation_active{false};
    bool drawing_bow{false};
    // Seconds of hit stagger left. While it runs the player cannot move, roll,
    // jump or guard; a hit has to cost more than health.
    float stagger_remaining{0.0F};

    [[nodiscard]] auto staggered() const -> bool { return stagger_remaining > 0.0F; }
    // A roll or jump pressed this late in a stagger is held for the recovery.
    [[nodiscard]] auto recovering_soon() const -> bool {
      return staggered() && stagger_remaining <= k_commander_movement_buffer_seconds;
    }
  };

  struct MotionOutcome {
    App::Core::MotorReport report;
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

  void capture_input(Engine::Core::World& world,
                     Engine::Core::EntityID commander_id,
                     float dt);
  [[nodiscard]] auto hold_for_rally(Engine::Core::World& world,
                                    const App::Core::CommanderHandles& body,
                                    Render::GL::Camera* camera,
                                    float dt) -> bool;
  void
  steer_view(Engine::Core::World& world, Engine::Core::Entity& commander, float dt);
  [[nodiscard]] static auto
  resolve_handles(Engine::Core::Entity& commander,
                  Engine::Core::TransformComponent& transform,
                  Engine::Core::UnitComponent& unit) -> App::Core::CommanderHandles;
  [[nodiscard]] static auto
  read_tick_facts(const Engine::Core::World& world,
                  const Engine::Core::Entity& commander) -> TickFacts;
  // True while the body faces the view for a fight rather than its own travel.
  [[nodiscard]] auto squared_up(const TickFacts& facts) const -> bool;
  [[nodiscard]] auto advance_jump_stage(const App::Core::CommanderHandles& body,
                                        const TickFacts& facts,
                                        float dt) -> bool;
  [[nodiscard]] auto
  start_dodge_stage(Engine::Core::World& world,
                    const App::Core::CommanderHandles& body,
                    const TickFacts& facts,
                    const MotionOutcome& motion) -> App::Core::MoveBasis;
  void present_tick(Engine::Core::World& world,
                    const App::Core::CommanderHandles& body,
                    Render::GL::Camera* camera,
                    float dt);
  [[nodiscard]] auto advance_combat(Engine::Core::World& world,
                                    const App::Core::CommanderHandles& body,
                                    const TickFacts& facts,
                                    const MotionOutcome& motion,
                                    Engine::Core::EntityID commander_id,
                                    int local_owner_id,
                                    float dt) -> bool;
  void queue_pressed_intents(const App::Core::CommanderHandles& body,
                             Engine::Core::CombatIntentQueueComponent& intents,
                             const Engine::Core::RpgCommanderAimComponent* aim);
  [[nodiscard]] auto
  dispatch_front_intent(Engine::Core::World& world,
                        const App::Core::CommanderHandles& body,
                        Engine::Core::CombatIntentQueueComponent& intents,
                        Engine::Core::EntityID commander_id,
                        int local_owner_id) -> bool;
  void advance_swing(const App::Core::CommanderHandles& body, float dt);
  [[nodiscard]] auto advance_motion(Engine::Core::World& world,
                                    const App::Core::CommanderHandles& body,
                                    const TickFacts& facts,
                                    float dt) -> MotionOutcome;
  void face_body(const App::Core::CommanderHandles& body,
                 const TickFacts& facts,
                 const MotionOutcome& motion,
                 float dt);
  [[nodiscard]] auto
  sync_direct_control(const App::Core::CommanderHandles& body,
                      const MotionOutcome& motion) -> DirectControlSync;
  void activate_abilities(Engine::Core::World& world,
                          const App::Core::CommanderHandles& body,
                          Engine::Core::EntityID commander_id,
                          int local_owner_id);
  [[nodiscard]] auto advance_strike(Engine::Core::World& world,
                                    const App::Core::CommanderHandles& body,
                                    const TickFacts& facts,
                                    Engine::Core::RpgCommanderAimComponent* aim,
                                    Engine::Core::EntityID commander_id,
                                    int local_owner_id,
                                    float dt) -> std::optional<QueueOutcome>;
  void advance_targeting(Engine::Core::World& world,
                         const App::Core::CommanderHandles& body,
                         Engine::Core::RpgCommanderAimComponent* aim,
                         Engine::Core::EntityID commander_id,
                         int local_owner_id,
                         float dt);
  void record_trace(const App::Core::CommanderHandles& body,
                    const TickFacts& facts,
                    const MotionOutcome& motion,
                    const QueueOutcome& queue,
                    float traced_stamina,
                    float dt);
  [[nodiscard]] bool update_impl(Engine::Core::World& world,
                                 Engine::Core::EntityID commander_id,
                                 int local_owner_id,
                                 Render::GL::Camera* camera,
                                 float dt);
  void update_camera(Engine::Core::World& world,
                     Engine::Core::Entity& commander,
                     Render::GL::Camera& camera,
                     float dt);
  [[nodiscard]] auto look_sensitivity_scale() const -> float;

  App::Core::CommanderLatencyProbe* m_latency_probe = nullptr;
  App::Core::PlayerFeedbackBus* m_feedback = nullptr;

  App::Core::CommanderInputPort m_input_port;
  App::Core::CommanderLook m_look;
  App::Core::CommanderCameraRig m_camera_rig;
  std::optional<std::pair<QVector3D, QVector3D>> m_tick_aim_ray;
  App::Core::CommanderMotor m_motor;
  App::Core::CommanderLocomotion m_locomotion;
  App::Core::CommanderBodyFacing m_body_facing;
  App::Core::CommanderLunge m_lunge;
  App::Core::CommanderStrike m_strike;
  App::Core::CommanderTargeting m_targeting;
  App::Core::CommanderDefence m_defence;
  App::Core::CommanderPresentation m_presentation;
  App::Core::CommanderAbilities m_abilities;

  bool m_trace_enabled = false;
  App::Core::CommanderPresentationTrace m_trace;
};
