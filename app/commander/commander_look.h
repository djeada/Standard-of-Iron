#pragma once

#include <QPoint>
#include <QVector3D>

#include "app/commander/commander_frame_intent.h"
#include "app/commander/commander_input_port.h"
#include "app/commander/commander_latency_probe.h"

class QQuickWindow;

namespace App::Core {

inline constexpr float k_commander_view_pitch_limit_degrees = 70.0F;

class CommanderLook {
public:
  [[nodiscard]] auto yaw() const -> float { return m_view_yaw; }
  [[nodiscard]] auto pitch() const -> float { return m_view_pitch; }
  [[nodiscard]] auto previous_yaw() const -> float { return m_previous_view_yaw; }
  [[nodiscard]] auto previous_pitch() const -> float { return m_previous_view_pitch; }
  [[nodiscard]] auto frame_intent() const -> const CommanderFrameIntent& {
    return m_frame_intent;
  }

  void set_latency_probe(CommanderLatencyProbe* probe) { m_latency_probe = probe; }
  void set_yaw(float yaw) { m_view_yaw = yaw; }
  void set_pitch(float pitch);
  void remember_previous_view();

  void apply_mouse_delta(double dx, double dy, float sensitivity_scale);
  void mouse_look_at(double sx,
                     double sy,
                     double center_sx,
                     double center_sy,
                     QQuickWindow* window,
                     float sensitivity_scale);
  void center_mouse(double center_sx, double center_sy, QQuickWindow* window);
  void poll_mouse(QQuickWindow* window, float sensitivity_scale);

  auto sample_frame_intent(const CommanderHeldInput& held) -> CommanderFrameIntent;

  void release_mouse();
  void reset();

private:
  CommanderLatencyProbe* m_latency_probe = nullptr;
  float m_view_yaw = 0.0F;
  float m_view_pitch = 0.0F;
  float m_previous_view_yaw = 0.0F;
  float m_previous_view_pitch = 0.0F;

  CommanderFrameIntent m_frame_intent;
  float m_intent_sample_yaw = 0.0F;
  float m_intent_sample_pitch = 0.0F;
  bool m_intent_sample_valid = false;

  QPoint m_mouse_center;
  bool m_mouse_center_valid = false;
  QPoint m_last_mouse_global;
  bool m_last_mouse_valid = false;
  bool m_mouse_warp_supported = false;
  bool m_mouse_recentering = false;
};

} // namespace App::Core
