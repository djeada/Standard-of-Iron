#pragma once

namespace App::Core {

struct BodyFacingInput {
  float view_yaw{0.0F};
  bool attack_animation_active{false};
  float action_redirect_authority{1.0F};
  bool must_face_view{false};
  bool follows_travel{false};
  float dt{0.0F};
};

class CommanderBodyFacing {
public:
  [[nodiscard]] auto advance(const BodyFacingInput& input) -> float;
  void reset() {
    m_body_yaw_valid = false;
    m_turning_in_place = false;
  }

private:
  float m_body_yaw = 0.0F;
  bool m_body_yaw_valid = false;
  bool m_turning_in_place = false;
};

} // namespace App::Core
