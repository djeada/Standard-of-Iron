#pragma once

#include <QPoint>
#include <QVector3D>
#include <Qt>

class QQuickWindow;

namespace Engine::Core {
class Entity;
class CommanderComponent;
class World;
using EntityID = unsigned int;
} // namespace Engine::Core

namespace Render::GL {
class Camera;
}

enum class DodgeState {
  None,
  Rolling,
  Recovering
};

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
    float primary_action_scan_cooldown = 0.0F;
    bool dodge_requested = false;
    bool jump_requested = false;
    bool shield_bash_requested = false;
    bool vanguard_rush_requested = false;
    bool second_wind_requested = false;
  };

  void reset();
  void set_view_yaw(float yaw);
  void set_view_pitch(float pitch);
  [[nodiscard]] float view_yaw() const;
  [[nodiscard]] float view_pitch() const;
  [[nodiscard]] InputState& input();
  [[nodiscard]] const InputState& input() const;

  void key_down(int key);
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
  void request_dodge();
  void request_jump();
  void special_action();
  void request_vanguard_rush();
  void request_second_wind();
  void toggle_close_camera_mode(Engine::Core::World& world,
                                Engine::Core::EntityID commander_id,
                                int local_owner_id) const;
  void cycle_lock_on_target(Engine::Core::World& world,
                            Engine::Core::EntityID commander_id,
                            int local_owner_id);
  [[nodiscard]] Engine::Core::EntityID locked_target_id() const;
  [[nodiscard]] Engine::Core::EntityID focus_target_id() const;
  [[nodiscard]] bool is_dodge_rolling() const { return m_dodge_state == DodgeState::Rolling; }
  [[nodiscard]] Engine::Core::Entity*
  controlled_commander(Engine::Core::World& world,
                       Engine::Core::EntityID commander_id,
                       int local_owner_id) const;
  [[nodiscard]] Engine::Core::EntityID
  find_primary_target(Engine::Core::World& world,
                      Engine::Core::EntityID commander_id,
                      int local_owner_id);
  [[nodiscard]] bool primary_action(Engine::Core::World& world,
                                    Engine::Core::EntityID commander_id,
                                    int local_owner_id);
  void release_guard(Engine::Core::World& world,
                     Engine::Core::EntityID commander_id,
                     int local_owner_id);
  [[nodiscard]] bool update(Engine::Core::World& world,
                            Engine::Core::EntityID commander_id,
                            int local_owner_id,
                            Render::GL::Camera& camera,
                            float dt);

private:
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
  void update_camera(Engine::Core::World& world,
                     Engine::Core::Entity& commander,
                     Render::GL::Camera& camera,
                     float dt);
  void update_lock_on_yaw(Engine::Core::World& world,
                          Engine::Core::Entity& commander,
                          float dt);
  InputState m_input;
  float m_view_yaw = 0.0F;
  float m_view_pitch = 0.0F;
  QPoint m_mouse_center;
  bool m_mouse_center_valid = false;
  QPoint m_last_mouse_global;
  bool m_last_mouse_valid = false;
  bool m_mouse_warp_supported = false;
  bool m_mouse_recentering = false;

  float m_bob_phase = 0.0F;
  float m_bob_amplitude = 0.0F;
  float m_breath_phase = 0.0F;
  float m_strafe_lean = 0.0F;
  float m_fov_current = 75.0F;
  QVector3D m_cam_eye_smooth{};
  QVector3D m_cam_target_smooth{};
  bool m_cam_smooth_valid = false;

  float m_move_speed = 0.0F;
  int m_move_right_axis = 0;
  int m_move_forward_axis = 0;
  bool m_move_running = false;

  float m_hit_trauma = 0.0F;
  float m_hit_shake_phase = 0.0F;
  float m_strike_punch_fwd = 0.0F;

  DodgeState m_dodge_state = DodgeState::None;
  float m_dodge_timer = 0.0F;
  QVector3D m_dodge_direction{0.0F, 0.0F, 1.0F};
  float m_dodge_fov_kick = 0.0F;
  float m_jump_timer = 0.0F;
  bool m_jump_safe_position_valid = false;
  QVector3D m_jump_last_walkable_position{0.0F, 0.0F, 0.0F};

  float m_combo_miss_timer = 0.0F;
  float m_primary_held_duration = 0.0F;
  float m_shield_bash_cooldown = 0.0F;
  float m_vanguard_rush_cooldown = 0.0F;
  float m_second_wind_cooldown = 0.0F;

  Engine::Core::EntityID m_locked_target_id = 0;
  Engine::Core::EntityID m_soft_target_id = 0;
  float m_lock_lost_timer = 0.0F;
  bool m_guard_was_active = false;
};
