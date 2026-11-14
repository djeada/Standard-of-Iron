#include "app/commander/commander_body_facing.h"

#include <algorithm>
#include <cmath>

#include "app/commander/commander_heading.h"

namespace App::Core {

namespace {

// Steering a committed strike toward the view, scaled by its authority.
constexpr float k_strike_redirect_turn_rate_degrees = 900.0F;
// Turning to run where the player points: half a turn in a quarter second, so a
// reversal reads as a plant and pivot rather than a flip.
constexpr float k_travel_turn_rate_degrees = 720.0F;
// Squaring up to the view for a fight.
constexpr float k_face_view_turn_rate_degrees = 1080.0F;

auto turn_toward(float& body_yaw, float view_yaw, float step) -> bool {
  if (step <= 0.0F) {
    return false;
  }
  float const offset = signed_angle_delta(view_yaw, body_yaw);
  if (std::abs(offset) <= step) {
    body_yaw = view_yaw;
    return true;
  }
  body_yaw = wrap_angle_degrees(body_yaw + std::copysign(step, offset));
  return false;
}

} // namespace

auto CommanderBodyFacing::advance(const BodyFacingInput& input) -> float {
  const float view_yaw =
      input.attack_animation_active
          ? wrap_angle_degrees(input.view_yaw + input.strike_aim_offset)
          : input.view_yaw;
  const float dt = input.dt;
  if (!m_body_yaw_valid) {
    m_body_yaw = view_yaw;
    m_body_yaw_valid = true;
  }

  if (input.attack_animation_active && input.action_redirect_authority < 1.0F) {
    float const step = k_strike_redirect_turn_rate_degrees *
                       input.action_redirect_authority * std::max(0.0F, dt);
    static_cast<void>(turn_toward(m_body_yaw, view_yaw, step));
    return m_body_yaw;
  }
  if (input.snap_to_view) {
    m_body_yaw = view_yaw;
    return m_body_yaw;
  }
  if (input.must_face_view) {
    static_cast<void>(turn_toward(
        m_body_yaw, view_yaw, k_face_view_turn_rate_degrees * std::max(0.0F, dt)));
    return m_body_yaw;
  }
  if (input.travel_yaw.has_value()) {
    static_cast<void>(turn_toward(m_body_yaw,
                                  wrap_angle_degrees(*input.travel_yaw),
                                  k_travel_turn_rate_degrees * std::max(0.0F, dt)));
  }
  return m_body_yaw;
}

} // namespace App::Core
