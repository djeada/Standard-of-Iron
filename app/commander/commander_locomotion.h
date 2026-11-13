#pragma once

#include <QVector3D>

#include <optional>

#include "app/commander/commander_entity_access.h"
#include "app/commander/commander_input_port.h"
#include "app/commander/commander_latency_probe.h"
#include "app/commander/commander_motor.h"
#include "app/commander/commander_presentation_trace.h"

enum class DodgeState {
  None,
  Rolling,
  Recovering
};

inline constexpr float k_commander_ground_acceleration_mps2 = 30.0F;
inline constexpr float k_commander_ground_deceleration_mps2 = 36.0F;
// Airborne steering. A jump commits to its takeoff momentum: releasing input
// keeps it, and steering can bend it by at most 3.8 m/s over a full jump.
inline constexpr float k_commander_air_acceleration_mps2 = 4.0F;
// A jump or dodge pressed this close to the end of a jump or roll is held and
// resolved on the first free tick. Earlier presses are still refused at once.
inline constexpr float k_commander_movement_buffer_seconds = 0.20F;

namespace Engine::Core {
class RpgCommanderActionComponent;
}

namespace App::Core {

struct MotorReport {
  CommanderDisplacementSource source{CommanderDisplacementSource::None};
  bool blocked{false};
  bool slid{false};
  float requested_speed{0.0F};
  float actual_speed{0.0F};
  bool running{false};
  float lunge_distance{0.0F};
  float snap_back_distance{0.0F};
  bool dodge_pose_active{false};
  float dodge_pose_phase{0.0F};
  QVector3D move{0.0F, 0.0F, 0.0F};
};

struct JumpTickInput {
  bool jump_pressed{false};
  bool primary_held{false};
  bool guard_held{false};
  bool ability_requested{false};
  // Reeling from a hit: the body is not his to command until it recovers.
  bool staggered{false};
  float dt{0.0F};
};

struct JumpTickResult {
  bool active{false};
  EdgeOutcome edge{EdgeOutcome::None};
};

struct DodgeStartInput {
  bool pressed{false};
  bool staggered{false};
  bool has_direction{false};
  QVector3D direction{0.0F, 0.0F, 0.0F};
  QVector3D move{0.0F, 0.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};
};

struct MotionTickInput {
  QVector3D move{0.0F, 0.0F, 0.0F};
  int forward_axis{0};
  int right_axis{0};
  bool run_held{false};
  bool drawing_bow{false};
  bool jump_active{false};
  // Facing the view for a fight. Only then is he stepping back or sideways;
  // moving freely he turns and runs, at full speed in every direction.
  bool squared_up{false};
  float dt{0.0F};
};

struct MoveBasis {
  QVector3D forward{0.0F, 0.0F, 1.0F};
  QVector3D move{0.0F, 0.0F, 0.0F};
};

class CommanderLocomotion {
public:
  [[nodiscard]] static auto
  move_basis(float view_yaw,
             int forward_axis,
             int right_axis,
             const Engine::Core::TransformComponent& transform,
             const std::optional<QVector3D>& lock_target_position) -> MoveBasis;

  [[nodiscard]] auto dodge_state() const -> DodgeState { return m_dodge_state; }
  [[nodiscard]] auto dodge_timer() const -> float { return m_dodge_timer; }
  [[nodiscard]] auto dodge_direction() const -> const QVector3D& {
    return m_dodge_direction;
  }
  [[nodiscard]] auto dodge_fov_kick() const -> float { return m_dodge_fov_kick; }
  [[nodiscard]] auto move_speed() const -> float { return m_move_speed; }
  [[nodiscard]] auto planar_speed() const -> float {
    return m_planar_velocity.length();
  }
  [[nodiscard]] auto move_right_axis() const -> int { return m_move_right_axis; }
  [[nodiscard]] auto move_forward_axis() const -> int { return m_move_forward_axis; }
  [[nodiscard]] auto move_running() const -> bool { return m_move_running; }
  [[nodiscard]] auto jump_airborne() const -> bool { return m_jump_timer > 0.0F; }
  [[nodiscard]] auto jump_followup_pending() const -> bool {
    return m_jump_followup_pending;
  }
  void clear_jump_followup() { m_jump_followup_pending = false; }
  // True while a jump or dodge is running and ends within `seconds`.
  [[nodiscard]] auto frees_within(float seconds) const -> bool;

  void decay_dodge_kick(float dt);

  [[nodiscard]] auto advance_jump(const CommanderHandles& body,
                                  const JumpTickInput& input) -> JumpTickResult;

  [[nodiscard]] auto try_start_dodge(const CommanderHandles& body,
                                     const DodgeStartInput& input,
                                     CommanderLatencyProbe* probe) -> EdgeOutcome;

  [[nodiscard]] auto advance_motion(const CommanderHandles& body,
                                    CommanderMotor& motor,
                                    const MotionTickInput& input) -> MotorReport;

  void
  commit_motion(const MotorReport& report, int forward_axis, int right_axis, float dt);

  void publish_fpv_motion(const CommanderHandles& body, float requested_speed) const;

  [[nodiscard]] auto engage_manual_motion(const CommanderHandles& body,
                                          CommanderLatencyProbe* probe) const -> float;

  void hold_for_rally();
  void release_input();
  void reset();

private:
  void resume_authored_jump(
      const CommanderHandles& body,
      const Engine::Core::RpgCommanderActionComponent* running_action,
      const Game::Systems::CombatActions::CombatActionDefinition* current_definition);
  void begin_jump(const CommanderHandles& body, bool followup);
  void tick_jump_timer(const CommanderHandles& body, float dt);
  void steer_airborne(const QVector3D& desired_velocity, float target_speed, float dt);

  void roll(const CommanderHandles& body,
            CommanderMotor& motor,
            float dt,
            MotorReport& report);
  void recover_roll(const CommanderHandles& body,
                    CommanderMotor& motor,
                    const MotionTickInput& input,
                    MotorReport& report);
  void
  accelerate_toward(const QVector3D& desired_velocity, float target_speed, float dt);
  void walk(const CommanderHandles& body,
            CommanderMotor& motor,
            const MotionTickInput& input,
            MotorReport& report);

  float m_move_speed = 0.0F;
  QVector3D m_planar_velocity{0.0F, 0.0F, 0.0F};
  float m_accepted_speed_smooth = 0.0F;
  int m_move_right_axis = 0;
  int m_move_forward_axis = 0;
  bool m_move_running = false;

  DodgeState m_dodge_state = DodgeState::None;
  float m_dodge_timer = 0.0F;
  QVector3D m_dodge_direction{0.0F, 0.0F, 1.0F};
  float m_dodge_fov_kick = 0.0F;

  float m_jump_timer = 0.0F;
  bool m_jump_followup_pending = false;
};

} // namespace App::Core
