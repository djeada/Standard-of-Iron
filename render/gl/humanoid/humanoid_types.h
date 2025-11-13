#pragma once

#include "../../palette.h"
#include <QVector3D>
#include <cstdint>

namespace Render::GL {

struct AnimationInputs {
  float time;
  bool isMoving;
  bool is_attacking;
  bool isMelee;
  bool isInHoldMode;
  bool isExitingHold;
  float holdExitProgress;
};

struct FormationParams {
  int individuals_per_unit;
  int max_per_row;
  float spacing;
};

struct AttachmentFrame {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};
  float radius{0.0F};
  float depth{0.0F};
};

using HeadFrame = AttachmentFrame;

struct BodyFrames {
  AttachmentFrame head{};
  AttachmentFrame torso{};
  AttachmentFrame back{};
  AttachmentFrame waist{};
  AttachmentFrame shoulderL{};
  AttachmentFrame shoulderR{};
  AttachmentFrame handL{};
  AttachmentFrame handR{};
  AttachmentFrame footL{};
  AttachmentFrame footR{};
};

struct HumanoidPose {
  QVector3D headPos;
  float headR{};
  QVector3D neck_base;

  HeadFrame headFrame{};

  BodyFrames bodyFrames{};

  QVector3D shoulderL, shoulderR;
  QVector3D elbowL, elbowR;
  QVector3D handL, handR;

  QVector3D pelvisPos;
  QVector3D knee_l, knee_r;

  float footYOffset{};
  QVector3D footL, footR;
};

struct VariationParams {
  float height_scale;
  float bulkScale;
  float stanceWidth;
  float armSwingAmp;
  float walkSpeedMult;
  float postureSlump;
  float shoulderTilt;

  static auto fromSeed(uint32_t seed) -> VariationParams {
    VariationParams v{};

    auto nextRand = [](uint32_t &s) -> float {
      s = s * 1664525U + 1013904223U;
      return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    uint32_t rng = seed;
    v.height_scale = 0.95F + nextRand(rng) * 0.10F;
    v.bulkScale = 0.92F + nextRand(rng) * 0.16F;
    v.stanceWidth = 0.88F + nextRand(rng) * 0.24F;
    v.armSwingAmp = 0.85F + nextRand(rng) * 0.30F;
    v.walkSpeedMult = 0.90F + nextRand(rng) * 0.20F;
    v.postureSlump = nextRand(rng) * 0.08F;
    v.shoulderTilt = (nextRand(rng) - 0.5F) * 0.06F;

    return v;
  }
};

enum class FacialHairStyle {
  None,
  Stubble,
  ShortBeard,
  FullBeard,
  LongBeard,
  Goatee,
  Mustache,
  MustacheAndBeard
};

struct FacialHairParams {
  FacialHairStyle style = FacialHairStyle::None;
  QVector3D color{0.15F, 0.12F, 0.10F};
  float length = 1.0F;
  float thickness = 1.0F;
  float coverage = 1.0F;
  float greyness = 0.0F;
};

struct HumanoidVariant {
  HumanoidPalette palette;
  FacialHairParams facialHair;
  float muscularity = 1.0F;
  float scarring = 0.0F;
  float weathering = 0.0F;
};

enum class HumanoidMotionState {
  Idle,
  Walk,
  Run,
  Hold,
  ExitingHold,
  Attacking
};

struct HumanoidGaitDescriptor {
  HumanoidMotionState state{HumanoidMotionState::Idle};
  float speed{0.0F};
  float normalized_speed{0.0F};
  float cycle_time{0.0F};
  float cycle_phase{0.0F};
  float stride_distance{0.0F};
  QVector3D velocity{0.0F, 0.0F, 0.0F};
  bool has_target{false};
  bool is_airborne{false};

  auto is_stationary() const -> bool { return speed <= 0.01F; }
  auto is_walking() const -> bool { return state == HumanoidMotionState::Walk; }
  auto is_running() const -> bool { return state == HumanoidMotionState::Run; }
  auto is_holding() const -> bool { return state == HumanoidMotionState::Hold; }
  auto is_attacking() const -> bool {
    return state == HumanoidMotionState::Attacking;
  }
};

struct HumanoidAnimationContext {
  AnimationInputs inputs;
  VariationParams variation;
  FormationParams formation;
  HumanoidGaitDescriptor gait{};
  HumanoidMotionState motion_state{HumanoidMotionState::Idle};
  float locomotion_cycle_time{0.0F};
  float locomotion_phase{0.0F};
  float attack_phase{0.0F};
  float jitter_seed{0.0F};
  QVector3D entity_forward{0.0F, 0.0F, 1.0F};
  QVector3D entity_right{1.0F, 0.0F, 0.0F};
  QVector3D entity_up{0.0F, 1.0F, 0.0F};
  QVector3D locomotion_direction{0.0F, 0.0F, 1.0F};
  QVector3D locomotion_velocity{0.0F, 0.0F, 0.0F};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
  QVector3D instance_position{0.0F, 0.0F, 0.0F};
  float move_speed{0.0F};
  bool has_movement_target{false};
  float yaw_radians{0.0F};
  float yaw_degrees{0.0F};

  auto locomotion_speed() const -> float { return gait.speed; }
  auto locomotion_normalized_speed() const -> float {
    return gait.normalized_speed;
  }
  auto locomotion_forward() const -> QVector3D { return locomotion_direction; }
  auto locomotion_velocity_flat() const -> QVector3D { return gait.velocity; }
  auto heading_forward() const -> QVector3D { return entity_forward; }
  auto heading_right() const -> QVector3D { return entity_right; }
  auto heading_up() const -> QVector3D { return entity_up; }
  auto is_stationary() const -> bool { return gait.is_stationary(); }
  auto is_walking() const -> bool { return gait.is_walking(); }
  auto is_running() const -> bool { return gait.is_running(); }
  auto is_holding() const -> bool { return gait.is_holding(); }
  auto is_attacking() const -> bool { return gait.is_attacking(); }
};

} // namespace Render::GL
