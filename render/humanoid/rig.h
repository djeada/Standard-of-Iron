#pragma once

#include "../entity/registry.h"
#include "../palette.h"
#include "humanoid_specs.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>

namespace Engine::Core {
class Entity;
class World;
class MovementComponent;
class TransformComponent;
} // namespace Engine::Core

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

struct HeadFrame {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};
  float radius{0.0F};
};

struct HumanoidPose {
  QVector3D headPos;
  float headR{};
  QVector3D neck_base;

  HeadFrame headFrame{};

  QVector3D shoulderL, shoulderR;
  QVector3D elbowL, elbowR;
  QVector3D handL, hand_r;

  QVector3D pelvisPos;
  QVector3D knee_l, knee_r;

  float footYOffset{};
  QVector3D footL, foot_r;
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

class HumanoidRendererBase {
public:
  virtual ~HumanoidRendererBase() = default;

  virtual auto getProportionScaling() const -> QVector3D {
    return {1.0F, 1.0F, 1.0F};
  }

  virtual void getVariant(const DrawContext &ctx, uint32_t seed,
                          HumanoidVariant &v) const;

  virtual void customizePose(const DrawContext &ctx,
                             const HumanoidAnimationContext &anim_ctx,
                             uint32_t seed, HumanoidPose &ioPose) const;

  virtual void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                              const HumanoidPose &pose,
                              const HumanoidAnimationContext &anim_ctx,
                              ISubmitter &out) const;

  virtual void drawHelmet(const DrawContext &ctx, const HumanoidVariant &v,
                          const HumanoidPose &pose, ISubmitter &out) const;

  virtual void draw_armorOverlay(const DrawContext &ctx,
                                 const HumanoidVariant &v,
                                 const HumanoidPose &pose, float y_top_cover,
                                 float torso_r, float shoulder_half_span,
                                 float upper_arm_r, const QVector3D &right_axis,
                                 ISubmitter &out) const;

  virtual void drawShoulderDecorations(const DrawContext &ctx,
                                       const HumanoidVariant &v,
                                       const HumanoidPose &pose,
                                       float y_top_cover, float y_neck,
                                       const QVector3D &right_axis,
                                       ISubmitter &out) const;

  virtual void drawFacialHair(const DrawContext &ctx, const HumanoidVariant &v,
                              const HumanoidPose &pose, ISubmitter &out) const;

  void render(const DrawContext &ctx, ISubmitter &out) const;

protected:
  mutable QVector3D m_cachedProportionScale;
  mutable bool m_proportionScaleCached = false;

  static auto resolveFormation(const DrawContext &ctx) -> FormationParams;

  static auto classifyMotionState(const AnimationInputs &anim,
                                  float move_speed) -> HumanoidMotionState;

  static void computeLocomotionPose(uint32_t seed, float time, bool isMoving,
                                    const VariationParams &variation,
                                    HumanoidPose &ioPose);

  static auto sampleAnimState(const DrawContext &ctx) -> AnimationInputs;

  static auto resolveTeamTint(const DrawContext &ctx) -> QVector3D;

  void drawCommonBody(const DrawContext &ctx, const HumanoidVariant &v,
                      HumanoidPose &pose, ISubmitter &out) const;

  static auto headLocalPosition(const HeadFrame &frame,
                                const QVector3D &local) -> QVector3D;

  static auto makeHeadLocalTransform(const QMatrix4x4 &parent,
                                     const HeadFrame &frame,
                                     const QVector3D &local_offset,
                                     float uniform_scale) -> QMatrix4x4;
};

} // namespace Render::GL
