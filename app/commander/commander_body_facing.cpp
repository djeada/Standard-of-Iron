#include "app/commander/commander_body_facing.h"

#include <algorithm>
#include <cmath>

#include "app/commander/commander_heading.h"

namespace App::Core {

namespace {

constexpr float k_turn_in_place_threshold_degrees = 50.0F;
constexpr float k_turn_in_place_rate_degrees = 260.0F;
constexpr float k_body_travel_turn_rate_degrees = 900.0F;

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
  const float view_yaw = input.view_yaw;
  const float dt = input.dt;
  if (!m_body_yaw_valid) {
    m_body_yaw = view_yaw;
    m_body_yaw_valid = true;
  }

  if (input.attack_animation_active && input.action_redirect_authority < 1.0F) {
    m_turning_in_place = false;
    float const step = k_body_travel_turn_rate_degrees *
                       input.action_redirect_authority * std::max(0.0F, dt);
    static_cast<void>(turn_toward(m_body_yaw, view_yaw, step));
    return m_body_yaw;
  }
  if (input.must_face_view) {
    m_body_yaw = view_yaw;
    m_turning_in_place = false;
    return m_body_yaw;
  }

  float const offset = signed_angle_delta(view_yaw, m_body_yaw);
  float turn_rate = 0.0F;
  if (input.follows_travel) {
    turn_rate = k_body_travel_turn_rate_degrees;
    m_turning_in_place = false;
  } else {
    if (std::abs(offset) > k_turn_in_place_threshold_degrees) {
      m_turning_in_place = true;
    }
    turn_rate = m_turning_in_place ? k_turn_in_place_rate_degrees : 0.0F;
  }

  if (turn_toward(m_body_yaw, view_yaw, turn_rate * std::max(0.0F, dt))) {
    m_turning_in_place = false;
  }
  return m_body_yaw;
}

} // namespace App::Core
