#include "archer_renderer.h"
#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../geom/math_utils.h"
#include "../geom/selection_disc.h"
#include "../geom/selection_ring.h"
#include "../geom/transforms.h"
#include "../gl/mesh.h"
#include "../gl/primitives.h"
#include "../gl/resources.h"
#include "../gl/texture.h"
#include "registry.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Render::GL {

namespace {
const QMatrix4x4 kIdentityMatrix;
}

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::clampVec01;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

struct HumanProportions {

  static constexpr float TOTAL_HEIGHT = 1.80f;
  static constexpr float HEAD_HEIGHT = 0.23f;

  static constexpr float GROUND_Y = 0.0f;
  static constexpr float HEAD_TOP_Y = GROUND_Y + TOTAL_HEIGHT;
  static constexpr float CHIN_Y = HEAD_TOP_Y - HEAD_HEIGHT;
  static constexpr float NECK_BASE_Y = CHIN_Y - 0.08f;
  static constexpr float SHOULDER_Y = NECK_BASE_Y - 0.12f;
  static constexpr float CHEST_Y = SHOULDER_Y - 0.42f;
  static constexpr float WAIST_Y = CHEST_Y - 0.30f;

  static constexpr float UPPER_LEG_LEN = 0.35f;
  static constexpr float LOWER_LEG_LEN = 0.35f;
  static constexpr float KNEE_Y = WAIST_Y - UPPER_LEG_LEN;

  static constexpr float SHOULDER_WIDTH = HEAD_HEIGHT * 1.85f;
  static constexpr float HEAD_RADIUS = HEAD_HEIGHT * 0.42f;
  static constexpr float NECK_RADIUS = HEAD_RADIUS * 0.38f;
  static constexpr float TORSO_TOP_R = HEAD_RADIUS * 1.15f;
  static constexpr float TORSO_BOT_R = HEAD_RADIUS * 1.05f;
  static constexpr float UPPER_ARM_R = HEAD_RADIUS * 0.38f;
  static constexpr float FORE_ARM_R = HEAD_RADIUS * 0.30f;
  static constexpr float HAND_RADIUS = HEAD_RADIUS * 0.28f;
  static constexpr float UPPER_LEG_R = HEAD_RADIUS * 0.50f;
  static constexpr float LOWER_LEG_R = HEAD_RADIUS * 0.42f;

  static constexpr float UPPER_ARM_LEN = 0.28f;
  static constexpr float FORE_ARM_LEN = 0.30f;
};

enum class MaterialType : uint8_t {
  Cloth = 0,
  Leather = 1,
  Metal = 2,
  Wood = 3,
  Skin = 4
};

struct ArcherColors {
  QVector3D tunic, skin, leather, leatherDark, wood, metal, metalHead,
      stringCol, fletch;
};

struct ArcherPose {
  using P = HumanProportions;

  QVector3D headPos{0.0f, (P::HEAD_TOP_Y + P::CHIN_Y) * 0.5f, 0.0f};
  float headR = P::HEAD_RADIUS;
  QVector3D neckBase{0.0f, P::NECK_BASE_Y, 0.0f};

  QVector3D shoulderL{-P::TORSO_TOP_R * 0.98f, P::SHOULDER_Y, 0.0f};
  QVector3D shoulderR{P::TORSO_TOP_R * 0.98f, P::SHOULDER_Y, 0.0f};

  QVector3D elbowL, elbowR;
  QVector3D handL, handR;

  float footYOffset = 0.02f;
  QVector3D footL{-P::SHOULDER_WIDTH * 0.58f, P::GROUND_Y + footYOffset, 0.0f};
  QVector3D footR{P::SHOULDER_WIDTH * 0.58f, P::GROUND_Y + footYOffset, 0.0f};

  float bowX = 0.0f;
  float bowTopY = P::SHOULDER_Y + 0.55f;
  float bowBotY = P::WAIST_Y - 0.25f;
  float bowRodR = 0.035f;
  float stringR = 0.008f;
  float bowDepth = 0.25f;
};

static inline ArcherPose makePose(uint32_t seed, float animTime, bool isMoving,
                                  bool isAttacking, bool isMelee = false) {
  ArcherPose P;

  using HP = HumanProportions;

  auto hash01 = [](uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (x & 0x00FFFFFF) / float(0x01000000);
  };

  float footAngleJitter = (hash01(seed ^ 0x5678u) - 0.5f) * 0.12f;
  float footDepthJitter = (hash01(seed ^ 0x9ABCu) - 0.5f) * 0.08f;
  float armHeightJitter = (hash01(seed ^ 0xABCDu) - 0.5f) * 0.03f;
  float armAsymmetry = (hash01(seed ^ 0xDEF0u) - 0.5f) * 0.04f;
  float shoulderRotation = (hash01(seed ^ 0x1234u) - 0.5f) * 0.05f;

  P.footL.setX(P.footL.x() + footAngleJitter);
  P.footR.setX(P.footR.x() - footAngleJitter);
  P.footL.setZ(P.footL.z() + footDepthJitter);
  P.footR.setZ(P.footR.z() - footDepthJitter);

  P.shoulderL.setY(P.shoulderL.y() + shoulderRotation);
  P.shoulderR.setY(P.shoulderR.y() - shoulderRotation);

  P.handL = QVector3D(P.bowX - 0.05f + armAsymmetry,
                      HP::SHOULDER_Y + 0.05f + armHeightJitter, 0.55f);
  P.handR = QVector3D(0.15f - armAsymmetry * 0.5f,
                      HP::SHOULDER_Y + 0.15f + armHeightJitter * 0.8f, 0.20f);

  if (isAttacking) {
    float attackCycleTime = 1.2f;
    float attackPhase = fmod(animTime * (1.0f / attackCycleTime), 1.0f);

    if (isMelee) {

      QVector3D restPos(0.25f, HP::SHOULDER_Y, 0.10f);
      QVector3D raisedPos(0.30f, HP::HEAD_TOP_Y + 0.2f, -0.05f);
      QVector3D strikePos(0.35f, HP::WAIST_Y, 0.45f);

      if (attackPhase < 0.25f) {
        float t = attackPhase / 0.25f;
        t = t * t;
        P.handR = restPos * (1.0f - t) + raisedPos * t;
        P.handL = QVector3D(-0.15f, HP::SHOULDER_Y - 0.1f * t, 0.20f);
      } else if (attackPhase < 0.35f) {
        P.handR = raisedPos;
        P.handL = QVector3D(-0.15f, HP::SHOULDER_Y - 0.1f, 0.20f);
      } else if (attackPhase < 0.55f) {
        float t = (attackPhase - 0.35f) / 0.2f;
        t = t * t * t;
        P.handR = raisedPos * (1.0f - t) + strikePos * t;
        P.handL = QVector3D(-0.15f, HP::SHOULDER_Y - 0.1f * (1.0f - t * 0.5f),
                            0.20f + 0.15f * t);
      } else {
        float t = (attackPhase - 0.55f) / 0.45f;
        t = 1.0f - (1.0f - t) * (1.0f - t);
        P.handR = strikePos * (1.0f - t) + restPos * t;
        P.handL = QVector3D(-0.15f, HP::SHOULDER_Y - 0.05f * (1.0f - t),
                            0.35f * (1.0f - t) + 0.20f * t);
      }
    } else {

      QVector3D restPos(0.15f, HP::SHOULDER_Y + 0.15f, 0.20f);
      QVector3D drawPos(0.35f, HP::SHOULDER_Y + 0.08f, -0.15f);

      if (attackPhase < 0.3f) {
        float t = attackPhase / 0.3f;
        t = t * t;
        P.handR = restPos * (1.0f - t) + drawPos * t;
      } else if (attackPhase < 0.6f) {
        P.handR = drawPos;
      } else {
        float t = (attackPhase - 0.6f) / 0.4f;
        t = 1.0f - (1.0f - t) * (1.0f - t);
        P.handR = drawPos * (1.0f - t) + restPos * t;
      }
    }
  }

  if (isMoving) {
    float walkCycleTime = 0.8f;
    float walkPhase = fmod(animTime * (1.0f / walkCycleTime), 1.0f);
    float leftPhase = walkPhase;
    float rightPhase = fmod(walkPhase + 0.5f, 1.0f);

    const float footYOffset = P.footYOffset;
    const float groundY = HP::GROUND_Y;
    const float strideLength = 0.35f;

    auto animateFoot = [groundY, footYOffset, strideLength](QVector3D &foot,
                                                             float phase) {
      float lift = std::sin(phase * 2.0f * 3.14159f);
      if (lift > 0.0f) {
        foot.setY(groundY + footYOffset + lift * 0.12f);
      } else {
        foot.setY(groundY + footYOffset);
      }
      foot.setZ(foot.z() + std::sin((phase - 0.25f) * 2.0f * 3.14159f) *
                               strideLength);
    };

    animateFoot(P.footL, leftPhase);
    animateFoot(P.footR, rightPhase);

    float hipSway = std::sin(walkPhase * 2.0f * 3.14159f) * 0.02f;
    P.shoulderL.setX(P.shoulderL.x() + hipSway);
    P.shoulderR.setX(P.shoulderR.x() + hipSway);
  }

  QVector3D rightAxis = P.shoulderR - P.shoulderL;
  rightAxis.setY(0.0f);
  if (rightAxis.lengthSquared() < 1e-8f)
    rightAxis = QVector3D(1, 0, 0);
  rightAxis.normalize();
  QVector3D outwardL = -rightAxis;
  QVector3D outwardR = rightAxis;

  auto elbowBendTorso = [&](const QVector3D &shoulder, const QVector3D &hand,
                            const QVector3D &outwardDir, float alongFrac,
                            float lateralOffset, float yBias,
                            float outwardSign) {
    QVector3D dir = hand - shoulder;
    float dist = std::max(dir.length(), 1e-5f);
    dir /= dist;

    QVector3D lateral =
        outwardDir - dir * QVector3D::dotProduct(outwardDir, dir);
    if (lateral.lengthSquared() < 1e-8f) {
      lateral = QVector3D::crossProduct(dir, QVector3D(0, 1, 0));
    }
    if (QVector3D::dotProduct(lateral, outwardDir) < 0.0f)
      lateral = -lateral;
    lateral.normalize();

    return shoulder + dir * (dist * alongFrac) +
           lateral * (lateralOffset * outwardSign) + QVector3D(0, yBias, 0);
  };

  P.elbowL = elbowBendTorso(P.shoulderL, P.handL, outwardL, 0.45f, 0.15f,
                            -0.08f, +1.0f);
  P.elbowR = elbowBendTorso(P.shoulderR, P.handR, outwardR, 0.48f, 0.12f, 0.02f,
                            +1.0f);

  return P;
}

static inline ArcherColors makeColors(const QVector3D &teamTint,
                                      uint32_t seed) {
  ArcherColors C;

  auto hash01 = [](uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (x & 0x00FFFFFF) / float(0x01000000);
  };

  auto tint = [&](float k) {
    return QVector3D(clamp01(teamTint.x() * k), clamp01(teamTint.y() * k),
                     clamp01(teamTint.z() * k));
  };

  float variation = (hash01(seed) - 0.5f) * 0.08f;

  C.tunic = clampVec01(teamTint * (1.0f + variation));
  C.skin = QVector3D(0.96f, 0.80f, 0.69f);

  float leatherVar = (hash01(seed ^ 0x1234u) - 0.5f) * 0.06f;
  float r = teamTint.x();
  float g = teamTint.y();
  float b = teamTint.z();
  float saturation = 0.6f;
  float brightness = 0.5f;
  QVector3D desaturated(r * saturation + (1.0f - saturation) * brightness,
                        g * saturation + (1.0f - saturation) * brightness,
                        b * saturation + (1.0f - saturation) * brightness);
  C.leather = clampVec01(desaturated * (0.7f + leatherVar));
  C.leatherDark = C.leather * 0.85f;

  C.wood = QVector3D(0.16f, 0.10f, 0.05f);

  QVector3D neutralGray(0.70f, 0.70f, 0.70f);
  C.metal = clampVec01(teamTint * 0.25f + neutralGray * 0.75f);
  C.metalHead = clampVec01(C.metal * 1.15f);
  C.stringCol = QVector3D(0.30f, 0.30f, 0.32f);
  C.fletch = tint(0.9f);
  return C;
}

static inline void drawTorso(const DrawContext &p, ISubmitter &out,
                             const ArcherColors &C, const ArcherPose &P) {
  using HP = HumanProportions;

  QVector3D torsoTop{0.0f, HP::NECK_BASE_Y - 0.05f, 0.0f};
  QVector3D torsoBot{0.0f, HP::WAIST_Y, 0.0f};

  float torsoRadius = HP::TORSO_TOP_R;

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, torsoTop, torsoBot, torsoRadius), C.tunic,
           nullptr, 1.0f);
}

static inline void drawTunicSkirt(const DrawContext &p, ISubmitter &out,
                                  const ArcherColors &C, const ArcherPose &P,
                                  float animTime, bool isMoving, uint32_t seed) {
  using HP = HumanProportions;

  auto hash01 = [](uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (x & 0x00FFFFFF) / float(0x01000000);
  };

  const float waistY = HP::WAIST_Y;
  const float skirtLength = 0.25f;
  const float hemY = waistY - skirtLength;
  const float waistRadius = HP::TORSO_BOT_R * 1.05f;
  const float hemRadius = HP::TORSO_BOT_R * 1.35f;

  const int segments = 12;
  const float windStrength = 0.18f;
  const float swayAmount = 0.05f;
  const float stiffness = 4.0f;
  const float damping = 0.4f;

  float characterVelX = isMoving ? std::sin(animTime * 3.0f) * 0.5f : 0.0f;
  float characterVelZ = isMoving ? std::cos(animTime * 3.0f) * 0.5f : 0.0f;

  float windPhase = animTime * 2.0f;
  float windX = std::sin(windPhase) * windStrength;
  float windZ = std::cos(windPhase * 0.7f) * windStrength;

  float swingX = windX - characterVelX * 0.8f;
  float swingZ = windZ - characterVelZ * 0.8f;
  float swingMag = std::sqrt(swingX * swingX + swingZ * swingZ) + 1e-5f;
  swingX /= swingMag;
  swingZ /= swingMag;

  const float legPushRadius = HP::UPPER_LEG_R * 1.6f;

  for (int i = 0; i < segments; ++i) {
    float angle1 = (i / float(segments)) * 2.0f * 3.14159f;
    float angle2 = ((i + 1) / float(segments)) * 2.0f * 3.14159f;

    float randomSeed = hash01(seed ^ (i * 7919u));
    float flutter = std::sin(animTime * 10.0f + randomSeed * 6.28f) * 0.02f;

    float phase = animTime * stiffness + randomSeed * 6.28f;
    float lag = std::exp(-damping) * std::sin(phase);

    float offsetX = (swingX * swayAmount + flutter) * lag;
    float offsetZ = (swingZ * swayAmount + flutter) * lag;

    QVector3D waist1(std::cos(angle1) * waistRadius, waistY,
                     std::sin(angle1) * waistRadius);
    QVector3D waist2(std::cos(angle2) * waistRadius, waistY,
                     std::sin(angle2) * waistRadius);

    QVector3D hem1Base(std::cos(angle1) * hemRadius, hemY,
                       std::sin(angle1) * hemRadius);
    QVector3D hem2Base(std::cos(angle2) * hemRadius, hemY,
                       std::sin(angle2) * hemRadius);

    QVector3D hem1 = hem1Base + QVector3D(offsetX, 0.0f, offsetZ);
    QVector3D hem2 = hem2Base + QVector3D(offsetX, 0.0f, offsetZ);

    float distToLegL1 = std::sqrt(std::pow(hem1.x() - P.footL.x(), 2) +
                                  std::pow(hem1.z() - P.footL.z(), 2));
    if (distToLegL1 < legPushRadius) {
      QVector3D pushDir =
          (hem1 - QVector3D(P.footL.x(), hem1.y(), P.footL.z())).normalized();
      hem1 += pushDir * (legPushRadius - distToLegL1);
    }

    float distToLegR1 = std::sqrt(std::pow(hem1.x() - P.footR.x(), 2) +
                                  std::pow(hem1.z() - P.footR.z(), 2));
    if (distToLegR1 < legPushRadius) {
      QVector3D pushDir =
          (hem1 - QVector3D(P.footR.x(), hem1.y(), P.footR.z())).normalized();
      hem1 += pushDir * (legPushRadius - distToLegR1);
    }

    float distToLegL2 = std::sqrt(std::pow(hem2.x() - P.footL.x(), 2) +
                                  std::pow(hem2.z() - P.footL.z(), 2));
    if (distToLegL2 < legPushRadius) {
      QVector3D pushDir =
          (hem2 - QVector3D(P.footL.x(), hem2.y(), P.footL.z())).normalized();
      hem2 += pushDir * (legPushRadius - distToLegL2);
    }

    float distToLegR2 = std::sqrt(std::pow(hem2.x() - P.footR.x(), 2) +
                                  std::pow(hem2.z() - P.footR.z(), 2));
    if (distToLegR2 < legPushRadius) {
      QVector3D pushDir =
          (hem2 - QVector3D(P.footR.x(), hem2.y(), P.footR.z())).normalized();
      hem2 += pushDir * (legPushRadius - distToLegR2);
    }

    out.mesh(getUnitCylinder(),
             Render::Geom::capsuleBetween(p.model, waist1, hem1, 0.012f),
             C.tunic * 0.92f, nullptr, 1.0f);
    out.mesh(getUnitCylinder(),
             Render::Geom::capsuleBetween(p.model, waist2, hem2, 0.012f),
             C.tunic * 0.92f, nullptr, 1.0f);
    out.mesh(getUnitCylinder(),
             Render::Geom::capsuleBetween(p.model, hem1, hem2, 0.015f),
             C.tunic * 0.88f, nullptr, 1.0f);

    QVector3D waist1Inner =
        waist1 - (waist1.normalized() * 0.008f) + QVector3D(0, 0.005f, 0);
    QVector3D waist2Inner =
        waist2 - (waist2.normalized() * 0.008f) + QVector3D(0, 0.005f, 0);
    QVector3D hem1Inner =
        hem1 - ((hem1 - QVector3D(0, hem1.y(), 0)).normalized() * 0.01f) +
        QVector3D(0, 0.002f, 0);
    QVector3D hem2Inner =
        hem2 - ((hem2 - QVector3D(0, hem2.y(), 0)).normalized() * 0.01f) +
        QVector3D(0, 0.002f, 0);

    out.mesh(getUnitCylinder(),
             Render::Geom::capsuleBetween(p.model, waist1Inner, hem1Inner,
                                          0.011f),
             C.tunic * 0.78f, nullptr, 0.85f);
    out.mesh(getUnitCylinder(),
             Render::Geom::capsuleBetween(p.model, waist2Inner, hem2Inner,
                                          0.011f),
             C.tunic * 0.78f, nullptr, 0.85f);
  }
}

static inline void drawHeadAndNeck(const DrawContext &p, ISubmitter &out,
                                   const ArcherPose &P, const ArcherColors &C) {
  using HP = HumanProportions;

  QVector3D chinPos{0.0f, HP::CHIN_Y, 0.0f};
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.neckBase, chinPos, HP::NECK_RADIUS),
           C.skin * 0.9f, nullptr, 1.0f);

  out.mesh(getUnitSphere(), sphereAt(p.model, P.headPos, P.headR), C.skin,
           nullptr, 1.0f);

  float headTopOffset = P.headR * 0.7f;
  QVector3D helmBase = P.headPos + QVector3D(0.0f, headTopOffset, 0.0f);
  QVector3D helmApex = P.headPos + QVector3D(0.0f, P.headR * 2.4f, 0.0f);
  float helmBaseR = P.headR * 1.45f;
  out.mesh(getUnitCone(), coneFromTo(p.model, helmBase, helmApex, helmBaseR),
           C.tunic, nullptr, 1.0f);

  QVector3D iris(0.06f, 0.06f, 0.07f);
  float eyeZ = P.headR * 0.7f;
  float eyeY = P.headPos.y() + P.headR * 0.1f;
  float eyeSpacing = P.headR * 0.35f;
  out.mesh(getUnitSphere(),
           p.model *
               sphereAt(QVector3D(-eyeSpacing, eyeY, eyeZ), P.headR * 0.15f),
           iris, nullptr, 1.0f);
  out.mesh(getUnitSphere(),
           p.model *
               sphereAt(QVector3D(eyeSpacing, eyeY, eyeZ), P.headR * 0.15f),
           iris, nullptr, 1.0f);
}

static inline void drawArms(const DrawContext &p, ISubmitter &out,
                            const ArcherPose &P, const ArcherColors &C) {
  using HP = HumanProportions;

  const float upperArmR = HP::UPPER_ARM_R;
  const float foreArmR = HP::FORE_ARM_R;
  const float jointR = HP::HAND_RADIUS * 1.05f;
  const float handR = HP::HAND_RADIUS * 0.95f;

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.shoulderL, P.elbowL, upperArmR), C.tunic,
           nullptr, 1.0f);
  out.mesh(getUnitSphere(), sphereAt(p.model, P.elbowL, jointR),
           C.tunic * 0.95f, nullptr, 1.0f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.elbowL, P.handL, foreArmR),
           C.skin * 0.95f, nullptr, 1.0f);
  out.mesh(getUnitSphere(), sphereAt(p.model, P.handL, handR),
           C.leatherDark * 0.92f, nullptr, 1.0f);

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.shoulderR, P.elbowR, upperArmR), C.tunic,
           nullptr, 1.0f);
  out.mesh(getUnitSphere(), sphereAt(p.model, P.elbowR, jointR),
           C.tunic * 0.95f, nullptr, 1.0f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.elbowR, P.handR, foreArmR),
           C.skin * 0.95f, nullptr, 1.0f);
  out.mesh(getUnitSphere(), sphereAt(p.model, P.handR, handR),
           C.leatherDark * 0.92f, nullptr, 1.0f);
}

static inline void drawLegs(const DrawContext &p, ISubmitter &out,
                            const ArcherPose &P, const ArcherColors &C) {
  using HP = HumanProportions;

  const float hipHalf = HP::UPPER_LEG_R * 1.7f;
  const float maxStance = hipHalf * 2.2f;

  const float upperScale = 1.40f * 3.0f;
  const float lowerScale = 1.35f * 3.0f;
  const float footLenMul = (5.5f * 0.1f);
  const float footRadMul = 0.70f;

  const float kneeForward = 0.15f;
  const float kneeDrop = 0.02f * HP::LOWER_LEG_LEN;

  const QVector3D FWD(0.f, 0.f, 1.f);
  const QVector3D UP(0.f, 1.f, 0.f);

  const float upperR = HP::UPPER_LEG_R * upperScale;
  const float lowerR = HP::LOWER_LEG_R * lowerScale;
  const float footR = lowerR * footRadMul;

  auto rotY = [](const QVector3D &v, float aRad) {
    const float c = std::cos(aRad), s = std::sin(aRad);
    return QVector3D(c * v.x() + s * v.z(), v.y(), -s * v.x() + c * v.z());
  };
  auto rightOf = [&](const QVector3D &fwd) {
    return QVector3D::crossProduct(UP, fwd).normalized();
  };
  constexpr float DEG = 3.1415926535f / 180.f;

  const QVector3D waist(0.f, HP::WAIST_Y, 0.f);
  const QVector3D hipL = waist + QVector3D(-hipHalf, 0.f, 0.f);
  const QVector3D hipR = waist + QVector3D(+hipHalf, 0.f, 0.f);
  const float midX = 0.5f * (hipL.x() + hipR.x());

  auto clampX = [&](const QVector3D &v, float mid) {
    const float dx = v.x() - mid;
    const float mag = std::min(std::abs(dx), maxStance);
    return QVector3D(mid + (dx < 0 ? -mag : mag), v.y(), v.z());
  };
  const QVector3D plantL = clampX(P.footL, midX);
  const QVector3D plantR = clampX(P.footR, midX);

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

  auto computeKnee = [&](const QVector3D &hip, const QVector3D &ankle) {
    QVector3D dir = ankle - hip;
    QVector3D knee = hip + 0.5f * dir;
    knee += QVector3D(0, 0, 1) * kneeForwardPush;
    knee.setY(knee.y() - kneeDropAbs);
    knee.setX((hip.x() + ankle.x()) * 0.5f);
    return knee;
  };

  QVector3D kneeL = computeKnee(hipL, ankleL);
  QVector3D kneeR = computeKnee(hipR, ankleR);

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
           Render::Geom::capsuleBetween(p.model, hipL, kneeL, upperR),
           C.leather, nullptr, 1.0f);
  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(p.model, hipR, kneeR, upperR),
           C.leather, nullptr, 1.0f);

  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(p.model, kneeL, ankleL, lowerR),
           C.leatherDark, nullptr, 1.0f);
  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(p.model, kneeR, ankleR, lowerR),
           C.leatherDark, nullptr, 1.0f);

  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(p.model, heelCenL, ballL, heelRad),
           C.leatherDark, nullptr, 1.0f);
  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(p.model, heelCenR, ballR, heelRad),
           C.leatherDark, nullptr, 1.0f);

  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(p.model, ballL, toeL, toeRad),
           C.leatherDark, nullptr, 1.0f);
  out.mesh(getUnitCapsule(8, 1),
           Render::Geom::capsuleBetween(p.model, ballR, toeR, toeRad),
           C.leatherDark, nullptr, 1.0f);
}

static inline void drawQuiver(const DrawContext &p, ISubmitter &out,
                              const ArcherColors &C, const ArcherPose &P,
                              uint32_t seed) {
  using HP = HumanProportions;

  auto hash01 = [](uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (x & 0x00FFFFFF) / float(0x01000000);
  };

  QVector3D qTop(-0.08f, HP::SHOULDER_Y + 0.10f, -0.25f);
  QVector3D qBase(-0.10f, HP::CHEST_Y, -0.22f);

  float quiverR = HP::HEAD_RADIUS * 0.45f;
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, qBase, qTop, quiverR),
           C.leather, nullptr, 1.0f);

  float j = (hash01(seed) - 0.5f) * 0.04f;
  float k = (hash01(seed ^ 0x9E3779B9u) - 0.5f) * 0.04f;

  QVector3D a1 = qTop + QVector3D(0.00f + j, 0.08f, 0.00f + k);
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, qTop, a1, 0.010f),
           C.wood, nullptr, 1.0f);
  out.mesh(getUnitCone(),
           coneFromTo(p.model, a1, a1 + QVector3D(0, 0.05f, 0), 0.025f),
           C.fletch, nullptr, 1.0f);

  QVector3D a2 = qTop + QVector3D(0.02f - j, 0.07f, 0.02f - k);
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, qTop, a2, 0.010f),
           C.wood, nullptr, 1.0f);
  out.mesh(getUnitCone(),
           coneFromTo(p.model, a2, a2 + QVector3D(0, 0.05f, 0), 0.025f),
           C.fletch, nullptr, 1.0f);
}

static inline void drawBowAndArrow(const DrawContext &p, ISubmitter &out,
                                   const ArcherPose &P, const ArcherColors &C) {
  const QVector3D up(0.0f, 1.0f, 0.0f);
  const QVector3D forward(0.0f, 0.0f, 1.0f);

  QVector3D grip = P.handL;
  QVector3D topEnd(P.bowX, P.bowTopY, grip.z());
  QVector3D botEnd(P.bowX, P.bowBotY, grip.z());

  QVector3D nock(P.bowX,
                 clampf(P.handR.y(), P.bowBotY + 0.05f, P.bowTopY - 0.05f),
                 clampf(P.handR.z(), grip.z() - 0.30f, grip.z() + 0.30f));

  const int segs = 22;
  auto qBezier = [](const QVector3D &a, const QVector3D &c, const QVector3D &b,
                    float t) {
    float u = 1.0f - t;
    return u * u * a + 2.0f * u * t * c + t * t * b;
  };
  QVector3D ctrl = nock + forward * P.bowDepth;
  QVector3D prev = botEnd;
  for (int i = 1; i <= segs; ++i) {
    float t = float(i) / float(segs);
    QVector3D cur = qBezier(botEnd, ctrl, topEnd, t);
    out.mesh(getUnitCylinder(), cylinderBetween(p.model, prev, cur, P.bowRodR),
             C.wood, nullptr, 1.0f);
    prev = cur;
  }
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, grip - up * 0.05f, grip + up * 0.05f,
                           P.bowRodR * 1.45f),
           C.wood, nullptr, 1.0f);

  out.mesh(getUnitCylinder(), cylinderBetween(p.model, topEnd, nock, P.stringR),
           C.stringCol, nullptr, 1.0f);
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, nock, botEnd, P.stringR),
           C.stringCol, nullptr, 1.0f);
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, P.handR, nock, 0.0045f),
           C.stringCol * 0.9f, nullptr, 1.0f);

  QVector3D tail = nock - forward * 0.06f;
  QVector3D tip = tail + forward * 0.90f;
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, tail, tip, 0.018f),
           C.wood, nullptr, 1.0f);
  QVector3D headBase = tip - forward * 0.10f;
  out.mesh(getUnitCone(), coneFromTo(p.model, headBase, tip, 0.05f),
           C.metalHead, nullptr, 1.0f);
  QVector3D f1b = tail - forward * 0.02f, f1a = f1b - forward * 0.06f;
  QVector3D f2b = tail + forward * 0.02f, f2a = f2b + forward * 0.06f;
  out.mesh(getUnitCone(), coneFromTo(p.model, f1b, f1a, 0.04f), C.fletch,
           nullptr, 1.0f);
  out.mesh(getUnitCone(), coneFromTo(p.model, f2a, f2b, 0.04f), C.fletch,
           nullptr, 1.0f);
}

static inline void drawSelectionFX(const DrawContext &p, ISubmitter &out) {
  if (p.selected || p.hovered) {
    float ringSize = 0.5f;
    if (p.entity) {
      auto *unit = p.entity->getComponent<Engine::Core::UnitComponent>();
      if (unit && !unit->unitType.empty()) {
        ringSize = Game::Units::TroopConfig::instance().getSelectionRingSize(
            unit->unitType);
      }
    }

    QMatrix4x4 ringM;
    QVector3D pos = p.model.column(3).toVector3D();
    ringM.translate(pos.x(), pos.y() + 0.05f, pos.z());
    ringM.scale(ringSize, 1.0f, ringSize);
    if (p.selected)
      out.selectionRing(ringM, 0.6f, 0.25f, QVector3D(0.2f, 0.4f, 1.0f));
    else
      out.selectionRing(ringM, 0.35f, 0.15f, QVector3D(0.90f, 0.90f, 0.25f));
  }
}

void registerArcherRenderer(Render::GL::EntityRendererRegistry &registry) {
  registry.registerRenderer("archer", [](const DrawContext &p,
                                         ISubmitter &out) {
    QVector3D tunic(0.8f, 0.9f, 1.0f);
    Engine::Core::UnitComponent *unit = nullptr;
    Engine::Core::RenderableComponent *rc = nullptr;
    if (p.entity) {
      unit = p.entity->getComponent<Engine::Core::UnitComponent>();
      rc = p.entity->getComponent<Engine::Core::RenderableComponent>();
    }
    if (unit && unit->ownerId > 0) {
      tunic = Game::Visuals::teamColorForOwner(unit->ownerId);
    } else if (rc) {
      tunic = QVector3D(rc->color[0], rc->color[1], rc->color[2]);
    }

    uint32_t seed = 0u;
    if (unit)
      seed ^= uint32_t(unit->ownerId * 2654435761u);
    if (p.entity)
      seed ^= uint32_t(reinterpret_cast<uintptr_t>(p.entity) & 0xFFFFFFFFu);

    const int individualsPerUnit =
        Game::Units::TroopConfig::instance().getIndividualsPerUnit("archer");
    const int maxUnitsPerRow =
        Game::Units::TroopConfig::instance().getMaxUnitsPerRow("archer");
    const int rows = (individualsPerUnit + maxUnitsPerRow - 1) / maxUnitsPerRow;
    const int cols = maxUnitsPerRow;
    const float spacing = 0.75f;

    ArcherColors colors = makeColors(tunic, seed);

    bool isMoving = false;
    bool isAttacking = false;
    bool isMelee = false;
    float targetRotationY = 0.0f;

    if (p.entity) {
      auto *movement =
          p.entity->getComponent<Engine::Core::MovementComponent>();
      auto *attack = p.entity->getComponent<Engine::Core::AttackComponent>();
      auto *attackTarget =
          p.entity->getComponent<Engine::Core::AttackTargetComponent>();
      auto *transform =
          p.entity->getComponent<Engine::Core::TransformComponent>();

      isMoving = (movement && movement->hasTarget);

      if (attack && attackTarget && attackTarget->targetId > 0 && transform) {

        isMelee = (attack->currentMode ==
                   Engine::Core::AttackComponent::CombatMode::Melee);

        bool stationary = !isMoving;
        float currentCooldown =
            isMelee ? attack->meleeCooldown : attack->cooldown;
        bool recentlyFired =
            attack->timeSinceLast < std::min(currentCooldown, 0.45f);
        bool targetInRange = false;

        if (p.world) {
          auto *target = p.world->getEntity(attackTarget->targetId);
          if (target) {
            auto *targetTransform =
                target->getComponent<Engine::Core::TransformComponent>();
            if (targetTransform) {
              float dx = targetTransform->position.x - transform->position.x;
              float dz = targetTransform->position.z - transform->position.z;
              float dist = std::sqrt(dx * dx + dz * dz);
              float targetRadius = 0.0f;
              if (target->hasComponent<Engine::Core::BuildingComponent>()) {
                targetRadius = std::max(targetTransform->scale.x,
                                        targetTransform->scale.z) *
                               0.5f;
              } else {
                targetRadius = std::max(targetTransform->scale.x,
                                        targetTransform->scale.z) *
                               0.5f;
              }
              float effectiveRange = attack->range + targetRadius + 0.25f;
              targetInRange = dist <= effectiveRange;

              targetRotationY = std::atan2(dx, dz) * 180.0f / 3.14159f;
            }
          }
        }

        isAttacking = stationary && (targetInRange || recentlyFired);
      } else {
        isAttacking = false;
      }
    }

    int visibleCount = rows * cols;
    if (unit) {
      int mh = std::max(1, unit->maxHealth);
      float ratio = std::clamp(unit->health / float(mh), 0.0f, 1.0f);
      visibleCount = std::max(1, (int)std::ceil(ratio * float(rows * cols)));
    }

    int idx = 0;
    for (; idx < visibleCount; ++idx) {
      int r = idx / cols;
      int c = idx % cols;

      float offsetX = (c - (cols - 1) * 0.5f) * spacing;
      float offsetZ = (r - (rows - 1) * 0.5f) * spacing;

      QMatrix4x4 instModel = p.model;

      uint32_t instSeed = seed ^ uint32_t(idx * 9176u);

      auto hash01 = [](uint32_t x) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return (x & 0x00FFFFFF) / float(0x01000000);
      };

      float posJitterX = (hash01(instSeed) - 0.5f) * 0.05f;
      float posJitterZ = (hash01(instSeed ^ 0x12345u) - 0.5f) * 0.05f;
      float verticalJitter = (hash01(instSeed ^ 0x9E37u) - 0.5f) * 0.03f;

      offsetX += posJitterX;
      offsetZ += posJitterZ;

      float yawOffset = (hash01(instSeed ^ 0xABCDu) - 0.5f) * 5.0f;

      float phaseOffset = hash01(instSeed >> 8) * 0.25f;

      if (p.entity) {
        if (auto *entT =
                p.entity->getComponent<Engine::Core::TransformComponent>()) {
          QMatrix4x4 M = kIdentityMatrix;
          M.translate(entT->position.x, entT->position.y, entT->position.z);
          float baseYaw = entT->rotation.y;
          float appliedYaw = baseYaw + yawOffset;
          M.rotate(appliedYaw, 0.0f, 1.0f, 0.0f);
          M.scale(entT->scale.x, entT->scale.y, entT->scale.z);

          M.translate(offsetX, verticalJitter, offsetZ);
          instModel = M;
        } else {
          instModel = p.model;
          instModel.rotate(yawOffset, 0.0f, 1.0f, 0.0f);
          instModel.translate(offsetX, verticalJitter, offsetZ);
        }
      } else {
        instModel = p.model;
        instModel.rotate(yawOffset, 0.0f, 1.0f, 0.0f);
        instModel.translate(offsetX, verticalJitter, offsetZ);
      }

      DrawContext instCtx{p.resources, p.entity, p.world, instModel};

      ArcherPose pose = makePose(instSeed, p.animationTime + phaseOffset,
                                 isMoving, isAttacking, isMelee);

      drawQuiver(instCtx, out, colors, pose, instSeed);
      drawLegs(instCtx, out, pose, colors);
      drawTorso(instCtx, out, colors, pose);
      drawTunicSkirt(instCtx, out, colors, pose, p.animationTime + phaseOffset,
                     isMoving, instSeed);
      drawArms(instCtx, out, pose, colors);
      drawHeadAndNeck(instCtx, out, pose, colors);
      drawBowAndArrow(instCtx, out, pose, colors);
    }

    drawSelectionFX(p, out);
  });
}
} // namespace Render::GL
