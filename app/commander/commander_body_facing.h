#pragma once

#include <optional>

namespace App::Core {

// Who the body faces, in order of precedence:
//  1. a committed strike, which steers toward its aim (the view bent toward the
//     target by CommanderLunge::aim_offset) by its authored authority;
//  2. the view, while the commander is squared up to a fight (attacking,
//     guarding, drawing a bow or locked on);
//  3. the direction he is travelling, while moving freely;
//  4. nothing: a standing body keeps its facing while the camera orbits it.
struct BodyFacingInput {
  float view_yaw{0.0F};
  bool attack_animation_active{false};
  float action_redirect_authority{1.0F};
  // While a strike runs, the body aims this many degrees off the view.
  float strike_aim_offset{0.0F};
  bool must_face_view{false};

  bool snap_to_view{false};
  // Heading of free ground movement; omitted while a roll owns the body.
  std::optional<float> travel_yaw;
  float dt{0.0F};
};

class CommanderBodyFacing {
public:
  [[nodiscard]] auto advance(const BodyFacingInput& input) -> float;
  void reset() { m_body_yaw_valid = false; }

private:
  float m_body_yaw = 0.0F;
  bool m_body_yaw_valid = false;
};

} // namespace App::Core
