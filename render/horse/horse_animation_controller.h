#pragma once

#include "rig.h"

namespace Render::GL {

enum class GaitType { IDLE, WALK, TROT, CANTER, GALLOP };

class HorseAnimationController {
public:
  HorseAnimationController(HorseProfile &profile, const AnimationInputs &anim,
                           const HumanoidAnimationContext &rider_ctx);

  void setGait(GaitType gait);
  void idle(float bob_intensity = 1.0F);
  void accelerate(float speed_delta);
  void decelerate(float speed_delta);

  void turn(float yaw_radians, float banking_amount);
  void strafeStep(bool left, float distance);

  void rear(float height_factor);
  void kick(bool rear_legs, float power);
  void buck(float intensity);
  void jumpObstacle(float height, float distance);

  auto getCurrentPhase() const -> float;
  auto getCurrentBob() const -> float;
  auto getStrideCycle() const -> float;

  void updateGaitParameters();

private:
  HorseProfile &m_profile;
  const AnimationInputs &m_anim;
  const HumanoidAnimationContext &m_rider_ctx;

  float m_phase{};
  float m_bob{};
  float m_rein_slack{};

  GaitType m_current_gait{GaitType::IDLE};
  GaitType m_target_gait{GaitType::IDLE};
  float m_gait_transition_progress{1.0F};
  float m_transition_start_time{0.0F};
  float m_speed{};
  float m_turn_angle{};
  float m_banking{};
  bool m_is_rearing{};
  float m_rear_height{};
  bool m_is_kicking{};
  bool m_kick_rear_legs{};
  float m_kick_power{};
  bool m_is_bucking{};
  float m_buck_intensity{};
  bool m_is_jumping{};
  float m_jump_height{};
  float m_jump_distance{};
};

} // namespace Render::GL
