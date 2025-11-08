#include "rig.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/units/spawn_type.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../entity/registry.h"
#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../gl/render_constants.h"
#include "../palette.h"
#include "../submitter.h"
#include "humanoid_math.h"
#include <QMatrix4x4>
#include <QVector4D>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <qmatrix4x4.h>
#include <qvectornd.h>

namespace Render::GL {

using namespace Render::GL::Geometry;
using Render::Geom::capsuleBetween;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

namespace {

constexpr float k_run_speed_threshold = 3.2F;
constexpr float k_reference_walk_speed = 2.35F;
constexpr float k_reference_run_speed = 5.10F;

} // namespace

auto HumanoidRendererBase::frameLocalPosition(
    const AttachmentFrame &frame, const QVector3D &local) -> QVector3D {
  float const lx = local.x() * frame.radius;
  float const ly = local.y() * frame.radius;
  float const lz = local.z() * frame.radius;
  return frame.origin + frame.right * lx + frame.up * ly + frame.forward * lz;
}

auto HumanoidRendererBase::makeFrameLocalTransform(
    const QMatrix4x4 &parent, const AttachmentFrame &frame,
    const QVector3D &local_offset, float uniform_scale) -> QMatrix4x4 {
  float scale = frame.radius * uniform_scale;
  if (scale == 0.0F) {
    scale = uniform_scale;
  }

  QVector3D const origin = frameLocalPosition(frame, local_offset);

  QMatrix4x4 local;
  local.setColumn(0, QVector4D(frame.right * scale, 0.0F));
  local.setColumn(1, QVector4D(frame.up * scale, 0.0F));
  local.setColumn(2, QVector4D(frame.forward * scale, 0.0F));
  local.setColumn(3, QVector4D(origin, 1.0F));
  return parent * local;
}

auto HumanoidRendererBase::headLocalPosition(
    const HeadFrame &frame, const QVector3D &local) -> QVector3D {
  return frameLocalPosition(frame, local);
}

auto HumanoidRendererBase::makeHeadLocalTransform(
    const QMatrix4x4 &parent, const HeadFrame &frame,
    const QVector3D &local_offset, float uniform_scale) -> QMatrix4x4 {
  return makeFrameLocalTransform(parent, frame, local_offset, uniform_scale);
}

void HumanoidRendererBase::get_variant(const DrawContext &ctx, uint32_t seed,
                                       HumanoidVariant &v) const {
  QVector3D const team_tint = resolveTeamTint(ctx);
  v.palette = makeHumanoidPalette(team_tint, seed);
}

void HumanoidRendererBase::customize_pose(const DrawContext &,
                                          const HumanoidAnimationContext &,
                                          uint32_t, HumanoidPose &) const {}

void HumanoidRendererBase::addAttachments(const DrawContext &,
                                          const HumanoidVariant &,
                                          const HumanoidPose &,
                                          const HumanoidAnimationContext &,
                                          ISubmitter &) const {}

auto HumanoidRendererBase::resolveTeamTint(const DrawContext &ctx)
    -> QVector3D {
  QVector3D tunic(0.8F, 0.9F, 1.0F);
  Engine::Core::UnitComponent *unit = nullptr;
  Engine::Core::RenderableComponent *rc = nullptr;

  if (ctx.entity != nullptr) {
    unit = ctx.entity->getComponent<Engine::Core::UnitComponent>();
    rc = ctx.entity->getComponent<Engine::Core::RenderableComponent>();
  }

  if ((unit != nullptr) && unit->owner_id > 0) {
    tunic = Game::Visuals::team_colorForOwner(unit->owner_id);
  } else if (rc != nullptr) {
    tunic = QVector3D(rc->color[0], rc->color[1], rc->color[2]);
  }

  return tunic;
}

auto HumanoidRendererBase::resolveFormation(const DrawContext &ctx)
    -> FormationParams {
  FormationParams params{};
  params.individuals_per_unit = 1;
  params.max_per_row = 1;
  params.spacing = 0.75F;

  if (ctx.entity != nullptr) {
    auto *unit = ctx.entity->getComponent<Engine::Core::UnitComponent>();
    if (unit != nullptr) {
      params.individuals_per_unit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              unit->spawn_type);
      params.max_per_row =
          Game::Units::TroopConfig::instance().getMaxUnitsPerRow(
              unit->spawn_type);
      if (unit->spawn_type == Game::Units::SpawnType::MountedKnight) {
        params.spacing = 1.35F;
      }
    }
  }

  return params;
}

auto HumanoidRendererBase::classifyMotionState(
    const AnimationInputs &anim, float move_speed) -> HumanoidMotionState {
  if (anim.isInHoldMode) {
    return HumanoidMotionState::Hold;
  }
  if (anim.isExitingHold) {
    return HumanoidMotionState::ExitingHold;
  }
  if (anim.is_attacking) {
    return HumanoidMotionState::Attacking;
  }
  if (anim.isMoving) {
    return (move_speed > k_run_speed_threshold) ? HumanoidMotionState::Run
                                                : HumanoidMotionState::Walk;
  }
  return HumanoidMotionState::Idle;
}

auto HumanoidRendererBase::sampleAnimState(const DrawContext &ctx)
    -> AnimationInputs {
  AnimationInputs anim{};
  anim.time = ctx.animationTime;
  anim.isMoving = false;
  anim.is_attacking = false;
  anim.isMelee = false;
  anim.isInHoldMode = false;
  anim.isExitingHold = false;
  anim.holdExitProgress = 0.0F;

  if (ctx.entity == nullptr) {
    return anim;
  }

  if (ctx.entity->hasComponent<Engine::Core::PendingRemovalComponent>()) {
    return anim;
  }

  auto *movement = ctx.entity->getComponent<Engine::Core::MovementComponent>();
  auto *attack = ctx.entity->getComponent<Engine::Core::AttackComponent>();
  auto *attack_target =
      ctx.entity->getComponent<Engine::Core::AttackTargetComponent>();
  auto *transform =
      ctx.entity->getComponent<Engine::Core::TransformComponent>();
  auto *hold_mode = ctx.entity->getComponent<Engine::Core::HoldModeComponent>();

  anim.isInHoldMode = ((hold_mode != nullptr) && hold_mode->active);
  if ((hold_mode != nullptr) && !hold_mode->active &&
      hold_mode->exitCooldown > 0.0F) {
    anim.isExitingHold = true;
    anim.holdExitProgress =
        1.0F - (hold_mode->exitCooldown / hold_mode->standUpDuration);
  }
  anim.isMoving = ((movement != nullptr) && movement->hasTarget);

  if ((attack != nullptr) && (attack_target != nullptr) &&
      attack_target->target_id > 0 && (transform != nullptr)) {
    anim.isMelee = (attack->currentMode ==
                    Engine::Core::AttackComponent::CombatMode::Melee);

    bool const stationary = !anim.isMoving;
    float const current_cooldown =
        anim.isMelee ? attack->meleeCooldown : attack->cooldown;
    bool const recently_fired =
        attack->timeSinceLast < std::min(current_cooldown, 0.45F);
    bool target_in_range = false;

    if (ctx.world != nullptr) {
      auto *target = ctx.world->getEntity(attack_target->target_id);
      if (target != nullptr) {
        auto *target_transform =
            target->getComponent<Engine::Core::TransformComponent>();
        if (target_transform != nullptr) {
          float const dx = target_transform->position.x - transform->position.x;
          float const dz = target_transform->position.z - transform->position.z;
          float const dist_squared = dx * dx + dz * dz;
          float target_radius = 0.0F;
          if (target->hasComponent<Engine::Core::BuildingComponent>()) {
            target_radius =
                std::max(target_transform->scale.x, target_transform->scale.z) *
                0.5F;
          } else {
            target_radius =
                std::max(target_transform->scale.x, target_transform->scale.z) *
                0.5F;
          }
          float const effective_range = attack->range + target_radius + 0.25F;
          target_in_range = (dist_squared <= effective_range * effective_range);
        }
      }
    }

    anim.is_attacking = stationary && (target_in_range || recently_fired);
  }

  return anim;
}

void HumanoidRendererBase::computeLocomotionPose(
    uint32_t seed, float time, bool isMoving, const VariationParams &variation,
    HumanoidPose &pose) {
  using HP = HumanProportions;

  float const h_scale = variation.height_scale;

  pose.headPos =
      QVector3D(0.0F, (HP::HEAD_TOP_Y + HP::CHIN_Y) * 0.5F * h_scale, 0.0F);
  pose.headR = HP::HEAD_RADIUS * h_scale;
  pose.neck_base = QVector3D(0.0F, HP::NECK_BASE_Y * h_scale, 0.0F);

  float const b_scale = variation.bulkScale;
  float const s_width = variation.stanceWidth;

  float const half_shoulder_span = 0.5F * HP::SHOULDER_WIDTH * b_scale;
  pose.shoulderL =
      QVector3D(-half_shoulder_span, HP::SHOULDER_Y * h_scale, 0.0F);
  pose.shoulderR =
      QVector3D(half_shoulder_span, HP::SHOULDER_Y * h_scale, 0.0F);

  pose.pelvisPos = QVector3D(0.0F, HP::WAIST_Y * h_scale, 0.0F);

  float const rest_stride = 0.06F + (variation.armSwingAmp - 1.0F) * 0.045F;
  float const foot_x_span = HP::SHOULDER_WIDTH * 0.62F * s_width;
  pose.footYOffset = 0.022F;
  pose.footL =
      QVector3D(-foot_x_span, HP::GROUND_Y + pose.footYOffset, rest_stride);
  pose.foot_r =
      QVector3D(foot_x_span, HP::GROUND_Y + pose.footYOffset, -rest_stride);

  pose.shoulderL.setY(pose.shoulderL.y() + variation.shoulderTilt);
  pose.shoulderR.setY(pose.shoulderR.y() - variation.shoulderTilt);

  float const slouch_offset = variation.postureSlump * 0.15F;
  pose.shoulderL.setZ(pose.shoulderL.z() + slouch_offset);
  pose.shoulderR.setZ(pose.shoulderR.z() + slouch_offset);

  float const foot_inward_jitter = (hash_01(seed ^ 0x5678U) - 0.5F) * 0.02F;
  float const foot_forward_jitter = (hash_01(seed ^ 0x9ABCU) - 0.5F) * 0.035F;

  pose.footL.setX(pose.footL.x() + foot_inward_jitter);
  pose.foot_r.setX(pose.foot_r.x() - foot_inward_jitter);
  pose.footL.setZ(pose.footL.z() + foot_forward_jitter);
  pose.foot_r.setZ(pose.foot_r.z() - foot_forward_jitter);

  float const arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
  float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

  pose.handL =
      QVector3D(-0.05F + arm_asymmetry,
                HP::SHOULDER_Y * h_scale + 0.05F + arm_height_jitter, 0.55F);
  pose.hand_r = QVector3D(
      0.15F - arm_asymmetry * 0.5F,
      HP::SHOULDER_Y * h_scale + 0.15F + arm_height_jitter * 0.8F, 0.20F);

  if (isMoving) {

    float const walk_cycle_time = 0.8F / variation.walkSpeedMult;
    float const walk_phase = std::fmod(time * (1.0F / walk_cycle_time), 1.0F);
    float const left_phase = walk_phase;
    float const right_phase = std::fmod(walk_phase + 0.5F, 1.0F);

    const float ground_y = HP::GROUND_Y;

    const float stride_length = 0.35F * variation.armSwingAmp;

    auto animate_foot = [ground_y, &pose, stride_length](QVector3D &foot,
                                                         float phase) {
      float const lift = std::sin(phase * 2.0F * std::numbers::pi_v<float>);
      if (lift > 0.0F) {
        foot.setY(ground_y + pose.footYOffset + lift * 0.12F);
      } else {
        foot.setY(ground_y + pose.footYOffset);
      }
      foot.setZ(foot.z() +
                std::sin((phase - 0.25F) * 2.0F * std::numbers::pi_v<float>) *
                    stride_length);
    };

    animate_foot(pose.footL, left_phase);
    animate_foot(pose.foot_r, right_phase);

    float const hip_sway =
        std::sin(walk_phase * 2.0F * std::numbers::pi_v<float>) * 0.02F *
        variation.armSwingAmp;
    pose.shoulderL.setX(pose.shoulderL.x() + hip_sway);
    pose.shoulderR.setX(pose.shoulderR.x() + hip_sway);
  }

  QVector3D const hip_l = pose.pelvisPos + QVector3D(-0.10F, -0.02F, 0.0F);
  QVector3D const hip_r = pose.pelvisPos + QVector3D(0.10F, -0.02F, 0.0F);

  auto solve_leg = [&](const QVector3D &hip, const QVector3D &foot,
                       bool is_left) -> QVector3D {
    QVector3D hip_to_foot = foot - hip;
    float const distance = hip_to_foot.length();
    if (distance < 1e-5F) {
      return hip;
    }

    float const upper_len = HP::UPPER_LEG_LEN * h_scale;
    float const lower_len = HP::LOWER_LEG_LEN * h_scale;
    float const reach = upper_len + lower_len;
    float const min_reach =
        std::max(std::abs(upper_len - lower_len) + 1e-4F, 1e-3F);
    float const max_reach = std::max(reach - 1e-4F, min_reach + 1e-4F);
    float const clamped_dist = std::clamp(distance, min_reach, max_reach);

    QVector3D const dir = hip_to_foot / distance;

    float cos_theta = (upper_len * upper_len + clamped_dist * clamped_dist -
                       lower_len * lower_len) /
                      (2.0F * upper_len * clamped_dist);
    cos_theta = std::clamp(cos_theta, -1.0F, 1.0F);
    float const sin_theta =
        std::sqrt(std::max(0.0F, 1.0F - cos_theta * cos_theta));

    QVector3D bend_pref = is_left ? QVector3D(-0.24F, 0.0F, 0.95F)
                                  : QVector3D(0.24F, 0.0F, 0.95F);
    bend_pref.normalize();

    QVector3D bend_axis =
        bend_pref - dir * QVector3D::dotProduct(dir, bend_pref);
    if (bend_axis.lengthSquared() < 1e-6F) {
      bend_axis = QVector3D::crossProduct(dir, QVector3D(0.0F, 1.0F, 0.0F));
      if (bend_axis.lengthSquared() < 1e-6F) {
        bend_axis = QVector3D::crossProduct(dir, QVector3D(1.0F, 0.0F, 0.0F));
      }
    }
    bend_axis.normalize();

    QVector3D const knee = hip + dir * (cos_theta * upper_len) +
                           bend_axis * (sin_theta * upper_len);

    float const knee_floor = HP::GROUND_Y + pose.footYOffset * 0.5F;
    if (knee.y() < knee_floor) {
      QVector3D adjusted = knee;
      adjusted.setY(knee_floor);
      return adjusted;
    }

    return knee;
  };

  pose.knee_l = solve_leg(hip_l, pose.footL, true);
  pose.knee_r = solve_leg(hip_r, pose.foot_r, false);

  QVector3D right_axis = pose.shoulderR - pose.shoulderL;
  right_axis.setY(0.0F);
  if (right_axis.lengthSquared() < 1e-8F) {
    right_axis = QVector3D(1, 0, 0);
  }
  right_axis.normalize();
  QVector3D const outward_l = -right_axis;
  QVector3D const outward_r = right_axis;

  pose.elbowL = elbowBendTorso(pose.shoulderL, pose.handL, outward_l, 0.45F,
                               0.15F, -0.08F, +1.0F);
  pose.elbowR = elbowBendTorso(pose.shoulderR, pose.hand_r, outward_r, 0.48F,
                               0.12F, 0.02F, +1.0F);
}

void HumanoidRendererBase::drawCommonBody(const DrawContext &ctx,
                                          const HumanoidVariant &v,
                                          HumanoidPose &pose,
                                          ISubmitter &out) const {
  using HP = HumanProportions;

  QVector3D const scaling = get_proportion_scaling();
  float const width_scale = scaling.x();
  float const height_scale = scaling.y();

  float const head_scale = 1.0F;

  QVector3D right_axis = pose.shoulderR - pose.shoulderL;
  if (right_axis.lengthSquared() < 1e-8F) {
    right_axis = QVector3D(1, 0, 0);
  }
  right_axis.normalize();

  QVector3D const up_axis(0.0F, 1.0F, 0.0F);
  QVector3D forward_axis = QVector3D::crossProduct(right_axis, up_axis);
  if (forward_axis.lengthSquared() < 1e-8F) {
    forward_axis = QVector3D(0.0F, 0.0F, 1.0F);
  }
  forward_axis.normalize();

  const float y_shoulder = 0.5F * (pose.shoulderL.y() + pose.shoulderR.y());
  const float y_neck = pose.neck_base.y();
  const float shoulder_half_span =
      0.5F * std::abs(pose.shoulderR.x() - pose.shoulderL.x());
  const float torso_r =
      std::max(HP::TORSO_TOP_R * width_scale, shoulder_half_span * 0.95F);

  const float y_top_cover = std::max(y_shoulder + 0.04F, y_neck + 0.00F);

  const float upper_arm_r = HP::UPPER_ARM_R * width_scale;
  const float fore_arm_r = HP::FORE_ARM_R * width_scale;
  const float joint_r = HP::HAND_RADIUS * width_scale * 1.05F;
  const float hand_r = HP::HAND_RADIUS * width_scale * 0.95F;

  const float leg_joint_r = HP::LOWER_LEG_R * width_scale * 0.95F;
  const float thigh_r = HP::UPPER_LEG_R * width_scale;
  const float shin_r = HP::LOWER_LEG_R * width_scale;
  const float foot_radius = shin_r * 1.10F;

  QVector3D const tunic_top{0.0F, y_top_cover - 0.006F, 0.0F};

  QVector3D const tunic_bot{0.0F, pose.pelvisPos.y() + 0.03F, 0.0F};

  QMatrix4x4 torso_transform =
      cylinderBetween(ctx.model, tunic_top, tunic_bot, torso_r);
  torso_transform.scale(1.0F, 1.0F, 0.65F);

  out.mesh(getUnitTorso(), torso_transform, v.palette.cloth, nullptr, 1.0F);

  QVector3D const chin_pos{0.0F, pose.headPos.y() - pose.headR, 0.0F};
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.neck_base, chin_pos,
                           HP::NECK_RADIUS * width_scale),
           v.palette.skin * 0.9F, nullptr, 1.0F);

  float const head_r = pose.headR;
  out.mesh(getUnitSphere(), sphereAt(ctx.model, pose.headPos, head_r),
           v.palette.skin, nullptr, 1.0F);

  QVector3D head_up = pose.headPos - pose.neck_base;
  if (head_up.lengthSquared() < 1e-8F) {
    head_up = up_axis;
  } else {
    head_up.normalize();
  }

  QVector3D head_right =
      right_axis - head_up * QVector3D::dotProduct(right_axis, head_up);
  if (head_right.lengthSquared() < 1e-8F) {
    head_right = QVector3D::crossProduct(head_up, forward_axis);
    if (head_right.lengthSquared() < 1e-8F) {
      head_right = QVector3D(1.0F, 0.0F, 0.0F);
    }
  }
  head_right.normalize();

  if (QVector3D::dotProduct(head_right, right_axis) < 0.0F) {
    head_right = -head_right;
  }

  QVector3D head_forward = QVector3D::crossProduct(head_right, head_up);
  if (head_forward.lengthSquared() < 1e-8F) {
    head_forward = forward_axis;
  } else {
    head_forward.normalize();
  }

  if (QVector3D::dotProduct(head_forward, forward_axis) < 0.0F) {
    head_right = -head_right;
    head_forward = -head_forward;
  }

  pose.headFrame.origin = pose.headPos;
  pose.headFrame.right = head_right;
  pose.headFrame.up = head_up;
  pose.headFrame.forward = head_forward;
  pose.headFrame.radius = head_r;

  pose.bodyFrames.head = pose.headFrame;

  pose.bodyFrames.torso.origin = QVector3D(0.0F, y_shoulder, 0.0F);
  pose.bodyFrames.torso.right = right_axis;
  pose.bodyFrames.torso.up = up_axis;
  pose.bodyFrames.torso.forward = forward_axis;
  pose.bodyFrames.torso.radius = torso_r;

  pose.bodyFrames.back.origin =
      QVector3D(0.0F, y_shoulder, 0.0F) - forward_axis * (torso_r * 0.65F);
  pose.bodyFrames.back.right = right_axis;
  pose.bodyFrames.back.up = up_axis;
  pose.bodyFrames.back.forward = -forward_axis;
  pose.bodyFrames.back.radius = torso_r * 0.8F;

  pose.bodyFrames.waist.origin = pose.pelvisPos;
  pose.bodyFrames.waist.right = right_axis;
  pose.bodyFrames.waist.up = up_axis;
  pose.bodyFrames.waist.forward = forward_axis;
  pose.bodyFrames.waist.radius = torso_r * 0.85F;

  QVector3D shoulder_up = (pose.shoulderL - pose.pelvisPos).normalized();
  QVector3D shoulder_forward_l =
      QVector3D::crossProduct(-right_axis, shoulder_up);
  if (shoulder_forward_l.lengthSquared() < 1e-8F) {
    shoulder_forward_l = forward_axis;
  } else {
    shoulder_forward_l.normalize();
  }

  pose.bodyFrames.shoulderL.origin = pose.shoulderL;
  pose.bodyFrames.shoulderL.right = -right_axis;
  pose.bodyFrames.shoulderL.up = shoulder_up;
  pose.bodyFrames.shoulderL.forward = shoulder_forward_l;
  pose.bodyFrames.shoulderL.radius = upper_arm_r;

  QVector3D shoulder_forward_r =
      QVector3D::crossProduct(right_axis, shoulder_up);
  if (shoulder_forward_r.lengthSquared() < 1e-8F) {
    shoulder_forward_r = forward_axis;
  } else {
    shoulder_forward_r.normalize();
  }

  pose.bodyFrames.shoulderR.origin = pose.shoulderR;
  pose.bodyFrames.shoulderR.right = right_axis;
  pose.bodyFrames.shoulderR.up = shoulder_up;
  pose.bodyFrames.shoulderR.forward = shoulder_forward_r;
  pose.bodyFrames.shoulderR.radius = upper_arm_r;

  QVector3D hand_up_l = (pose.handL - pose.elbowL);
  if (hand_up_l.lengthSquared() > 1e-8F) {
    hand_up_l.normalize();
  } else {
    hand_up_l = up_axis;
  }
  QVector3D hand_forward_l = QVector3D::crossProduct(-right_axis, hand_up_l);
  if (hand_forward_l.lengthSquared() < 1e-8F) {
    hand_forward_l = forward_axis;
  } else {
    hand_forward_l.normalize();
  }

  pose.bodyFrames.handL.origin = pose.handL;
  pose.bodyFrames.handL.right = -right_axis;
  pose.bodyFrames.handL.up = hand_up_l;
  pose.bodyFrames.handL.forward = hand_forward_l;
  pose.bodyFrames.handL.radius = hand_r;

  QVector3D hand_up_r = (pose.hand_r - pose.elbowR);
  if (hand_up_r.lengthSquared() > 1e-8F) {
    hand_up_r.normalize();
  } else {
    hand_up_r = up_axis;
  }
  QVector3D hand_forward_r = QVector3D::crossProduct(right_axis, hand_up_r);
  if (hand_forward_r.lengthSquared() < 1e-8F) {
    hand_forward_r = forward_axis;
  } else {
    hand_forward_r.normalize();
  }

  pose.bodyFrames.handR.origin = pose.hand_r;
  pose.bodyFrames.handR.right = right_axis;
  pose.bodyFrames.handR.up = hand_up_r;
  pose.bodyFrames.handR.forward = hand_forward_r;
  pose.bodyFrames.handR.radius = hand_r;

  QVector3D foot_up_l = up_axis;
  QVector3D foot_forward_l = forward_axis - right_axis * 0.12F;
  if (foot_forward_l.lengthSquared() > 1e-8F) {
    foot_forward_l.normalize();
  } else {
    foot_forward_l = forward_axis;
  }

  pose.bodyFrames.footL.origin = pose.footL;
  pose.bodyFrames.footL.right = -right_axis;
  pose.bodyFrames.footL.up = foot_up_l;
  pose.bodyFrames.footL.forward = foot_forward_l;
  pose.bodyFrames.footL.radius = foot_radius;

  QVector3D foot_forward_r = forward_axis + right_axis * 0.12F;
  if (foot_forward_r.lengthSquared() > 1e-8F) {
    foot_forward_r.normalize();
  } else {
    foot_forward_r = forward_axis;
  }

  pose.bodyFrames.footR.origin = pose.foot_r;
  pose.bodyFrames.footR.right = right_axis;
  pose.bodyFrames.footR.up = foot_up_l;
  pose.bodyFrames.footR.forward = foot_forward_r;
  pose.bodyFrames.footR.radius = foot_radius;

  QVector3D const iris = QVector3D(0.10F, 0.10F, 0.12F);
  auto eyePosition = [&](float lateral) {
    QVector3D const local(lateral, 0.12F, 0.92F);
    QVector3D world = frameLocalPosition(pose.bodyFrames.head, local);
    world +=
        pose.bodyFrames.head.forward * (pose.bodyFrames.head.radius * 0.02F);
    return world;
  };
  QVector3D const left_eye_world = eyePosition(-0.32F);
  QVector3D const right_eye_world = eyePosition(0.32F);
  float const eye_radius = pose.bodyFrames.head.radius * 0.17F;

  out.mesh(getUnitSphere(), sphereAt(ctx.model, left_eye_world, eye_radius),
           iris, nullptr, 1.0F);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, right_eye_world, eye_radius),
           iris, nullptr, 1.0F);

  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.shoulderL, pose.elbowL, upper_arm_r),
           v.palette.cloth, nullptr, 1.0F);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, pose.elbowL, joint_r),
           v.palette.cloth * 0.95F, nullptr, 1.0F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.elbowL, pose.handL, fore_arm_r),
           v.palette.skin * 0.95F, nullptr, 1.0F);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, pose.handL, hand_r),
           v.palette.leatherDark * 0.92F, nullptr, 1.0F);

  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.shoulderR, pose.elbowR, upper_arm_r),
           v.palette.cloth, nullptr, 1.0F);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, pose.elbowR, joint_r),
           v.palette.cloth * 0.95F, nullptr, 1.0F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.elbowR, pose.hand_r, fore_arm_r),
           v.palette.skin * 0.95F, nullptr, 1.0F);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, pose.hand_r, hand_r),
           v.palette.leatherDark * 0.92F, nullptr, 1.0F);

  QVector3D const hip_l = pose.pelvisPos + QVector3D(-0.10F, -0.02F, 0.0F);
  QVector3D const hip_r = pose.pelvisPos + QVector3D(0.10F, -0.02F, 0.0F);

  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, hip_l, pose.knee_l, thigh_r),
           v.palette.cloth * 0.92F, nullptr, 1.0F);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, pose.knee_l, leg_joint_r),
           v.palette.cloth * 0.90F, nullptr, 1.0F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.knee_l, pose.footL, shin_r),
           v.palette.leather * 0.95F, nullptr, 1.0F);

  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, hip_r, pose.knee_r, thigh_r),
           v.palette.cloth * 0.92F, nullptr, 1.0F);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, pose.knee_r, leg_joint_r),
           v.palette.cloth * 0.90F, nullptr, 1.0F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.knee_r, pose.foot_r, shin_r),
           v.palette.leather * 0.95F, nullptr, 1.0F);

  auto draw_foot = [&](const QVector3D &ankle, bool is_left) {
    QVector3D lateral = is_left ? -right_axis : right_axis;
    QVector3D foot_forward =
        forward_axis + lateral * (is_left ? -0.12F : 0.12F);
    if (foot_forward.lengthSquared() < 1e-6F) {
      foot_forward = forward_axis;
    }
    foot_forward.normalize();

    float const heel_span = foot_radius * 2.20F;
    float const toe_span = foot_radius * 3.30F;
    float const sole_y = HP::GROUND_Y + 0.003F;

    QVector3D heel = ankle - foot_forward * heel_span;
    QVector3D toe = ankle + foot_forward * toe_span;
    heel.setY(sole_y);
    toe.setY(sole_y);

    QMatrix4x4 foot_mat = capsuleBetween(ctx.model, heel, toe, foot_radius);
    
    float const width_at_heel = 1.2F;
    float const width_at_toe = 2.5F;
    float const height_scale = 0.26F;
    float const depth_scale = 1.0F;
    
    QMatrix4x4 scale_mat;
    scale_mat.setToIdentity();
    scale_mat.scale((width_at_heel + width_at_toe) * 0.5F, height_scale, depth_scale);
    
    QMatrix4x4 shear_mat;
    shear_mat.setToIdentity();
    shear_mat(0, 2) = (width_at_toe - width_at_heel) * 0.5F;
    
    foot_mat = foot_mat * scale_mat * shear_mat;
    
    out.mesh(getUnitCapsule(), foot_mat, v.palette.leatherDark * 0.92F, nullptr,
             1.0F);
  };

  draw_foot(pose.footL, true);
  draw_foot(pose.foot_r, false);

  draw_armorOverlay(ctx, v, pose, y_top_cover, torso_r, shoulder_half_span,
                    upper_arm_r, right_axis, out);

  drawShoulderDecorations(ctx, v, pose, y_top_cover, pose.neck_base.y(),
                          right_axis, out);

  draw_helmet(ctx, v, pose, out);
}

void HumanoidRendererBase::draw_armorOverlay(const DrawContext &,
                                             const HumanoidVariant &,
                                             const HumanoidPose &, float, float,
                                             float, float, const QVector3D &,
                                             ISubmitter &) const {}

void HumanoidRendererBase::draw_armor(const DrawContext &,
                                      const HumanoidVariant &,
                                      const HumanoidPose &,
                                      const HumanoidAnimationContext &,
                                      ISubmitter &) const {}

void HumanoidRendererBase::drawShoulderDecorations(const DrawContext &,
                                                   const HumanoidVariant &,
                                                   const HumanoidPose &, float,
                                                   float, const QVector3D &,
                                                   ISubmitter &) const {}

void HumanoidRendererBase::draw_helmet(const DrawContext &,
                                       const HumanoidVariant &,
                                       const HumanoidPose &,
                                       ISubmitter &) const {}

void HumanoidRendererBase::drawFacialHair(const DrawContext &ctx,
                                          const HumanoidVariant &v,
                                          const HumanoidPose &pose,
                                          ISubmitter &out) const {
  const FacialHairParams &fh = v.facialHair;

  if (fh.style == FacialHairStyle::None || fh.coverage < 0.01F) {
    return;
  }

  const AttachmentFrame &frame = pose.bodyFrames.head;
  float const head_r = frame.radius;
  if (head_r <= 0.0F) {
    return;
  }

  auto saturate = [](const QVector3D &c) -> QVector3D {
    return {std::clamp(c.x(), 0.0F, 1.0F), std::clamp(c.y(), 0.0F, 1.0F),
            std::clamp(c.z(), 0.0F, 1.0F)};
  };

  QVector3D hair_color = fh.color * (1.0F - fh.greyness) +
                         QVector3D(0.75F, 0.75F, 0.75F) * fh.greyness;
  QVector3D hair_dark = hair_color * 0.80F;
  QVector3D const hair_root = hair_dark * 0.95F;
  QVector3D const hair_tip = hair_color * 1.08F;

  float const chin_y = -head_r * 0.95F;
  float const mouth_y = -head_r * 0.18F;
  float const jaw_z = head_r * 0.68F;

  float const chin_norm = chin_y / head_r;
  float const mouth_norm = mouth_y / head_r;
  float const jaw_forward_norm = jaw_z / head_r;

  uint32_t rand_state = 0x9E3779B9U;
  if (ctx.entity != nullptr) {
    uintptr_t ptr = reinterpret_cast<uintptr_t>(ctx.entity);
    rand_state ^= static_cast<uint32_t>(ptr & 0xFFFFFFFFU);
    rand_state ^= static_cast<uint32_t>((ptr >> 32) & 0xFFFFFFFFU);
  }
  rand_state ^= static_cast<uint32_t>(fh.length * 9973.0F);
  rand_state ^= static_cast<uint32_t>(fh.thickness * 6151.0F);
  rand_state ^= static_cast<uint32_t>(fh.coverage * 4099.0F);

  auto random01 = [&]() -> float {
    rand_state = rand_state * 1664525U + 1013904223U;
    return hash_01(rand_state);
  };

  auto jitter = [&](float amplitude) -> float {
    return (random01() - 0.5F) * 2.0F * amplitude;
  };

  auto place_strands = [&](int rows, int segments, float jaw_span,
                           float row_spacing_norm, float base_length_norm,
                           float length_variation, float forward_bias_norm,
                           float base_radius_norm) {
    if (rows <= 0 || segments <= 0) {
      return;
    }

    const float phi_half_range = std::max(0.35F, jaw_span * 0.5F);
    const float base_y_norm = chin_norm + 0.10F;
    for (int row = 0; row < rows; ++row) {
      float const row_t = (rows > 1) ? float(row) / float(rows - 1) : 0.0F;
      float const target_y_norm =
          std::clamp(base_y_norm + row_t * row_spacing_norm, -0.92F, 0.10F);
      float const plane_radius =
          std::sqrt(std::max(0.02F, 1.0F - target_y_norm * target_y_norm));
      for (int seg = 0; seg < segments; ++seg) {
        float const seg_t =
            (segments > 1) ? float(seg) / float(segments - 1) : 0.5F;
        float const base_phi = (seg_t - 0.5F) * jaw_span;
        float const phi = std::clamp(base_phi + jitter(0.25F), -phi_half_range,
                                     phi_half_range);
        float const coverage_falloff =
            1.0F - std::abs(phi) / std::max(0.001F, phi_half_range);
        float const keep_prob = std::clamp(
            fh.coverage * (0.75F + coverage_falloff * 0.35F), 0.05F, 1.0F);
        if (random01() > keep_prob) {
          continue;
        }

        float const wrap_scale = 0.80F + (1.0F - row_t) * 0.20F;
        float lateral_norm = plane_radius * std::sin(phi) * wrap_scale;
        float forward_norm = plane_radius * std::cos(phi);
        lateral_norm += jitter(0.06F);
        forward_norm += jitter(0.08F);
        float const y_norm = target_y_norm + jitter(0.05F);

        QVector3D surface_dir(lateral_norm, y_norm,
                              forward_norm *
                                      (0.75F + forward_bias_norm * 0.45F) +
                                  (forward_bias_norm - 0.05F));
        float const dir_len = surface_dir.length();
        if (dir_len < 1e-4F) {
          continue;
        }
        surface_dir /= dir_len;

        float const shell = 1.02F + jitter(0.03F);
        QVector3D const root = frameLocalPosition(frame, surface_dir * shell);

        QVector3D local_dir(jitter(0.15F),
                            -(0.55F + row_t * 0.30F) + jitter(0.10F),
                            forward_bias_norm + row_t * 0.20F + jitter(0.12F));
        QVector3D strand_dir =
            frame.right * local_dir.x() + frame.up * local_dir.y() +
            frame.forward * local_dir.z() - surface_dir * 0.25F;
        if (strand_dir.lengthSquared() < 1e-6F) {
          continue;
        }
        strand_dir.normalize();

        float const strand_length = base_length_norm * fh.length *
                                    (1.0F + length_variation * jitter(0.5F)) *
                                    (1.0F + row_t * 0.25F);
        if (strand_length < 0.05F) {
          continue;
        }

        QVector3D const tip = root + strand_dir * (head_r * strand_length);

        float const base_radius =
            std::max(head_r * base_radius_norm * fh.thickness *
                         (0.7F + coverage_falloff * 0.35F),
                     head_r * 0.010F);
        float const mid_radius = base_radius * 0.55F;

        float const color_jitter = 0.85F + random01() * 0.30F;
        QVector3D const root_color = saturate(hair_root * color_jitter);
        QVector3D const tip_color = saturate(hair_tip * color_jitter);

        QMatrix4x4 base_blob = sphereAt(ctx.model, root, base_radius * 0.95F);
        out.mesh(getUnitSphere(), base_blob, root_color, nullptr, 1.0F);

        QVector3D const mid = root + (tip - root) * 0.40F;
        out.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, root, mid, base_radius), root_color,
                 nullptr, 1.0F);

        out.mesh(getUnitCone(), coneFromTo(ctx.model, mid, tip, mid_radius),
                 tip_color, nullptr, 1.0F);
      }
    }
  };

  auto place_mustache = [&](int segments, float base_length_norm,
                            float upward_bias_norm) {
    if (segments <= 0) {
      return;
    }

    float const mustache_y_norm = mouth_norm + upward_bias_norm - 0.04F;
    float const phi_half_range = 0.55F;
    for (int side = -1; side <= 1; side += 2) {
      for (int seg = 0; seg < segments; ++seg) {
        float const t =
            (segments > 1) ? float(seg) / float(segments - 1) : 0.5F;
        float const base_phi = (t - 0.5F) * (phi_half_range * 2.0F);
        float const phi = std::clamp(base_phi + jitter(0.18F), -phi_half_range,
                                     phi_half_range);
        float const plane_radius = std::sqrt(
            std::max(0.02F, 1.0F - mustache_y_norm * mustache_y_norm));
        float lateral_norm = plane_radius * std::sin(phi);
        float forward_norm = plane_radius * std::cos(phi);
        lateral_norm += jitter(0.04F);
        forward_norm += jitter(0.05F);
        if (random01() > fh.coverage) {
          continue;
        }
        QVector3D surface_dir(lateral_norm, mustache_y_norm + jitter(0.03F),
                              forward_norm * 0.85F + 0.20F);
        float const dir_len = surface_dir.length();
        if (dir_len < 1e-4F) {
          continue;
        }
        surface_dir /= dir_len;
        QVector3D const root =
            frameLocalPosition(frame, surface_dir * (1.01F + jitter(0.02F)));

        QVector3D const dir_local(side * (0.55F + jitter(0.12F)), jitter(0.06F),
                                  0.34F + jitter(0.08F));
        QVector3D strand_dir =
            frame.right * dir_local.x() + frame.up * dir_local.y() +
            frame.forward * dir_local.z() - surface_dir * 0.20F;
        if (strand_dir.lengthSquared() < 1e-6F) {
          continue;
        }
        strand_dir.normalize();

        float const strand_length =
            base_length_norm * fh.length * (1.0F + jitter(0.35F));
        QVector3D const tip = root + strand_dir * (head_r * strand_length);

        float const base_radius =
            std::max(head_r * 0.028F * fh.thickness, head_r * 0.0065F);
        float const mid_radius = base_radius * 0.45F;
        float const color_jitter = 0.92F + random01() * 0.18F;
        QVector3D const root_color =
            saturate(hair_root * (color_jitter * 0.95F));
        QVector3D const tip_color = saturate(hair_tip * (color_jitter * 1.02F));

        out.mesh(getUnitSphere(), sphereAt(ctx.model, root, base_radius * 0.7F),
                 root_color, nullptr, 1.0F);

        QVector3D const mid = root + (tip - root) * 0.5F;
        out.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, root, mid, base_radius * 0.85F),
                 root_color, nullptr, 1.0F);
        out.mesh(getUnitCone(), coneFromTo(ctx.model, mid, tip, mid_radius),
                 tip_color, nullptr, 1.0F);
      }
    }
  };

  switch (fh.style) {
  case FacialHairStyle::Stubble: {
    place_strands(1, 11, 2.0F, 0.12F, 0.28F, 0.30F, 0.08F, 0.035F);
    break;
  }

  case FacialHairStyle::ShortBeard: {
    place_strands(3, 14, 2.6F, 0.18F, 0.58F, 0.38F, 0.12F, 0.055F);
    break;
  }

  case FacialHairStyle::FullBeard:
  case FacialHairStyle::LongBeard: {
    bool const is_long = (fh.style == FacialHairStyle::LongBeard);
    if (is_long) {
      place_strands(5, 20, 3.0F, 0.24F, 1.00F, 0.48F, 0.18F, 0.060F);
    } else {
      place_strands(4, 18, 2.8F, 0.22F, 0.85F, 0.42F, 0.16F, 0.058F);
    }
    break;
  }

  case FacialHairStyle::Goatee: {
    place_strands(2, 8, 1.8F, 0.16F, 0.70F, 0.34F, 0.14F, 0.055F);
    break;
  }

  case FacialHairStyle::Mustache: {
    place_mustache(5, 0.32F, 0.05F);
    break;
  }

  case FacialHairStyle::MustacheAndBeard: {
    FacialHairParams mustache_only = fh;
    mustache_only.style = FacialHairStyle::Mustache;
    FacialHairParams beard_only = fh;
    beard_only.style = FacialHairStyle::ShortBeard;

    HumanoidVariant v_mustache = v;
    v_mustache.facialHair = mustache_only;
    drawFacialHair(ctx, v_mustache, pose, out);

    HumanoidVariant v_beard = v;
    v_beard.facialHair = beard_only;
    drawFacialHair(ctx, v_beard, pose, out);
    break;
  }

  case FacialHairStyle::None:
  default:
    break;
  }
}

void HumanoidRendererBase::render(const DrawContext &ctx,
                                  ISubmitter &out) const {
  FormationParams const formation = resolveFormation(ctx);
  AnimationInputs const anim = sampleAnimState(ctx);

  Engine::Core::UnitComponent *unit_comp = nullptr;
  if (ctx.entity != nullptr) {
    unit_comp = ctx.entity->getComponent<Engine::Core::UnitComponent>();
  }

  Engine::Core::MovementComponent *movement_comp = nullptr;
  Engine::Core::TransformComponent *transform_comp = nullptr;
  if (ctx.entity != nullptr) {
    movement_comp = ctx.entity->getComponent<Engine::Core::MovementComponent>();
    transform_comp =
        ctx.entity->getComponent<Engine::Core::TransformComponent>();
  }

  uint32_t seed = 0U;
  if (unit_comp != nullptr) {
    seed ^= uint32_t(unit_comp->owner_id * 2654435761U);
  }
  if (ctx.entity != nullptr) {
    seed ^= uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
  }

  const int rows =
      (formation.individuals_per_unit + formation.max_per_row - 1) /
      formation.max_per_row;
  const int cols = formation.max_per_row;

  int visible_count = rows * cols;
  if (unit_comp != nullptr) {
    int const mh = std::max(1, unit_comp->max_health);
    float const ratio = std::clamp(unit_comp->health / float(mh), 0.0F, 1.0F);
    visible_count = std::max(1, (int)std::ceil(ratio * float(rows * cols)));
  }

  HumanoidVariant variant;
  get_variant(ctx, seed, variant);

  if (!m_proportionScaleCached) {
    m_cachedProportionScale = get_proportion_scaling();
    m_proportionScaleCached = true;
  }
  const QVector3D prop_scale = m_cachedProportionScale;
  const float height_scale = prop_scale.y();
  const bool needs_height_scaling = std::abs(height_scale - 1.0F) > 0.001F;

  const QMatrix4x4 k_identity_matrix;

  auto fast_random = [](uint32_t &state) -> float {
    state = state * 1664525U + 1013904223U;
    return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
  };

  for (int idx = 0; idx < visible_count; ++idx) {
    int const r = idx / cols;
    int const c = idx % cols;

    float offset_x = (c - (cols - 1) * 0.5F) * formation.spacing;
    float offset_z = (r - (rows - 1) * 0.5F) * formation.spacing;

    uint32_t const inst_seed = seed ^ uint32_t(idx * 9176U);

    uint32_t rng_state = inst_seed;
    float const pos_jitter_x = (fast_random(rng_state) - 0.5F) * 0.05F;
    float const pos_jitter_z = (fast_random(rng_state) - 0.5F) * 0.05F;
    float const vertical_jitter = (fast_random(rng_state) - 0.5F) * 0.03F;
    float const yaw_offset = (fast_random(rng_state) - 0.5F) * 5.0F;
    float const phase_offset = fast_random(rng_state) * 0.25F;

    offset_x += pos_jitter_x;
    offset_z += pos_jitter_z;

    QMatrix4x4 inst_model;
    float applied_yaw = yaw_offset;

    if (transform_comp != nullptr) {
      applied_yaw = transform_comp->rotation.y + yaw_offset;
      QMatrix4x4 m = k_identity_matrix;
      m.translate(transform_comp->position.x, transform_comp->position.y,
                  transform_comp->position.z);
      m.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
      m.scale(transform_comp->scale.x, transform_comp->scale.y,
              transform_comp->scale.z);
      m.translate(offset_x, vertical_jitter, offset_z);
      inst_model = m;
    } else {
      inst_model = ctx.model;
      inst_model.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
      inst_model.translate(offset_x, vertical_jitter, offset_z);
    }

    DrawContext inst_ctx{ctx.resources, ctx.entity, ctx.world, inst_model};
    inst_ctx.selected = ctx.selected;
    inst_ctx.hovered = ctx.hovered;
    inst_ctx.animationTime = ctx.animationTime;
    inst_ctx.rendererId = ctx.rendererId;
    inst_ctx.backend = ctx.backend;

    VariationParams variation = VariationParams::fromSeed(inst_seed);
    adjust_variation(inst_ctx, inst_seed, variation);

    float const combined_height_scale = height_scale * variation.height_scale;
    if (needs_height_scaling ||
        std::abs(variation.height_scale - 1.0F) > 0.001F) {
      QMatrix4x4 scale_matrix;
      scale_matrix.scale(variation.bulkScale, combined_height_scale, 1.0F);
      inst_ctx.model = inst_ctx.model * scale_matrix;
    }

    HumanoidPose pose;
    computeLocomotionPose(inst_seed, anim.time + phase_offset, anim.isMoving,
                          variation, pose);

    HumanoidAnimationContext anim_ctx{};
    anim_ctx.inputs = anim;
    anim_ctx.variation = variation;
    anim_ctx.formation = formation;
    anim_ctx.jitter_seed = phase_offset;
    anim_ctx.instance_position =
        inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));

    float yaw_rad = qDegreesToRadians(applied_yaw);
    QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
    if (forward.lengthSquared() > 1e-8F) {
      forward.normalize();
    } else {
      forward = QVector3D(0.0F, 0.0F, 1.0F);
    }
    QVector3D up(0.0F, 1.0F, 0.0F);
    QVector3D right = QVector3D::crossProduct(up, forward);
    if (right.lengthSquared() > 1e-8F) {
      right.normalize();
    } else {
      right = QVector3D(1.0F, 0.0F, 0.0F);
    }

    anim_ctx.entity_forward = forward;
    anim_ctx.entity_right = right;
    anim_ctx.entity_up = up;
    anim_ctx.locomotion_direction = forward;
    anim_ctx.yaw_degrees = applied_yaw;
    anim_ctx.yaw_radians = yaw_rad;
    anim_ctx.has_movement_target = false;
    anim_ctx.move_speed = 0.0F;
    anim_ctx.movement_target = QVector3D(0.0F, 0.0F, 0.0F);

    if (movement_comp != nullptr) {
      QVector3D velocity(movement_comp->vx, 0.0F, movement_comp->vz);
      float speed = velocity.length();
      anim_ctx.move_speed = speed;
      if (speed > 1e-4F) {
        anim_ctx.locomotion_direction = velocity.normalized();
      }
      anim_ctx.has_movement_target = movement_comp->hasTarget;
      anim_ctx.movement_target =
          QVector3D(movement_comp->target_x, 0.0F, movement_comp->target_y);
    }

    anim_ctx.locomotion_velocity =
        anim_ctx.locomotion_direction * anim_ctx.move_speed;
    anim_ctx.motion_state = classifyMotionState(anim, anim_ctx.move_speed);
    anim_ctx.gait.state = anim_ctx.motion_state;
    anim_ctx.gait.speed = anim_ctx.move_speed;
    anim_ctx.gait.velocity = anim_ctx.locomotion_velocity;
    anim_ctx.gait.has_target = anim_ctx.has_movement_target;
    anim_ctx.gait.is_airborne = false;

    float reference_speed = (anim_ctx.gait.state == HumanoidMotionState::Run)
                                ? k_reference_run_speed
                                : k_reference_walk_speed;
    if (reference_speed > 0.0001F) {
      anim_ctx.gait.normalized_speed =
          std::clamp(anim_ctx.gait.speed / reference_speed, 0.0F, 1.0F);
    } else {
      anim_ctx.gait.normalized_speed = 0.0F;
    }
    if (!anim.isMoving) {
      anim_ctx.gait.normalized_speed = 0.0F;
    }

    if (anim.isMoving) {
      float stride_base = 0.8F;
      if (anim_ctx.motion_state == HumanoidMotionState::Run) {
        stride_base = 0.55F;
      }
      float const base_cycle =
          stride_base / std::max(0.1F, variation.walkSpeedMult);
      anim_ctx.locomotion_cycle_time = base_cycle;
      anim_ctx.locomotion_phase = std::fmod(
          (anim.time + phase_offset) / std::max(0.001F, base_cycle), 1.0F);
      anim_ctx.gait.cycle_time = anim_ctx.locomotion_cycle_time;
      anim_ctx.gait.cycle_phase = anim_ctx.locomotion_phase;
      anim_ctx.gait.stride_distance =
          anim_ctx.gait.speed * anim_ctx.gait.cycle_time;
    } else {
      anim_ctx.locomotion_cycle_time = 0.0F;
      anim_ctx.locomotion_phase = 0.0F;
      anim_ctx.gait.cycle_time = 0.0F;
      anim_ctx.gait.cycle_phase = 0.0F;
      anim_ctx.gait.stride_distance = 0.0F;
    }
    if (anim.is_attacking) {
      anim_ctx.attack_phase = std::fmod(anim.time, 1.0F);
    }

    customize_pose(inst_ctx, anim_ctx, inst_seed, pose);

    if (anim_ctx.motion_state == HumanoidMotionState::Run) {
      pose.pelvisPos.setZ(pose.pelvisPos.z() - 0.05F);
      pose.shoulderL.setZ(pose.shoulderL.z() + 0.10F);
      pose.shoulderR.setZ(pose.shoulderR.z() + 0.10F);
      pose.neck_base.setZ(pose.neck_base.z() + 0.06F);
      pose.headPos.setZ(pose.headPos.z() + 0.04F);
      pose.shoulderL.setY(pose.shoulderL.y() - 0.02F);
      pose.shoulderR.setY(pose.shoulderR.y() - 0.02F);
    }

    drawCommonBody(inst_ctx, variant, pose, out);
    drawFacialHair(inst_ctx, variant, pose, out);
    draw_armor(inst_ctx, variant, pose, anim_ctx, out);

    addAttachments(inst_ctx, variant, pose, anim_ctx, out);
  }
}

} // namespace Render::GL
