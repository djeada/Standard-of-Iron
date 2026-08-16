#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>

namespace Engine::Core {
enum class FightContext : std::uint8_t;
}

namespace Render::GL {
class Camera;
}

namespace App::Core {

enum class CommanderFramingState : std::uint8_t {
  Explore,
  Melee,
  DuelLock,
  BowAim
};

struct CommanderCameraInputs {
  float dt{0.0F};
  float view_yaw_degrees{0.0F};
  float view_pitch_degrees{0.0F};
  float move_speed{0.0F};
  int move_right_axis{0};
  bool move_running{false};
  bool aiming_bow{false};
  bool close_camera_mode{false};
  bool lock_target_active{false};
  float jump_height_offset{0.0F};
  float dodge_fov_kick{0.0F};
  bool dodge_rolling{false};
  float dodge_tilt_progress{0.0F};
  QVector3D dodge_direction{};
  QVector3D commander_position{};
  std::optional<QVector3D> lock_target_position;
  std::optional<QVector3D> soft_focus_position;
  Engine::Core::FightContext fight_context{};
  float threat_side_bias{0.0F};
};

class CommanderCameraRig {
public:
  void reset();

  auto update(Render::GL::Camera& camera, const CommanderCameraInputs& inputs) -> float;

  void add_impact_kick(float strength);

  [[nodiscard]] auto framing_state() const -> CommanderFramingState {
    return m_framing_state;
  }
  [[nodiscard]] auto eye() const -> const QVector3D& { return m_eye_smooth; }
  [[nodiscard]] auto eye_valid() const -> bool { return m_smooth_valid; }
  [[nodiscard]] auto forward() const -> const QVector3D& { return m_forward; }
  [[nodiscard]] auto forward_valid() const -> bool { return m_forward_valid; }
  [[nodiscard]] auto fov() const -> float { return m_fov_current; }
  [[nodiscard]] auto aim_blend() const -> float { return m_aim_blend; }
  [[nodiscard]] auto bob_phase() const -> float { return m_bob_phase; }
  [[nodiscard]] auto bob_amplitude() const -> float { return m_bob_amplitude; }

  static auto
  select_framing(bool aiming_bow,
                 bool lock_target_active,
                 Engine::Core::FightContext fight_context) -> CommanderFramingState;

private:
  struct Framing {
    float back{3.10F};
    float up{1.15F};
    float side{0.90F};
    float distance{6.0F};
    float fov{68.0F};

    float look_drop{0.0F};
  };

  static auto framing_for(CommanderFramingState state,
                          bool close_camera_mode) -> Framing;

  CommanderFramingState m_framing_state{CommanderFramingState::Explore};
  Framing m_framing_current{};
  bool m_framing_valid{false};

  float m_bob_phase{0.0F};
  float m_bob_amplitude{0.0F};
  float m_breath_phase{0.0F};
  float m_strafe_lean{0.0F};
  float m_fov_current{75.0F};
  float m_aim_blend{0.0F};
  float m_hit_impact_kick{0.0F};
  float m_threat_bias_smooth{0.0F};

  QVector3D m_eye_smooth{};
  QVector3D m_target_smooth{};
  bool m_smooth_valid{false};

  QVector3D m_forward{0.0F, 0.0F, 1.0F};
  bool m_forward_valid{false};

  float m_ground_y{0.0F};
  bool m_ground_valid{false};
};

} // namespace App::Core
