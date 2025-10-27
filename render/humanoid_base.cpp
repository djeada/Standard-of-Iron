#include "humanoid_base.h"
#include "../game/core/component.h"
#include "../game/core/entity.h"
#include "../game/core/world.h"
#include "../game/units/troop_config.h"
#include "../game/visuals/team_colors.h"
#include "entity/registry.h"
#include "geom/math_utils.h"
#include "geom/transforms.h"
#include "gl/primitives.h"
#include "gl/render_constants.h"
#include "humanoid_math.h"
#include "humanoid_specs.h"
#include "palette.h"
#include "submitter.h"
#include "units/spawn_type.h"
#include <QMatrix4x4>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <qmatrix4x4.h>
#include <qvectornd.h>

namespace Render::GL {

using namespace Render::GL::Geometry;
using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::clampVec01;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

void HumanoidRendererBase::getVariant(const DrawContext &ctx, uint32_t seed,
                                      HumanoidVariant &v) const {
  QVector3D const team_tint = resolveTeamTint(ctx);
  v.palette = makeHumanoidPalette(team_tint, seed);
}

void HumanoidRendererBase::customizePose(const DrawContext &ctx,
                                         const AnimationInputs &anim,
                                         uint32_t seed,
                                         HumanoidPose &ioPose) const {}

void HumanoidRendererBase::addAttachments(const DrawContext &ctx,
                                          const HumanoidVariant &v,
                                          const HumanoidPose &pose,
                                          const AnimationInputs &anim,
                                          ISubmitter &out) const {}

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

  pose.shoulderL = QVector3D(-HP::TORSO_TOP_R * 0.98F * b_scale,
                             HP::SHOULDER_Y * h_scale, 0.0F);
  pose.shoulderR = QVector3D(HP::TORSO_TOP_R * 0.98F * b_scale,
                             HP::SHOULDER_Y * h_scale, 0.0F);

  pose.footYOffset = 0.02F;
  pose.footL = QVector3D(-HP::SHOULDER_WIDTH * 0.58F * s_width,
                         HP::GROUND_Y + pose.footYOffset, 0.0F);
  pose.foot_r = QVector3D(HP::SHOULDER_WIDTH * 0.58F * s_width,
                          HP::GROUND_Y + pose.footYOffset, 0.0F);

  pose.pelvisPos = QVector3D(0.0F, HP::WAIST_Y * h_scale, 0.0F);

  pose.knee_l = QVector3D(pose.footL.x(), HP::KNEE_Y * h_scale, pose.footL.z());
  pose.knee_r =
      QVector3D(pose.foot_r.x(), HP::KNEE_Y * h_scale, pose.foot_r.z());

  pose.shoulderL.setY(pose.shoulderL.y() + variation.shoulderTilt);
  pose.shoulderR.setY(pose.shoulderR.y() - variation.shoulderTilt);

  float const slouch_offset = variation.postureSlump * 0.15F;
  pose.shoulderL.setZ(pose.shoulderL.z() + slouch_offset);
  pose.shoulderR.setZ(pose.shoulderR.z() + slouch_offset);

  float const foot_angle_jitter = (hash_01(seed ^ 0x5678U) - 0.5F) * 0.12F;
  float const foot_depth_jitter = (hash_01(seed ^ 0x9ABCU) - 0.5F) * 0.08F;

  pose.footL.setX(pose.footL.x() + foot_angle_jitter);
  pose.foot_r.setX(pose.foot_r.x() - foot_angle_jitter);
  pose.footL.setZ(pose.footL.z() + foot_depth_jitter);
  pose.foot_r.setZ(pose.foot_r.z() - foot_depth_jitter);

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
                                          const HumanoidPose &pose,
                                          ISubmitter &out) const {
  using HP = HumanProportions;

  QVector3D const scaling = getProportionScaling();
  float const width_scale = scaling.x();
  float const height_scale = scaling.y();
  float const head_scale = scaling.z();

  QVector3D right_axis = pose.shoulderR - pose.shoulderL;
  if (right_axis.lengthSquared() < 1e-8F) {
    right_axis = QVector3D(1, 0, 0);
  }
  right_axis.normalize();

  const float y_shoulder = 0.5F * (pose.shoulderL.y() + pose.shoulderR.y());
  const float y_neck = pose.neck_base.y();
  const float shoulder_half_span =
      0.5F * std::abs(pose.shoulderR.x() - pose.shoulderL.x());
  const float torso_r =
      std::max(HP::TORSO_TOP_R * width_scale, shoulder_half_span * 0.95F);

  const float y_top_cover = std::max(y_shoulder + 0.04F, y_neck + 0.00F);

  QVector3D const tunic_top{0.0F, y_top_cover - 0.006F, 0.0F};

  QVector3D const tunic_bot{0.0F, pose.pelvisPos.y() + 0.03F, 0.0F};
  out.mesh(getUnitTorso(),
           cylinderBetween(ctx.model, tunic_top, tunic_bot, torso_r),
           v.palette.cloth, nullptr, 1.0F);

  QVector3D const chin_pos{0.0F, pose.headPos.y() - pose.headR, 0.0F};
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.neck_base, chin_pos,
                           HP::NECK_RADIUS * width_scale),
           v.palette.skin * 0.9F, nullptr, 1.0F);

  out.mesh(getUnitSphere(),
           sphereAt(ctx.model, pose.headPos, pose.headR * head_scale),
           v.palette.skin, nullptr, 1.0F);

  QVector3D const iris(0.06F, 0.06F, 0.07F);
  float const eyeZ = pose.headR * head_scale * 0.7F;
  float const eyeY = pose.headPos.y() + pose.headR * head_scale * 0.1F;
  float const eye_spacing = pose.headR * head_scale * 0.35F;
  out.mesh(getUnitSphere(),
           ctx.model * sphereAt(QVector3D(-eye_spacing, eyeY, eyeZ),
                                pose.headR * head_scale * 0.15F),
           iris, nullptr, 1.0F);
  out.mesh(getUnitSphere(),
           ctx.model * sphereAt(QVector3D(eye_spacing, eyeY, eyeZ),
                                pose.headR * head_scale * 0.15F),
           iris, nullptr, 1.0F);

  const float upper_arm_r = HP::UPPER_ARM_R * width_scale;
  const float fore_arm_r = HP::FORE_ARM_R * width_scale;
  const float joint_r = HP::HAND_RADIUS * width_scale * 1.05F;
  const float hand_r = HP::HAND_RADIUS * width_scale * 0.95F;

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

  const float hip_half = HP::UPPER_LEG_R * width_scale * 1.7F;
  const float max_stance = hip_half * 2.2F;

  const float upper_scale = 1.40F * 3.0F * width_scale;
  const float lower_scale = 1.35F * 3.0F * width_scale;
  const float foot_len_mul = (5.5F * 0.1F);
  const float foot_rad_mul = 0.70F;

  const float knee_forward = 0.15F;
  const float knee_drop = 0.02F * HP::LOWER_LEG_LEN;

  const QVector3D FWD(0.F, 0.F, 1.F);

  const float upper_r = HP::UPPER_LEG_R * upper_scale;
  const float lower_r = HP::LOWER_LEG_R * lower_scale;
  const float foot_r = lower_r * foot_rad_mul;

  constexpr float DEG = std::numbers::pi_v<float> / 180.F;

  const QVector3D hipL = pose.pelvisPos + QVector3D(-hip_half, 0.F, 0.F);
  const QVector3D hipR = pose.pelvisPos + QVector3D(+hip_half, 0.F, 0.F);
  const float midX = 0.5F * (hipL.x() + hipR.x());

  auto clamp_x = [&](const QVector3D &v, float mid) {
    const float dx = v.x() - mid;
    const float mag = std::min(std::abs(dx), max_stance);
    return QVector3D(mid + (dx < 0 ? -mag : mag), v.y(), v.z());
  };
  const QVector3D plant_l = clamp_x(pose.footL, midX);
  const QVector3D plant_r = clamp_x(pose.foot_r, midX);

  const float foot_len = foot_len_mul * lower_r;
  const float heel_back_frac = 0.15F;
  const float ball_frac = 0.72F;
  const float toe_up_frac = 0.06F;
  const float yaw_out_deg = 12.0F;
  const float ankle_fwd_frac = 0.10F;
  const float ankle_up_frac = 0.50F;
  const float toe_splay_frac = 0.06F;

  const QVector3D fwdL = rotY(FWD, -yaw_out_deg * DEG);
  const QVector3D fwdR = rotY(FWD, +yaw_out_deg * DEG);
  const QVector3D right_l = rightOf(fwdL);
  const QVector3D right_r = rightOf(fwdR);

  const float foot_cly_l = plant_l.y() + foot_r;
  const float foot_cly_r = plant_r.y() + foot_r;

  QVector3D heel_cen_l(plant_l.x(), foot_cly_l, plant_l.z());
  QVector3D heel_cen_r(plant_r.x(), foot_cly_r, plant_r.z());

  QVector3D ankle_l = heel_cen_l + fwdL * (ankle_fwd_frac * foot_len);
  QVector3D ankle_r = heel_cen_r + fwdR * (ankle_fwd_frac * foot_len);
  ankle_l.setY(heel_cen_l.y() + ankle_up_frac * foot_r);
  ankle_r.setY(heel_cen_r.y() + ankle_up_frac * foot_r);

  const float knee_forwardPush = HP::LOWER_LEG_LEN * knee_forward;
  const float knee_dropAbs = knee_drop;

  QVector3D knee_l;
  QVector3D knee_r;
  bool const use_custom_knees = (pose.knee_l.y() < HP::KNEE_Y * 0.9F ||
                                 pose.knee_r.y() < HP::KNEE_Y * 0.9F);

  if (use_custom_knees) {
    knee_l = pose.knee_l;
    knee_r = pose.knee_r;
  } else {
    auto compute_knee = [&](const QVector3D &hip, const QVector3D &ankle) {
      QVector3D const dir = ankle - hip;
      QVector3D knee = hip + 0.5F * dir;
      knee += QVector3D(0, 0, 1) * knee_forwardPush;
      knee.setY(knee.y() - knee_dropAbs);
      knee.setX((hip.x() + ankle.x()) * 0.5F);
      return knee;
    };
    knee_l = compute_knee(hipL, ankle_l);
    knee_r = compute_knee(hipR, ankle_r);
  }

  const float heel_back = heel_back_frac * foot_len;
  const float ball_len = ball_frac * foot_len;
  const float toe_len = (1.0F - ball_frac) * foot_len;

  QVector3D const ball_l = heel_cen_l + fwdL * ball_len;
  QVector3D const ball_r = heel_cen_r + fwdR * ball_len;

  const float toe_up_l = toe_up_frac * foot_len;
  const float toe_up_r = toe_up_frac * foot_len;
  const float toe_splay = toe_splay_frac * foot_len;

  QVector3D toeL = ball_l + fwdL * toe_len - right_l * toe_splay;
  QVector3D toeR = ball_r + fwdR * toe_len + right_r * toe_splay;
  toeL.setY(ball_l.y() + toe_up_l);
  toeR.setY(ball_r.y() + toe_up_r);

  heel_cen_l -= fwdL * heel_back;
  heel_cen_r -= fwdR * heel_back;

  const float heel_rad = foot_r * 1.05F;
  const float toe_rad = foot_r * 0.85F;

  out.mesh(getUnitCapsule(DefaultCapsuleSegments, 1),
           Render::Geom::capsuleBetween(ctx.model, hipL, knee_l, upper_r),
           v.palette.leather, nullptr, 1.0F);
  out.mesh(getUnitCapsule(DefaultCapsuleSegments, 1),
           Render::Geom::capsuleBetween(ctx.model, hipR, knee_r, upper_r),
           v.palette.leather, nullptr, 1.0F);

  out.mesh(getUnitCapsule(DefaultCapsuleSegments, 1),
           Render::Geom::capsuleBetween(ctx.model, knee_l, ankle_l, lower_r),
           v.palette.leatherDark, nullptr, 1.0F);
  out.mesh(getUnitCapsule(DefaultCapsuleSegments, 1),
           Render::Geom::capsuleBetween(ctx.model, knee_r, ankle_r, lower_r),
           v.palette.leatherDark, nullptr, 1.0F);

  out.mesh(
      getUnitCapsule(DefaultCapsuleSegments, 1),
      Render::Geom::capsuleBetween(ctx.model, heel_cen_l, ball_l, heel_rad),
      v.palette.leatherDark, nullptr, 1.0F);
  out.mesh(
      getUnitCapsule(DefaultCapsuleSegments, 1),
      Render::Geom::capsuleBetween(ctx.model, heel_cen_r, ball_r, heel_rad),
      v.palette.leatherDark, nullptr, 1.0F);

  out.mesh(getUnitCapsule(DefaultCapsuleSegments, 1),
           Render::Geom::capsuleBetween(ctx.model, ball_l, toeL, toe_rad),
           v.palette.leatherDark, nullptr, 1.0F);
  out.mesh(getUnitCapsule(DefaultCapsuleSegments, 1),
           Render::Geom::capsuleBetween(ctx.model, ball_r, toeR, toe_rad),
           v.palette.leatherDark, nullptr, 1.0F);

  drawHelmet(ctx, v, pose, out);

  draw_armorOverlay(ctx, v, pose, y_top_cover, torso_r, shoulder_half_span,
                    upper_arm_r, right_axis, out);

  drawShoulderDecorations(ctx, v, pose, y_top_cover, y_neck, right_axis, out);
}

void HumanoidRendererBase::drawHelmet(const DrawContext &ctx,
                                      const HumanoidVariant &v,
                                      const HumanoidPose &pose,
                                      ISubmitter &out) const {

  using HP = HumanProportions;
  QVector3D const capC = pose.headPos + QVector3D(0, pose.headR * 0.8F, 0);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, capC, pose.headR * 0.85F),
           v.palette.cloth * 0.9F, nullptr, 1.0F);
}

void HumanoidRendererBase::draw_armorOverlay(
    const DrawContext &ctx, const HumanoidVariant &v, const HumanoidPose &pose,
    float y_top_cover, float torso_r, float shoulder_half_span,
    float upper_arm_r, const QVector3D &right_axis, ISubmitter &out) const {}

void HumanoidRendererBase::drawShoulderDecorations(
    const DrawContext &ctx, const HumanoidVariant &v, const HumanoidPose &pose,
    float y_top_cover, float y_neck, const QVector3D &right_axis,
    ISubmitter &out) const {}

void HumanoidRendererBase::render(const DrawContext &ctx,
                                  ISubmitter &out) const {
  FormationParams const formation = resolveFormation(ctx);
  AnimationInputs const anim = sampleAnimState(ctx);

  Engine::Core::UnitComponent *unit_comp = nullptr;
  if (ctx.entity != nullptr) {
    unit_comp = ctx.entity->getComponent<Engine::Core::UnitComponent>();
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
  getVariant(ctx, seed, variant);

  if (!m_proportionScaleCached) {
    m_cachedProportionScale = getProportionScaling();
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
    if (ctx.entity != nullptr) {
      if (auto *entT =
              ctx.entity->getComponent<Engine::Core::TransformComponent>()) {
        QMatrix4x4 M = k_identity_matrix;
        M.translate(entT->position.x, entT->position.y, entT->position.z);
        float const base_yaw = entT->rotation.y;
        float const applied_yaw = base_yaw + yaw_offset;
        M.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
        M.scale(entT->scale.x, entT->scale.y, entT->scale.z);
        M.translate(offset_x, vertical_jitter, offset_z);
        inst_model = M;
      } else {
        inst_model = ctx.model;
        inst_model.rotate(yaw_offset, 0.0F, 1.0F, 0.0F);
        inst_model.translate(offset_x, vertical_jitter, offset_z);
      }
    } else {
      inst_model = ctx.model;
      inst_model.rotate(yaw_offset, 0.0F, 1.0F, 0.0F);
      inst_model.translate(offset_x, vertical_jitter, offset_z);
    }

    DrawContext inst_ctx{ctx.resources, ctx.entity, ctx.world, inst_model};

    VariationParams const variation = VariationParams::fromSeed(inst_seed);

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

    customizePose(inst_ctx, anim, inst_seed, pose);

    drawCommonBody(inst_ctx, variant, pose, out);

    addAttachments(inst_ctx, variant, pose, anim, out);
  }
}

} // namespace Render::GL
