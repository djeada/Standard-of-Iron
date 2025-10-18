#include "humanoid_base.h"
#include "../game/core/component.h"
#include "../game/core/entity.h"
#include "../game/core/world.h"
#include "../game/units/troop_config.h"
#include "../game/visuals/team_colors.h"
#include "geom/math_utils.h"
#include "geom/selection_ring.h"
#include "geom/transforms.h"
#include "gl/mesh.h"
#include "gl/primitives.h"
#include "humanoid_math.h"
#include <QMatrix4x4>
#include <algorithm>
#include <cmath>

namespace Render::GL {

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::clampVec01;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

void HumanoidRendererBase::getVariant(const DrawContext &ctx, uint32_t seed,
                                      HumanoidVariant &v) const {
  QVector3D teamTint = resolveTeamTint(ctx);
  v.palette = makeHumanoidPalette(teamTint, seed);
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

QVector3D HumanoidRendererBase::resolveTeamTint(const DrawContext &ctx) {
  QVector3D tunic(0.8f, 0.9f, 1.0f);
  Engine::Core::UnitComponent *unit = nullptr;
  Engine::Core::RenderableComponent *rc = nullptr;

  if (ctx.entity) {
    unit = ctx.entity->getComponent<Engine::Core::UnitComponent>();
    rc = ctx.entity->getComponent<Engine::Core::RenderableComponent>();
  }

  if (unit && unit->ownerId > 0) {
    tunic = Game::Visuals::teamColorForOwner(unit->ownerId);
  } else if (rc) {
    tunic = QVector3D(rc->color[0], rc->color[1], rc->color[2]);
  }

  return tunic;
}

FormationParams HumanoidRendererBase::resolveFormation(const DrawContext &ctx) {
  FormationParams params;
  params.individualsPerUnit = 1;
  params.maxPerRow = 1;
  params.spacing = 0.75f;

  if (ctx.entity) {
    auto *unit = ctx.entity->getComponent<Engine::Core::UnitComponent>();
    if (unit && !unit->unitType.empty()) {
      params.individualsPerUnit =
          Game::Units::TroopConfig::instance().getIndividualsPerUnit(
              unit->unitType);
      params.maxPerRow = Game::Units::TroopConfig::instance().getMaxUnitsPerRow(
          unit->unitType);
    }
  }

  return params;
}

AnimationInputs HumanoidRendererBase::sampleAnimState(const DrawContext &ctx) {
  AnimationInputs anim;
  anim.time = ctx.animationTime;
  anim.isMoving = false;
  anim.isAttacking = false;
  anim.isMelee = false;
  anim.isInHoldMode = false;
  anim.isExitingHold = false;
  anim.holdExitProgress = 0.0f;

  if (!ctx.entity)
    return anim;

  auto *movement = ctx.entity->getComponent<Engine::Core::MovementComponent>();
  auto *attack = ctx.entity->getComponent<Engine::Core::AttackComponent>();
  auto *attackTarget =
      ctx.entity->getComponent<Engine::Core::AttackTargetComponent>();
  auto *transform =
      ctx.entity->getComponent<Engine::Core::TransformComponent>();
  auto *holdMode = ctx.entity->getComponent<Engine::Core::HoldModeComponent>();

  anim.isInHoldMode = (holdMode && holdMode->active);
  if (holdMode && !holdMode->active && holdMode->exitCooldown > 0.0f) {
    anim.isExitingHold = true;
    anim.holdExitProgress =
        1.0f - (holdMode->exitCooldown / holdMode->standUpDuration);
  }
  anim.isMoving = (movement && movement->hasTarget);

  if (attack && attackTarget && attackTarget->targetId > 0 && transform) {
    anim.isMelee = (attack->currentMode ==
                    Engine::Core::AttackComponent::CombatMode::Melee);

    bool stationary = !anim.isMoving;
    float currentCooldown =
        anim.isMelee ? attack->meleeCooldown : attack->cooldown;
    bool recentlyFired =
        attack->timeSinceLast < std::min(currentCooldown, 0.45f);
    bool targetInRange = false;

    if (ctx.world) {
      auto *target = ctx.world->getEntity(attackTarget->targetId);
      if (target) {
        auto *targetTransform =
            target->getComponent<Engine::Core::TransformComponent>();
        if (targetTransform) {
          float dx = targetTransform->position.x - transform->position.x;
          float dz = targetTransform->position.z - transform->position.z;
          float distSquared = dx * dx + dz * dz;
          float targetRadius = 0.0f;
          if (target->hasComponent<Engine::Core::BuildingComponent>()) {
            targetRadius =
                std::max(targetTransform->scale.x, targetTransform->scale.z) *
                0.5f;
          } else {
            targetRadius =
                std::max(targetTransform->scale.x, targetTransform->scale.z) *
                0.5f;
          }
          float effectiveRange = attack->range + targetRadius + 0.25f;
          targetInRange = (distSquared <= effectiveRange * effectiveRange);
        }
      }
    }

    anim.isAttacking = stationary && (targetInRange || recentlyFired);
  }

  return anim;
}

void HumanoidRendererBase::computeLocomotionPose(
    uint32_t seed, float time, bool isMoving, const VariationParams &variation,
    HumanoidPose &pose) {
  using HP = HumanProportions;

  float hScale = variation.heightScale;

  pose.headPos =
      QVector3D(0.0f, (HP::HEAD_TOP_Y + HP::CHIN_Y) * 0.5f * hScale, 0.0f);
  pose.headR = HP::HEAD_RADIUS * hScale;
  pose.neckBase = QVector3D(0.0f, HP::NECK_BASE_Y * hScale, 0.0f);

  float bScale = variation.bulkScale;
  float sWidth = variation.stanceWidth;

  pose.shoulderL = QVector3D(-HP::TORSO_TOP_R * 0.98f * bScale,
                             HP::SHOULDER_Y * hScale, 0.0f);
  pose.shoulderR = QVector3D(HP::TORSO_TOP_R * 0.98f * bScale,
                             HP::SHOULDER_Y * hScale, 0.0f);

  pose.footYOffset = 0.02f;
  pose.footL = QVector3D(-HP::SHOULDER_WIDTH * 0.58f * sWidth,
                         HP::GROUND_Y + pose.footYOffset, 0.0f);
  pose.footR = QVector3D(HP::SHOULDER_WIDTH * 0.58f * sWidth,
                         HP::GROUND_Y + pose.footYOffset, 0.0f);

  // Initialize pelvis at waist height by default
  pose.pelvisPos = QVector3D(0.0f, HP::WAIST_Y * hScale, 0.0f);

  // Initialize knees at default standing position
  pose.kneeL = QVector3D(pose.footL.x(), HP::KNEE_Y * hScale, pose.footL.z());
  pose.kneeR = QVector3D(pose.footR.x(), HP::KNEE_Y * hScale, pose.footR.z());

  pose.shoulderL.setY(pose.shoulderL.y() + variation.shoulderTilt);
  pose.shoulderR.setY(pose.shoulderR.y() - variation.shoulderTilt);

  float slouchOffset = variation.postureSlump * 0.15f;
  pose.shoulderL.setZ(pose.shoulderL.z() + slouchOffset);
  pose.shoulderR.setZ(pose.shoulderR.z() + slouchOffset);

  float footAngleJitter = (hash01(seed ^ 0x5678u) - 0.5f) * 0.12f;
  float footDepthJitter = (hash01(seed ^ 0x9ABCu) - 0.5f) * 0.08f;

  pose.footL.setX(pose.footL.x() + footAngleJitter);
  pose.footR.setX(pose.footR.x() - footAngleJitter);
  pose.footL.setZ(pose.footL.z() + footDepthJitter);
  pose.footR.setZ(pose.footR.z() - footDepthJitter);

  float armHeightJitter = (hash01(seed ^ 0xABCDu) - 0.5f) * 0.03f;
  float armAsymmetry = (hash01(seed ^ 0xDEF0u) - 0.5f) * 0.04f;

  pose.handL =
      QVector3D(-0.05f + armAsymmetry,
                HP::SHOULDER_Y * hScale + 0.05f + armHeightJitter, 0.55f);
  pose.handR = QVector3D(
      0.15f - armAsymmetry * 0.5f,
      HP::SHOULDER_Y * hScale + 0.15f + armHeightJitter * 0.8f, 0.20f);

  if (isMoving) {

    float walkCycleTime = 0.8f / variation.walkSpeedMult;
    float walkPhase = fmod(time * (1.0f / walkCycleTime), 1.0f);
    float leftPhase = walkPhase;
    float rightPhase = fmod(walkPhase + 0.5f, 1.0f);

    const float groundY = HP::GROUND_Y;

    const float strideLength = 0.35f * variation.armSwingAmp;

    auto animateFoot = [groundY, &pose, strideLength](QVector3D &foot,
                                                      float phase) {
      float lift = std::sin(phase * 2.0f * 3.14159f);
      if (lift > 0.0f) {
        foot.setY(groundY + pose.footYOffset + lift * 0.12f);
      } else {
        foot.setY(groundY + pose.footYOffset);
      }
      foot.setZ(foot.z() +
                std::sin((phase - 0.25f) * 2.0f * 3.14159f) * strideLength);
    };

    animateFoot(pose.footL, leftPhase);
    animateFoot(pose.footR, rightPhase);

    float hipSway =
        std::sin(walkPhase * 2.0f * 3.14159f) * 0.02f * variation.armSwingAmp;
    pose.shoulderL.setX(pose.shoulderL.x() + hipSway);
    pose.shoulderR.setX(pose.shoulderR.x() + hipSway);
  }

  QVector3D rightAxis = pose.shoulderR - pose.shoulderL;
  rightAxis.setY(0.0f);
  if (rightAxis.lengthSquared() < 1e-8f)
    rightAxis = QVector3D(1, 0, 0);
  rightAxis.normalize();
  QVector3D outwardL = -rightAxis;
  QVector3D outwardR = rightAxis;

  pose.elbowL = elbowBendTorso(pose.shoulderL, pose.handL, outwardL, 0.45f,
                               0.15f, -0.08f, +1.0f);
  pose.elbowR = elbowBendTorso(pose.shoulderR, pose.handR, outwardR, 0.48f,
                               0.12f, 0.02f, +1.0f);
}

void HumanoidRendererBase::drawCommonBody(const DrawContext &ctx,
                                          const HumanoidVariant &v,
                                          const HumanoidPose &pose,
                                          ISubmitter &out) const {
  using HP = HumanProportions;

  QVector3D scaling = getProportionScaling();
  float widthScale = scaling.x();
  float heightScale = scaling.y();
  float headScale = scaling.z();

  QVector3D rightAxis = pose.shoulderR - pose.shoulderL;
  if (rightAxis.lengthSquared() < 1e-8f)
    rightAxis = QVector3D(1, 0, 0);
  rightAxis.normalize();

  const float yShoulder = 0.5f * (pose.shoulderL.y() + pose.shoulderR.y());
  const float yNeck = pose.neckBase.y();
  const float shoulderHalfSpan =
      0.5f * std::abs(pose.shoulderR.x() - pose.shoulderL.x());
  const float torsoR =
      std::max(HP::TORSO_TOP_R * widthScale, shoulderHalfSpan * 0.95f);

  const float yTopCover = std::max(yShoulder + 0.04f, yNeck + 0.00f);

  QVector3D tunicTop{0.0f, yTopCover - 0.006f, 0.0f};
  // Use pelvisPos instead of hardcoded WAIST_Y so torso follows when kneeling
  QVector3D tunicBot{0.0f, pose.pelvisPos.y() + 0.03f, 0.0f};
  out.mesh(getUnitTorso(),
           cylinderBetween(ctx.model, tunicTop, tunicBot, torsoR),
           v.palette.cloth, nullptr, 1.0f);

  QVector3D chinPos{0.0f, HP::CHIN_Y, 0.0f};
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.neckBase, chinPos,
                           HP::NECK_RADIUS * widthScale),
           v.palette.skin * 0.9f, nullptr, 1.0f);

  out.mesh(getUnitSphere(),
           sphereAt(ctx.model, pose.headPos, pose.headR * headScale),
           v.palette.skin, nullptr, 1.0f);

  QVector3D iris(0.06f, 0.06f, 0.07f);
  float eyeZ = pose.headR * headScale * 0.7f;
  float eyeY = pose.headPos.y() + pose.headR * headScale * 0.1f;
  float eyeSpacing = pose.headR * headScale * 0.35f;
  out.mesh(getUnitSphere(),
           ctx.model * sphereAt(QVector3D(-eyeSpacing, eyeY, eyeZ),
                                pose.headR * headScale * 0.15f),
           iris, nullptr, 1.0f);
  out.mesh(getUnitSphere(),
           ctx.model * sphereAt(QVector3D(eyeSpacing, eyeY, eyeZ),
                                pose.headR * headScale * 0.15f),
           iris, nullptr, 1.0f);

  const float upperArmR = HP::UPPER_ARM_R * widthScale;
  const float foreArmR = HP::FORE_ARM_R * widthScale;
  const float jointR = HP::HAND_RADIUS * widthScale * 1.05f;
  const float handR = HP::HAND_RADIUS * widthScale * 0.95f;

  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.shoulderL, pose.elbowL, upperArmR),
           v.palette.cloth, nullptr, 1.0f);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, pose.elbowL, jointR),
           v.palette.cloth * 0.95f, nullptr, 1.0f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.elbowL, pose.handL, foreArmR),
           v.palette.skin * 0.95f, nullptr, 1.0f);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, pose.handL, handR),
           v.palette.leatherDark * 0.92f, nullptr, 1.0f);

  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.shoulderR, pose.elbowR, upperArmR),
           v.palette.cloth, nullptr, 1.0f);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, pose.elbowR, jointR),
           v.palette.cloth * 0.95f, nullptr, 1.0f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, pose.elbowR, pose.handR, foreArmR),
           v.palette.skin * 0.95f, nullptr, 1.0f);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, pose.handR, handR),
           v.palette.leatherDark * 0.92f, nullptr, 1.0f);

  const float hipHalf = HP::UPPER_LEG_R * widthScale * 1.7f;
  const float maxStance = hipHalf * 2.2f;

  const float upperScale = 1.40f * 3.0f * widthScale;
  const float lowerScale = 1.35f * 3.0f * widthScale;
  const float footLenMul = (5.5f * 0.1f);
  const float footRadMul = 0.70f;

  const float kneeForward = 0.15f;
  const float kneeDrop = 0.02f * HP::LOWER_LEG_LEN;

  const QVector3D FWD(0.f, 0.f, 1.f);

  const float upperR = HP::UPPER_LEG_R * upperScale;
  const float lowerR = HP::LOWER_LEG_R * lowerScale;
  const float footR = lowerR * footRadMul;

  constexpr float DEG = 3.1415926535f / 180.f;

  // Use pose.pelvisPos instead of hardcoded waist for proper kneeling
  const QVector3D hipL = pose.pelvisPos + QVector3D(-hipHalf, 0.f, 0.f);
  const QVector3D hipR = pose.pelvisPos + QVector3D(+hipHalf, 0.f, 0.f);
  const float midX = 0.5f * (hipL.x() + hipR.x());

  auto clampX = [&](const QVector3D &v, float mid) {
    const float dx = v.x() - mid;
    const float mag = std::min(std::abs(dx), maxStance);
    return QVector3D(mid + (dx < 0 ? -mag : mag), v.y(), v.z());
  };
  const QVector3D plantL = clampX(pose.footL, midX);
  const QVector3D plantR = clampX(pose.footR, midX);

  const float footLen = footLenMul * lowerR;
  const float heelBackFrac = 0.15f;
  const float ballFrac = 0.72f;
  const float toeUpFrac = 0.06f;
  const float yawOutDeg = 12.0f;
  const float ankleFwdFrac = 0.10f;
  const float ankleUpFrac = 0.50f;
  const float toeSplayFrac = 0.06f;

  const QVector3D fwdL = rotY(FWD, -yawOutDeg * DEG);
  const QVector3D fwdR = rotY(FWD, +yawOutDeg * DEG);
  const QVector3D rightL = rightOf(fwdL);
  const QVector3D rightR = rightOf(fwdR);

  const float footCLyL = plantL.y() + footR;
  const float footCLyR = plantR.y() + footR;

  QVector3D heelCenL(plantL.x(), footCLyL, plantL.z());
  QVector3D heelCenR(plantR.x(), footCLyR, plantR.z());

  QVector3D ankleL = heelCenL + fwdL * (ankleFwdFrac * footLen);
  QVector3D ankleR = heelCenR + fwdR * (ankleFwdFrac * footLen);
  ankleL.setY(heelCenL.y() + ankleUpFrac * footR);
  ankleR.setY(heelCenR.y() + ankleUpFrac * footR);

  const float kneeForwardPush = HP::LOWER_LEG_LEN * kneeForward;
  const float kneeDropAbs = kneeDrop;

  // Use pose's knee positions if they've been customized (e.g., for kneeling)
  // Otherwise compute default standing knees
  QVector3D kneeL, kneeR;
  bool useCustomKnees = (pose.kneeL.y() < HP::KNEE_Y * 0.9f || 
                         pose.kneeR.y() < HP::KNEE_Y * 0.9f);
  
  if (useCustomKnees) {
    kneeL = pose.kneeL;
    kneeR = pose.kneeR;
  } else {
    auto computeKnee = [&](const QVector3D &hip, const QVector3D &ankle) {
      QVector3D dir = ankle - hip;
      QVector3D knee = hip + 0.5f * dir;
      knee += QVector3D(0, 0, 1) * kneeForwardPush;
      knee.setY(knee.y() - kneeDropAbs);
      knee.setX((hip.x() + ankle.x()) * 0.5f);
      return knee;
    };
    kneeL = computeKnee(hipL, ankleL);
    kneeR = computeKnee(hipR, ankleR);
  }

  const float heelBack = heelBackFrac * footLen;
  const float ballLen = ballFrac * footLen;
  const float toeLen = (1.0f - ballFrac) * footLen;

  QVector3D ballL = heelCenL + fwdL * ballLen;
  QVector3D ballR = heelCenR + fwdR * ballLen;

  const float toeUpL = toeUpFrac * footLen;
  const float toeUpR = toeUpFrac * footLen;
  const float toeSplay = toeSplayFrac * footLen;

  QVector3D toeL = ballL + fwdL * toeLen - rightL * toeSplay;
  QVector3D toeR = ballR + fwdR * toeLen + rightR * toeSplay;
  toeL.setY(ballL.y() + toeUpL);
  toeR.setY(ballR.y() + toeUpR);

  heelCenL -= fwdL * heelBack;
  heelCenR -= fwdR * heelBack;

  const float heelRad = footR * 1.05f;
  const float toeRad = footR * 0.85f;

  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(ctx.model, hipL, kneeL, upperR),
           v.palette.leather, nullptr, 1.0f);
  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(ctx.model, hipR, kneeR, upperR),
           v.palette.leather, nullptr, 1.0f);

  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(ctx.model, kneeL, ankleL, lowerR),
           v.palette.leatherDark, nullptr, 1.0f);
  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(ctx.model, kneeR, ankleR, lowerR),
           v.palette.leatherDark, nullptr, 1.0f);

  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(ctx.model, heelCenL, ballL, heelRad),
           v.palette.leatherDark, nullptr, 1.0f);
  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(ctx.model, heelCenR, ballR, heelRad),
           v.palette.leatherDark, nullptr, 1.0f);

  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(ctx.model, ballL, toeL, toeRad),
           v.palette.leatherDark, nullptr, 1.0f);
  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(ctx.model, ballR, toeR, toeRad),
           v.palette.leatherDark, nullptr, 1.0f);

  drawHelmet(ctx, v, pose, out);

  drawArmorOverlay(ctx, v, pose, yTopCover, torsoR, shoulderHalfSpan, upperArmR,
                   rightAxis, out);

  drawShoulderDecorations(ctx, v, pose, yTopCover, yNeck, rightAxis, out);
}

void HumanoidRendererBase::drawHelmet(const DrawContext &ctx,
                                      const HumanoidVariant &v,
                                      const HumanoidPose &pose,
                                      ISubmitter &out) const {

  using HP = HumanProportions;
  QVector3D capC = pose.headPos + QVector3D(0, pose.headR * 0.8f, 0);
  out.mesh(getUnitSphere(), sphereAt(ctx.model, capC, pose.headR * 0.85f),
           v.palette.cloth * 0.9f, nullptr, 1.0f);
}

void HumanoidRendererBase::drawArmorOverlay(
    const DrawContext &ctx, const HumanoidVariant &v, const HumanoidPose &pose,
    float yTopCover, float torsoR, float shoulderHalfSpan, float upperArmR,
    const QVector3D &rightAxis, ISubmitter &out) const {}

void HumanoidRendererBase::drawShoulderDecorations(const DrawContext &ctx,
                                                   const HumanoidVariant &v,
                                                   const HumanoidPose &pose,
                                                   float yTopCover, float yNeck,
                                                   const QVector3D &rightAxis,
                                                   ISubmitter &out) const {}

void HumanoidRendererBase::drawSelectionFX(const DrawContext &ctx,
                                           ISubmitter &out) {
  if (ctx.selected || ctx.hovered) {
    float ringSize = 0.5f;
    if (ctx.entity) {
      auto *unit = ctx.entity->getComponent<Engine::Core::UnitComponent>();
      if (unit && !unit->unitType.empty()) {
        ringSize = Game::Units::TroopConfig::instance().getSelectionRingSize(
            unit->unitType);
      }
    }

    QMatrix4x4 ringM;
    QVector3D pos = ctx.model.column(3).toVector3D();
    ringM.translate(pos.x(), pos.y() + 0.05f, pos.z());
    ringM.scale(ringSize, 1.0f, ringSize);
    if (ctx.selected)
      out.selectionRing(ringM, 0.6f, 0.25f, QVector3D(0.2f, 0.4f, 1.0f));
    else
      out.selectionRing(ringM, 0.35f, 0.15f, QVector3D(0.90f, 0.90f, 0.25f));
  }
}

void HumanoidRendererBase::render(const DrawContext &ctx,
                                  ISubmitter &out) const {
  FormationParams formation = resolveFormation(ctx);
  AnimationInputs anim = sampleAnimState(ctx);

  Engine::Core::UnitComponent *unitComp = nullptr;
  if (ctx.entity) {
    unitComp = ctx.entity->getComponent<Engine::Core::UnitComponent>();
  }

  uint32_t seed = 0u;
  if (unitComp) {
    seed ^= uint32_t(unitComp->ownerId * 2654435761u);
  }
  if (ctx.entity) {
    seed ^= uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFu);
  }

  const int rows = (formation.individualsPerUnit + formation.maxPerRow - 1) /
                   formation.maxPerRow;
  const int cols = formation.maxPerRow;

  int visibleCount = rows * cols;
  if (unitComp) {
    int mh = std::max(1, unitComp->maxHealth);
    float ratio = std::clamp(unitComp->health / float(mh), 0.0f, 1.0f);
    visibleCount = std::max(1, (int)std::ceil(ratio * float(rows * cols)));
  }

  HumanoidVariant variant;
  getVariant(ctx, seed, variant);

  if (!m_proportionScaleCached) {
    m_cachedProportionScale = getProportionScaling();
    m_proportionScaleCached = true;
  }
  const QVector3D propScale = m_cachedProportionScale;
  const float heightScale = propScale.y();
  const bool needsHeightScaling = std::abs(heightScale - 1.0f) > 0.001f;

  const QMatrix4x4 kIdentityMatrix;

  auto fastRandom = [](uint32_t &state) -> float {
    state = state * 1664525u + 1013904223u;
    return float(state & 0x7FFFFFu) / float(0x7FFFFFu);
  };

  for (int idx = 0; idx < visibleCount; ++idx) {
    int r = idx / cols;
    int c = idx % cols;

    float offsetX = (c - (cols - 1) * 0.5f) * formation.spacing;
    float offsetZ = (r - (rows - 1) * 0.5f) * formation.spacing;

    uint32_t instSeed = seed ^ uint32_t(idx * 9176u);

    uint32_t rngState = instSeed;
    float posJitterX = (fastRandom(rngState) - 0.5f) * 0.05f;
    float posJitterZ = (fastRandom(rngState) - 0.5f) * 0.05f;
    float verticalJitter = (fastRandom(rngState) - 0.5f) * 0.03f;
    float yawOffset = (fastRandom(rngState) - 0.5f) * 5.0f;
    float phaseOffset = fastRandom(rngState) * 0.25f;

    offsetX += posJitterX;
    offsetZ += posJitterZ;

    QMatrix4x4 instModel;
    if (ctx.entity) {
      if (auto *entT =
              ctx.entity->getComponent<Engine::Core::TransformComponent>()) {
        QMatrix4x4 M = kIdentityMatrix;
        M.translate(entT->position.x, entT->position.y, entT->position.z);
        float baseYaw = entT->rotation.y;
        float appliedYaw = baseYaw + yawOffset;
        M.rotate(appliedYaw, 0.0f, 1.0f, 0.0f);
        M.scale(entT->scale.x, entT->scale.y, entT->scale.z);
        M.translate(offsetX, verticalJitter, offsetZ);
        instModel = M;
      } else {
        instModel = ctx.model;
        instModel.rotate(yawOffset, 0.0f, 1.0f, 0.0f);
        instModel.translate(offsetX, verticalJitter, offsetZ);
      }
    } else {
      instModel = ctx.model;
      instModel.rotate(yawOffset, 0.0f, 1.0f, 0.0f);
      instModel.translate(offsetX, verticalJitter, offsetZ);
    }

    DrawContext instCtx{ctx.resources, ctx.entity, ctx.world, instModel};

    VariationParams variation = VariationParams::fromSeed(instSeed);

    float combinedHeightScale = heightScale * variation.heightScale;
    if (needsHeightScaling || std::abs(variation.heightScale - 1.0f) > 0.001f) {
      QMatrix4x4 scaleMatrix;
      scaleMatrix.scale(variation.bulkScale, combinedHeightScale, 1.0f);
      instCtx.model = instCtx.model * scaleMatrix;
    }

    HumanoidPose pose;
    computeLocomotionPose(instSeed, anim.time + phaseOffset, anim.isMoving,
                          variation, pose);

    customizePose(instCtx, anim, instSeed, pose);

    drawCommonBody(instCtx, variant, pose, out);

    addAttachments(instCtx, variant, pose, anim, out);
  }

  drawSelectionFX(ctx, out);
}

} // namespace Render::GL
