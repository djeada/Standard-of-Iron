#pragma once

#include <optional>

namespace App::Core {

struct BodyFacingInput {
  float view_yaw{0.0F};
  bool attack_animation_active{false};
  float action_redirect_authority{1.0F};

  float strike_aim_offset{0.0F};
  bool must_face_view{false};

  bool snap_to_view{false};

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
