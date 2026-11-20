#include "app/commander/commander_look.h"

#include <QCursor>
#include <QQuickWindow>
#include <QVector2D>

#include <algorithm>
#include <cmath>

#include "app/commander/commander_heading.h"
#include "game/accessibility/commander_input_settings.h"

namespace App::Core {

void CommanderLook::set_pitch(float pitch) {
  m_view_pitch = std::clamp(pitch,
                            -k_commander_view_pitch_limit_degrees,
                            k_commander_view_pitch_limit_degrees);
}

void CommanderLook::remember_previous_view() {
  m_previous_view_yaw = m_view_yaw;
  m_previous_view_pitch = m_view_pitch;
}

void CommanderLook::apply_mouse_delta(double dx, double dy, float sensitivity_scale) {
  if (m_latency_probe != nullptr && (std::abs(dx) > 0.0 || std::abs(dy) > 0.0)) {
    m_latency_probe->note_input();
  }
  constexpr float k_mouse_yaw_sensitivity = 0.18F;
  constexpr float k_mouse_pitch_sensitivity = 0.12F;
  const float user_x = Game::Accessibility::CommanderInput::look_sensitivity_x();
  const float user_y = Game::Accessibility::CommanderInput::look_sensitivity_y();
  const float pitch_sign =
      Game::Accessibility::CommanderInput::invert_look_y() ? 1.0F : -1.0F;
  // The view looks along (sin yaw, cos yaw) in a right-handed world, so its
  // right-hand side is (-cos yaw, sin yaw) and a growing yaw swings the view
  // to the LEFT. A mouse pushed right therefore has to take yaw down. Adding
  // it, as this did, turned the camera the opposite way to the hand.
  m_view_yaw -=
      static_cast<float>(dx) * k_mouse_yaw_sensitivity * sensitivity_scale * user_x;
  m_view_pitch += pitch_sign * static_cast<float>(dy) * k_mouse_pitch_sensitivity *
                  sensitivity_scale * user_y;
  m_view_yaw = std::fmod(m_view_yaw, 360.0F);
  if (m_view_yaw < 0.0F) {
    m_view_yaw += 360.0F;
  }
  m_view_pitch = std::clamp(m_view_pitch,
                            -k_commander_view_pitch_limit_degrees,
                            k_commander_view_pitch_limit_degrees);
}

void CommanderLook::mouse_look_at(double sx,
                                  double sy,
                                  double center_sx,
                                  double center_sy,
                                  QQuickWindow* window,
                                  float sensitivity_scale) {
  const double dx = sx - center_sx;
  const double dy = sy - center_sy;
  if (m_mouse_recentering && std::abs(dx) <= 2.0 && std::abs(dy) <= 2.0) {
    m_mouse_recentering = false;
    return;
  }
  m_mouse_recentering = false;

  if (std::abs(dx) > 0.5 || std::abs(dy) > 0.5) {
    apply_mouse_delta(dx, dy, sensitivity_scale);
  }
  center_mouse(center_sx, center_sy, window);
}

void CommanderLook::center_mouse(double center_sx,
                                 double center_sy,
                                 QQuickWindow* window) {
  if (window == nullptr) {
    return;
  }

  const QPoint local_center(static_cast<int>(std::round(center_sx)),
                            static_cast<int>(std::round(center_sy)));
  m_mouse_center = local_center;
  m_mouse_center_valid = true;
  warp_to_centre(window->mapToGlobal(local_center));
  m_mouse_recentering = false;
}

void CommanderLook::warp_to_centre(const QPoint& global_center) {
  if (QCursor::pos() != global_center) {
    QCursor::setPos(global_center);
  }

  // The pointer is read back rather than assumed: a platform that refuses the
  // warp (Wayland), clamps it (a window reaching past the screen edge) or
  // rounds it (fractional display scaling) leaves it somewhere else, and the
  // next delta must be measured from where it really is. Measuring from the
  // requested centre turned the whole offset into one look delta, which threw
  // the view to the pitch limit the moment control was taken.
  const QPoint landed = QCursor::pos();
  m_mouse_warp_supported =
      (landed - global_center).manhattanLength() <= k_warp_tolerance_pixels;
  m_last_mouse_global = landed;
  m_last_mouse_valid = true;
}

void CommanderLook::poll_mouse(QQuickWindow* window, float sensitivity_scale) {
  if (window == nullptr || !window->isActive()) {
    // Deltas are never measured across a spell without focus: the pointer was
    // somebody else's while the window was inactive.
    m_last_mouse_valid = false;
    return;
  }

  const QPoint current_global = QCursor::pos();
  if (!m_last_mouse_valid) {
    m_last_mouse_global = current_global;
    m_last_mouse_valid = true;
    if (m_mouse_center_valid) {
      warp_to_centre(window->mapToGlobal(m_mouse_center));
    }
    return;
  }

  const QPoint delta = current_global - m_last_mouse_global;
  if (!delta.isNull()) {
    apply_mouse_delta(delta.x(), delta.y(), sensitivity_scale);
  }

  m_last_mouse_global = current_global;
  if (!delta.isNull() && m_mouse_warp_supported && m_mouse_center_valid) {
    warp_to_centre(window->mapToGlobal(m_mouse_center));
  }
}

auto CommanderLook::sample_frame_intent(const CommanderHeldInput& held)
    -> CommanderFrameIntent {
  if (!m_intent_sample_valid) {
    m_intent_sample_yaw = m_view_yaw;
    m_intent_sample_pitch = m_view_pitch;
    m_intent_sample_valid = true;
  }

  m_frame_intent.look_delta =
      QVector2D(rightward_look_delta(m_view_yaw, m_intent_sample_yaw),
                m_view_pitch - m_intent_sample_pitch);
  m_intent_sample_yaw = m_view_yaw;
  m_intent_sample_pitch = m_view_pitch;
  m_frame_intent.view_yaw = m_view_yaw;
  m_frame_intent.view_pitch = m_view_pitch;
  m_frame_intent.move =
      QVector2D(static_cast<float>((held.right ? 1 : 0) - (held.left ? 1 : 0)),
                static_cast<float>((held.forward ? 1 : 0) - (held.backward ? 1 : 0)));
  m_frame_intent.guard = held.secondary_action;
  m_frame_intent.attack_held = held.primary_action;
  m_frame_intent.run = held.run;
  m_frame_intent.dodge_pressed = held.dodge_requested;
  m_frame_intent.jump_pressed = held.jump_requested;
  ++m_frame_intent.frame_index;
  return m_frame_intent;
}

void CommanderLook::release_mouse() {
  m_last_mouse_valid = false;
  m_mouse_center_valid = false;
  m_mouse_recentering = false;
}

void CommanderLook::reset() {

  remember_previous_view();
  m_frame_intent = {};
  m_intent_sample_valid = false;
  m_mouse_center_valid = false;
  m_last_mouse_valid = false;
  m_mouse_warp_supported = false;
  m_mouse_recentering = false;
}

} // namespace App::Core
