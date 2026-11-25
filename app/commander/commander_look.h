#pragma once

#include <QPoint>
#include <QVector3D>

#include "app/commander/commander_frame_intent.h"
#include "app/commander/commander_pointer_capture.h"
#include "game/commander/commander_input_port.h"
#include "game/commander/commander_latency_probe.h"

class QQuickWindow;

namespace App::Core {

using Game::Commander::CommanderHeldInput;
using Game::Commander::CommanderLatencyProbe;

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
  void poll_pointer(PointerDevice& device,
                    const QPoint& global_center,
                    float sensitivity_scale);

  auto sample_frame_intent(const CommanderHeldInput& held) -> CommanderFrameIntent;

  void suspend_mouse();
  void release_mouse();
  void reset();

  [[nodiscard]] auto mouse_captured() const -> bool { return m_pointer.captured(); }
  [[nodiscard]] auto mouse_warp_supported() const -> bool {
    return m_pointer.warp_supported();
  }

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

  CommanderPointerCapture m_pointer;
  QPoint m_mouse_center;
  bool m_mouse_center_valid = false;
  bool m_mouse_recentering = false;
};

} // namespace App::Core
