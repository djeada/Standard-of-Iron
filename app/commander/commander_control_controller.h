#pragma once

#include <QPoint>
#include <QVector3D>
#include <Qt>

#include <cstdint>
#include <limits>
#include <mutex>

#include "app/commander/commander_camera_rig.h"
#include "app/commander/commander_frame_intent.h"
#include "app/commander/commander_input_snapshot.h"
#include "app/commander/commander_latency_probe.h"
#include "app/commander/commander_motor.h"
#include "app/commander/commander_presentation_trace.h"
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

enum class DodgeState {
  None,
  Rolling,
  Recovering
};

inline constexpr float k_commander_rest_view_pitch_degrees = -6.0F;

class CommanderControlController {
public:
  struct InputState {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool turn_left = false;
    bool turn_right = false;
    bool run = false;
    bool primary_action = false;
    bool secondary_action = false;
    bool dodge_requested = false;
    bool jump_requested = false;
    bool shield_bash_requested = false;
    bool vanguard_rush_requested = false;
    bool second_wind_requested = false;
  };

  void reset();
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
  void secondary_action_down();
  void secondary_action_up();
  void mouse_move(qreal dx, qreal dy);
  void mouse_look_at(
      qreal sx, qreal sy, qreal center_sx, qreal center_sy, QQuickWindow* window);
  void center_mouse(qreal center_sx, qreal center_sy, QQuickWindow* window);
  void poll_mouse_look(QQuickWindow* window);
  void set_latency_probe(App::Core::CommanderLatencyProbe* probe) {
    m_latency_probe = probe;
  }
  void set_feedback_bus(App::Core::PlayerFeedbackBus* bus) { m_feedback = bus; }
  auto sample_frame_intent(QQuickWindow* window) -> CommanderFrameIntent;
  [[nodiscard]] auto frame_intent() const -> const CommanderFrameIntent& {
    return m_frame_intent;
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
                            int local_owner_id) const;
  void cycle_lock_on_target(Engine::Core::World& world,
                            Engine::Core::EntityID commander_id,
                            int local_owner_id);
  [[nodiscard]] Engine::Core::EntityID locked_target_id() const;
  [[nodiscard]] Engine::Core::EntityID focus_target_id() const;
  [[nodiscard]] bool is_dodge_rolling() const {
    return m_dodge_state == DodgeState::Rolling;
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
    return m_presentation_pose;
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
    return m_edges;
  }
  [[nodiscard]] auto camera_trace() const -> const App::Core::CommanderCameraTrace& {
    return m_camera_rig.trace();
  }

private:
  [[nodiscard]] auto take_input_snapshot() -> CommanderInputSnapshot;
  void discard_input_edges(CommanderInputSnapshot& snapshot);
  void publish_presentation_sample(Engine::Core::Entity& commander,
                                   const Engine::Core::TransformComponent& transform,
                                   float dt);
  [[nodiscard]] auto
  advance_presentation_pose(Engine::Core::Entity& commander,
                            const Engine::Core::TransformComponent& transform,
                            float dt) -> Engine::Core::PresentationPose;
  [[nodiscard]] bool primary_action(Engine::Core::World& world,
                                    Engine::Core::EntityID commander_id,
                                    int local_owner_id);
  [[nodiscard]] bool update_impl(Engine::Core::World& world,
                                 Engine::Core::EntityID commander_id,
                                 int local_owner_id,
                                 Render::GL::Camera* camera,
                                 float dt);
  [[nodiscard]] Engine::Core::EntityID
  resolve_ability_target(Engine::Core::World& world,
                         Engine::Core::Entity& commander,
                         int local_owner_id,
                         float max_range) const;
  void update_ability_cooldowns(Engine::Core::CommanderComponent* commander, float dt);
  void try_activate_shield_bash(Engine::Core::World& world,
                                Engine::Core::Entity& commander,
                                Engine::Core::EntityID commander_id,
                                int local_owner_id);
  void try_activate_vanguard_rush(Engine::Core::World& world,
                                  Engine::Core::Entity& commander,
                                  Engine::Core::EntityID commander_id,
                                  int local_owner_id);
  void try_activate_second_wind(Engine::Core::Entity& commander);
  void
  publish_resolved_defense_feedback(Engine::Core::Entity& commander,
                                    Engine::Core::EntityID commander_id,
                                    const Engine::Core::TransformComponent& transform);
  void update_camera(Engine::Core::World& world,
                     Engine::Core::Entity& commander,
                     Render::GL::Camera& camera,
                     float dt);
  void play_footstep_if_stride_landed(const Engine::Core::Entity& commander,
                                      float previous_bob_phase);
  [[nodiscard]] auto look_sensitivity_scale() const -> float;
  void update_lock_on_yaw(Engine::Core::World& world,
                          Engine::Core::Entity& commander,
                          float dt);
  void apply_strike_lunge(Engine::Core::World& world,
                          Engine::Core::Entity& commander,
                          Engine::Core::TransformComponent& transform,
                          float dt);
  App::Core::CommanderLatencyProbe* m_latency_probe = nullptr;
  App::Core::PlayerFeedbackBus* m_feedback = nullptr;
  InputState m_input;
  CommanderFrameIntent m_frame_intent;
  float m_intent_sample_yaw = 0.0F;
  float m_intent_sample_pitch = 0.0F;
  bool m_intent_sample_valid = false;
  float m_view_yaw = 0.0F;
  float m_view_pitch = 0.0F;

  float m_previous_view_yaw = 0.0F;
  float m_previous_view_pitch = 0.0F;
  QPoint m_mouse_center;
  bool m_mouse_center_valid = false;
  QPoint m_last_mouse_global;
  bool m_last_mouse_valid = false;
  bool m_mouse_warp_supported = false;
  bool m_mouse_recentering = false;

  App::Core::CommanderCameraRig m_camera_rig;
  App::Core::CommanderMotor m_motor;

  float m_move_speed = 0.0F;
  float m_planar_speed_smooth = 0.0F;
  QVector3D m_last_move_direction{0.0F, 0.0F, 1.0F};
  int m_move_right_axis = 0;
  int m_move_forward_axis = 0;
  bool m_move_running = false;

  DodgeState m_dodge_state = DodgeState::None;
  float m_dodge_timer = 0.0F;
  QVector3D m_dodge_direction{0.0F, 0.0F, 1.0F};
  QVector3D m_requested_dodge_direction{0.0F, 0.0F, 0.0F};
  bool m_has_requested_dodge_direction = false;
  float m_dodge_fov_kick = 0.0F;

  std::uint32_t m_observed_hit_confirm_sequence = 0;
  std::uint32_t m_observed_blocked_contacts = 0;
  std::uint32_t m_observed_perfect_guard_contacts = 0;
  std::uint32_t m_observed_dodged_contacts = 0;
  std::uint32_t m_observed_guard_broken_contacts = 0;
  std::uint8_t m_observed_action_hit_count = 0;
  float m_jump_timer = 0.0F;
  bool m_jump_safe_position_valid = false;
  QVector3D m_jump_last_walkable_position{0.0F, 0.0F, 0.0F};

  float m_combo_miss_timer = 0.0F;
  float m_primary_held_duration = 0.0F;
  bool m_primary_press_pending = false;
  float m_primary_scan_cooldown = 0.0F;
  bool m_carried_primary_press = false;
  float m_body_yaw = 0.0F;
  bool m_body_yaw_valid = false;
  bool m_turning_in_place = false;
  Engine::Core::PresentationPose m_presentation_pose;
  bool m_presentation_snap_requested = true;
  std::uint32_t m_presentation_seen_sequence = 0;
  float m_presentation_age = 0.0F;
  CommanderInputSnapshot m_tick_input;
  std::uint64_t m_input_snapshot_sequence = 0;
  mutable std::mutex m_input_mutex;
  float m_shield_bash_cooldown = 0.0F;
  float m_vanguard_rush_cooldown = 0.0F;
  float m_second_wind_cooldown = 0.0F;

  Engine::Core::EntityID m_locked_target_id = 0;
  std::uint16_t m_locked_target_slot{std::numeric_limits<std::uint16_t>::max()};
  Engine::Core::EntityID m_soft_target_id = 0;
  std::uint16_t m_soft_target_slot{std::numeric_limits<std::uint16_t>::max()};
  std::uint16_t m_primary_target_slot{std::numeric_limits<std::uint16_t>::max()};
  float m_lock_lost_timer = 0.0F;
  float m_lock_spring_yaw = 0.0F;
  bool m_lock_spring_yaw_valid = false;
  float m_lock_manual_override_timer = 0.0F;
  bool m_guard_was_active = false;

  bool m_trace_enabled = false;
  App::Core::CommanderPresentationTrace m_trace;
  App::Core::CommanderInputTrace m_edges;
};
