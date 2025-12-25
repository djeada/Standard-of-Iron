#pragma once

namespace Game {

struct CameraConfig {
  float default_distance = 12.0F;
  float default_pitch = 45.0F;
  float default_yaw = 225.0F;
  float orbit_step_normal = 4.0F;
  float orbit_step_shift = 8.0F;
};

struct ArrowConfig {
  float arc_height_multiplier = 0.15F;
  float arc_height_min = 0.2F;
  float arc_height_max = 1.2F;
  float speed_default = 8.0F;
  float speed_attack = 6.0F;
};

struct GameplayConfig {
  float visibility_update_interval = 0.075F;
  float formation_spacing_default = 1.0F;
  int max_troops_per_player = 500;
};

class GameConfig {
public:
  static auto instance() noexcept -> GameConfig & {
    static GameConfig inst;
    return inst;
  }

  [[nodiscard]] auto camera() const noexcept -> const CameraConfig & {
    return m_camera;
  }
  [[nodiscard]] auto arrow() const noexcept -> const ArrowConfig & {
    return m_arrow;
  }
  [[nodiscard]] auto gameplay() const noexcept -> const GameplayConfig & {
    return m_gameplay;
  }

  [[nodiscard]] auto get_camera_default_distance() const noexcept -> float {
    return m_camera.default_distance;
  }
  [[nodiscard]] auto get_camera_default_pitch() const noexcept -> float {
    return m_camera.default_pitch;
  }
  [[nodiscard]] auto get_camera_default_yaw() const noexcept -> float {
    return m_camera.default_yaw;
  }

  [[nodiscard]] auto get_arrow_arc_height_multiplier() const noexcept -> float {
    return m_arrow.arc_height_multiplier;
  }
  [[nodiscard]] auto get_arrow_arc_height_min() const noexcept -> float {
    return m_arrow.arc_height_min;
  }
  [[nodiscard]] auto get_arrow_arc_height_max() const noexcept -> float {
    return m_arrow.arc_height_max;
  }

  [[nodiscard]] auto get_arrow_speed_default() const noexcept -> float {
    return m_arrow.speed_default;
  }
  [[nodiscard]] auto get_arrow_speed_attack() const noexcept -> float {
    return m_arrow.speed_attack;
  }

  [[nodiscard]] auto get_visibility_update_interval() const noexcept -> float {
    return m_gameplay.visibility_update_interval;
  }

  [[nodiscard]] auto get_formation_spacing_default() const noexcept -> float {
    return m_gameplay.formation_spacing_default;
  }

  [[nodiscard]] auto get_camera_orbit_step_normal() const noexcept -> float {
    return m_camera.orbit_step_normal;
  }
  [[nodiscard]] auto get_camera_orbit_step_shift() const noexcept -> float {
    return m_camera.orbit_step_shift;
  }

  void set_camera_default_distance(float value) noexcept {
    m_camera.default_distance = value;
  }
  void set_camera_default_pitch(float value) noexcept {
    m_camera.default_pitch = value;
  }
  void set_camera_default_yaw(float value) noexcept {
    m_camera.default_yaw = value;
  }

  void set_arrow_arc_height_multiplier(float value) noexcept {
    m_arrow.arc_height_multiplier = value;
  }
  void set_arrow_arc_height_min(float value) noexcept {
    m_arrow.arc_height_min = value;
  }
  void set_arrow_arc_height_max(float value) noexcept {
    m_arrow.arc_height_max = value;
  }

  void set_arrow_speed_default(float value) noexcept {
    m_arrow.speed_default = value;
  }
  void set_arrow_speed_attack(float value) noexcept {
    m_arrow.speed_attack = value;
  }

  void set_visibility_update_interval(float value) noexcept {
    m_gameplay.visibility_update_interval = value;
  }

  void set_formation_spacing_default(float value) noexcept {
    m_gameplay.formation_spacing_default = value;
  }

  void set_camera_orbit_step_normal(float value) noexcept {
    m_camera.orbit_step_normal = value;
  }
  void set_camera_orbit_step_shift(float value) noexcept {
    m_camera.orbit_step_shift = value;
  }

  [[nodiscard]] auto get_max_troops_per_player() const noexcept -> int {
    return m_gameplay.max_troops_per_player;
  }

  void set_max_troops_per_player(int value) noexcept {
    m_gameplay.max_troops_per_player = value;
  }

private:
  GameConfig() = default;

  CameraConfig m_camera;
  ArrowConfig m_arrow;
  GameplayConfig m_gameplay;
};

} 
