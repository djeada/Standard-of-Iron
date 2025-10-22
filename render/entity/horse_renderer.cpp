#include "horse_renderer.h"

#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../gl/mesh.h"
#include "../gl/primitives.h"
#include "../humanoid_base.h"
#include "../palette.h"

#include <QMatrix4x4>
#include <QRandomGenerator>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Render::GL {

using Render::Geom::clamp01;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::lerp;
using Render::Geom::smoothstep;

namespace {

constexpr float kPi = 3.14159265358979323846f;

inline float hash01(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return (x & 0xFFFFFF) / float(0x1000000);
}

inline float randBetween(uint32_t seed, uint32_t salt, float minV, float maxV) {
  const float t = hash01(seed ^ salt);
  return minV + (maxV - minV) * t;
}

inline float saturate(float x) { return std::min(1.0f, std::max(0.0f, x)); }

inline QVector3D rotateAroundY(const QVector3D &v, float angle) {
  float s = std::sin(angle), c = std::cos(angle);
  return QVector3D(v.x() * c + v.z() * s, v.y(), -v.x() * s + v.z() * c);
}
inline QVector3D rotateAroundZ(const QVector3D &v, float angle) {
  float s = std::sin(angle), c = std::cos(angle);
  return QVector3D(v.x() * c - v.y() * s, v.x() * s + v.y() * c, v.z());
}

inline QVector3D darken(const QVector3D &c, float k) { return c * k; }
inline QVector3D lighten(const QVector3D &c, float k) {
  return QVector3D(saturate(c.x() * k), saturate(c.y() * k),
                   saturate(c.z() * k));
}

inline uint32_t colorHash(const QVector3D &c) {
  uint32_t r = uint32_t(saturate(c.x()) * 255.0f);
  uint32_t g = uint32_t(saturate(c.y()) * 255.0f);
  uint32_t b = uint32_t(saturate(c.z()) * 255.0f);
  uint32_t v = (r << 16) | (g << 8) | b;

  v ^= v >> 16;
  v *= 0x7feb352dU;
  v ^= v >> 15;
  v *= 0x846ca68bU;
  v ^= v >> 16;
  return v;
}

} // namespace

HorseDimensions makeHorseDimensions(uint32_t seed) {
  HorseDimensions d;

  d.bodyLength = randBetween(seed, 0x12u, 0.74f, 0.84f);
  d.bodyWidth = randBetween(seed, 0x34u, 0.17f, 0.20f);
  d.bodyHeight = randBetween(seed, 0x56u, 0.33f, 0.37f);
  d.barrelCenterY = randBetween(seed, 0x78u, 0.02f, 0.08f);

  d.neckLength = randBetween(seed, 0x9Au, 0.32f, 0.37f);
  d.neckRise = randBetween(seed, 0xBCu, 0.20f, 0.26f);
  d.headLength = randBetween(seed, 0xDEu, 0.24f, 0.28f);
  d.headWidth = randBetween(seed, 0xF1u, 0.13f, 0.16f);
  d.headHeight = randBetween(seed, 0x1357u, 0.16f, 0.19f);
  d.muzzleLength = randBetween(seed, 0x2468u, 0.11f, 0.14f);

  d.legLength = randBetween(seed, 0x369Cu, 0.87f, 0.99f);
  d.hoofHeight = randBetween(seed, 0x48AEu, 0.070f, 0.080f);

  d.tailLength = randBetween(seed, 0x5ABCu, 0.30f, 0.36f);

  d.saddleThickness = randBetween(seed, 0x6CDEu, 0.040f, 0.052f);
  d.seatForwardOffset = randBetween(seed, 0x7531u, 0.020f, 0.050f);
  d.stirrupOut = d.bodyWidth * randBetween(seed, 0x8642u, 0.88f, 0.98f);
  d.stirrupDrop = randBetween(seed, 0x9753u, 0.23f, 0.27f);

  d.idleBobAmplitude = randBetween(seed, 0xA864u, 0.005f, 0.008f);
  d.moveBobAmplitude = randBetween(seed, 0xB975u, 0.020f, 0.028f);

  d.saddleHeight = d.barrelCenterY + d.bodyHeight * 0.55f + d.saddleThickness;

  return d;
}

HorseVariant makeHorseVariant(uint32_t seed, const QVector3D &leatherBase,
                              const QVector3D &clothBase) {
  HorseVariant v;

  float coatHue = hash01(seed ^ 0x23456u);
  if (coatHue < 0.18f) {
    v.coatColor = QVector3D(0.70f, 0.68f, 0.63f);
  } else if (coatHue < 0.38f) {
    v.coatColor = QVector3D(0.40f, 0.30f, 0.22f);
  } else if (coatHue < 0.65f) {
    v.coatColor = QVector3D(0.28f, 0.22f, 0.19f);
  } else if (coatHue < 0.85f) {
    v.coatColor = QVector3D(0.18f, 0.15f, 0.13f);
  } else {
    v.coatColor = QVector3D(0.48f, 0.42f, 0.39f);
  }

  float blazeChance = hash01(seed ^ 0x1122u);
  if (blazeChance > 0.82f) {
    v.coatColor = lerp(v.coatColor, QVector3D(0.92f, 0.92f, 0.90f), 0.25f);
  }

  v.maneColor = lerp(v.coatColor, QVector3D(0.10f, 0.09f, 0.08f),
                        randBetween(seed, 0x3344u, 0.55f, 0.85f));
  v.tailColor = lerp(v.maneColor, v.coatColor, 0.35f);

  v.muzzleColor = lerp(v.coatColor, QVector3D(0.18f, 0.14f, 0.12f), 0.65f);
  v.hoofColor =
      lerp(QVector3D(0.16f, 0.14f, 0.12f), QVector3D(0.40f, 0.35f, 0.32f),
              randBetween(seed, 0x5566u, 0.15f, 0.65f));

  float leatherTone = randBetween(seed, 0x7788u, 0.78f, 0.96f);
  float tackTone = randBetween(seed, 0x88AAu, 0.58f, 0.78f);
  QVector3D leatherTint = leatherBase * leatherTone;
  QVector3D tackTint = leatherBase * tackTone;
  if (blazeChance > 0.90f) {

    tackTint = lerp(tackTint, QVector3D(0.18f, 0.19f, 0.22f), 0.25f);
  }
  v.saddleColor = leatherTint;
  v.tackColor = tackTint;

  v.blanketColor = clothBase * randBetween(seed, 0x99B0u, 0.92f, 1.05f);

  return v;
}

HorseProfile makeHorseProfile(uint32_t seed, const QVector3D &leatherBase,
                              const QVector3D &clothBase) {
  HorseProfile profile;
  profile.dims = makeHorseDimensions(seed);
  profile.variant = makeHorseVariant(seed, leatherBase, clothBase);

  profile.gait.cycleTime = randBetween(seed, 0xAA12u, 0.50f, 0.58f);
  profile.gait.frontLegPhase = randBetween(seed, 0xBB34u, 0.10f, 0.18f);
  float diagonalLead = randBetween(seed, 0xCC56u, 0.48f, 0.56f);
  profile.gait.rearLegPhase =
      std::fmod(profile.gait.frontLegPhase + diagonalLead, 1.0f);
  profile.gait.strideSwing = randBetween(seed, 0xDD78u, 0.18f, 0.24f);
  profile.gait.strideLift = randBetween(seed, 0xEE9Au, 0.11f, 0.15f);

  return profile;
}

void HorseRenderer::render(const DrawContext &ctx, const AnimationInputs &anim,
                           const HorseProfile &profile, ISubmitter &out) const {
  const HorseDimensions &d = profile.dims;
  const HorseVariant &v = profile.variant;
  const HorseGait &g = profile.gait;

  float phase = 0.0f;
  float bob = 0.0f;

  if (anim.isMoving) {
    float cycle = std::max(0.20f, g.cycleTime);
    phase = std::fmod(anim.time / cycle, 1.0f);
    bob = std::sin(phase * 2.0f * kPi) * d.moveBobAmplitude;
  } else {
    phase = std::fmod(anim.time * 0.25f, 1.0f);
    bob = std::sin(phase * 2.0f * kPi) * d.idleBobAmplitude;
  }

  float headNod = anim.isMoving ? std::sin((phase + 0.25f) * 2.0f * kPi) * 0.04f
                                : std::sin(anim.time * 1.5f) * 0.01f;

  uint32_t vhash = colorHash(v.coatColor);
  float sockChanceFL = hash01(vhash ^ 0x101u);
  float sockChanceFR = hash01(vhash ^ 0x202u);
  float sockChanceRL = hash01(vhash ^ 0x303u);
  float sockChanceRR = hash01(vhash ^ 0x404u);
  bool hasBlaze = hash01(vhash ^ 0x505u) > 0.82f;

  QVector3D barrelCenter(0.0f, d.barrelCenterY + bob, 0.0f);
  QVector3D chestCenter = barrelCenter + QVector3D(0.0f, d.bodyHeight * 0.12f,
                                                   d.bodyLength * 0.34f);
  QVector3D rumpCenter = barrelCenter + QVector3D(0.0f, d.bodyHeight * 0.08f,
                                                  -d.bodyLength * 0.36f);
  QVector3D bellyCenter = barrelCenter + QVector3D(0.0f, -d.bodyHeight * 0.35f,
                                                   -d.bodyLength * 0.05f);

  {
    QMatrix4x4 chest = ctx.model;
    chest.translate(chestCenter);
    chest.scale(d.bodyWidth * 1.12f, d.bodyHeight * 0.95f,
                d.bodyLength * 0.36f);
    out.mesh(getUnitSphere(), chest, v.coatColor * 1.03f, nullptr, 1.0f);
  }

  {
    QMatrix4x4 withers = ctx.model;
    withers.translate(chestCenter + QVector3D(0.0f, d.bodyHeight * 0.55f,
                                              -d.bodyLength * 0.03f));
    withers.scale(d.bodyWidth * 0.75f, d.bodyHeight * 0.35f,
                  d.bodyLength * 0.18f);
    out.mesh(getUnitSphere(), withers, v.coatColor * 0.96f, nullptr, 1.0f);
  }

  {
    QMatrix4x4 belly = ctx.model;
    belly.translate(bellyCenter);
    belly.scale(d.bodyWidth * 0.98f, d.bodyHeight * 0.64f,
                d.bodyLength * 0.40f);
    out.mesh(getUnitSphere(), belly, v.coatColor * 1.06f, nullptr, 1.0f);
  }

  for (int i = 0; i < 2; ++i) {
    float side = (i == 0) ? 1.0f : -1.0f;
    QMatrix4x4 ribs = ctx.model;
    ribs.translate(barrelCenter + QVector3D(side * d.bodyWidth * 0.90f,
                                            -d.bodyHeight * 0.10f,
                                            -d.bodyLength * 0.05f));
    ribs.scale(d.bodyWidth * 0.38f, d.bodyHeight * 0.42f, d.bodyLength * 0.30f);
    out.mesh(getUnitSphere(), ribs, v.coatColor * 1.00f, nullptr, 1.0f);
  }

  {
    QMatrix4x4 rump = ctx.model;
    rump.translate(rumpCenter);
    rump.scale(d.bodyWidth * 1.18f, d.bodyHeight * 1.00f, d.bodyLength * 0.36f);
    out.mesh(getUnitSphere(), rump, v.coatColor * 0.98f, nullptr, 1.0f);
  }

  for (int i = 0; i < 2; ++i) {
    float side = (i == 0) ? 1.0f : -1.0f;
    QMatrix4x4 hip = ctx.model;
    hip.translate(rumpCenter + QVector3D(side * d.bodyWidth * 0.95f,
                                         -d.bodyHeight * 0.10f,
                                         -d.bodyLength * 0.08f));
    hip.scale(d.bodyWidth * 0.45f, d.bodyHeight * 0.42f, d.bodyLength * 0.26f);
    out.mesh(getUnitSphere(), hip, v.coatColor * 0.99f, nullptr, 1.0f);
  }

  QVector3D neckBase =
      chestCenter + QVector3D(0.0f, d.bodyHeight * 0.38f, d.bodyLength * 0.06f);
  QVector3D neckTop = neckBase + QVector3D(0.0f, d.neckRise, d.neckLength);
  float neckRadius = d.bodyWidth * 0.42f;

  QVector3D neckMid =
      lerp(neckBase, neckTop, 0.55f) +
      QVector3D(0.0f, d.bodyHeight * 0.02f, d.bodyLength * 0.02f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, neckBase, neckMid, neckRadius * 1.00f),
           v.coatColor * 1.03f, nullptr, 1.0f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, neckMid, neckTop, neckRadius * 0.86f),
           v.coatColor * 1.04f, nullptr, 1.0f);

  QVector3D headCenter =
      neckTop + QVector3D(0.0f, d.headHeight * (0.10f - headNod * 0.15f),
                          d.headLength * 0.40f);

  {
    QMatrix4x4 skull = ctx.model;
    skull.translate(headCenter + QVector3D(0.0f, d.headHeight * 0.10f,
                                           -d.headLength * 0.10f));
    skull.scale(d.headWidth * 0.95f, d.headHeight * 0.90f,
                d.headLength * 0.80f);
    out.mesh(getUnitSphere(), skull, v.coatColor * 1.05f, nullptr, 1.0f);
  }

  for (int i = 0; i < 2; ++i) {
    float side = (i == 0) ? 1.0f : -1.0f;
    QMatrix4x4 cheek = ctx.model;
    cheek.translate(headCenter + QVector3D(side * d.headWidth * 0.55f,
                                           -d.headHeight * 0.15f, 0.0f));
    cheek.scale(d.headWidth * 0.45f, d.headHeight * 0.50f,
                d.headLength * 0.60f);
    out.mesh(getUnitSphere(), cheek, v.coatColor * 1.02f, nullptr, 1.0f);
  }

  QVector3D muzzleCenter =
      headCenter + QVector3D(0.0f, -d.headHeight * 0.18f, d.headLength * 0.58f);
  {
    QMatrix4x4 muzzle = ctx.model;
    muzzle.translate(muzzleCenter +
                     QVector3D(0.0f, -d.headHeight * 0.05f, 0.0f));
    muzzle.scale(d.headWidth * 0.68f, d.headHeight * 0.60f,
                 d.muzzleLength * 1.05f);
    out.mesh(getUnitSphere(), muzzle, v.muzzleColor, nullptr, 1.0f);
  }

  {
    QVector3D nostrilBase =
        muzzleCenter +
        QVector3D(0.0f, -d.headHeight * 0.02f, d.muzzleLength * 0.60f);
    QVector3D leftBase =
        nostrilBase + QVector3D(d.headWidth * 0.26f, 0.0f, 0.0f);
    QVector3D rightBase =
        nostrilBase + QVector3D(-d.headWidth * 0.26f, 0.0f, 0.0f);
    QVector3D inward =
        QVector3D(0.0f, -d.headHeight * 0.02f, d.muzzleLength * -0.30f);
    out.mesh(
        getUnitCone(),
        coneFromTo(ctx.model, leftBase + inward, leftBase, d.headWidth * 0.11f),
        darken(v.muzzleColor, 0.6f), nullptr, 1.0f);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, rightBase + inward, rightBase,
                        d.headWidth * 0.11f),
             darken(v.muzzleColor, 0.6f), nullptr, 1.0f);
  }

  float earFlickL = std::sin(anim.time * 1.7f + 1.3f) * 0.15f;
  float earFlickR = std::sin(anim.time * 1.9f + 2.1f) * -0.12f;

  QVector3D earBaseLeft =
      headCenter + QVector3D(d.headWidth * 0.45f, d.headHeight * 0.42f,
                             -d.headLength * 0.20f);
  QVector3D earTipLeft =
      earBaseLeft +
      rotateAroundY(QVector3D(d.headWidth * 0.08f, d.headHeight * 0.42f,
                              -d.headLength * 0.10f),
                    earFlickL);
  QVector3D earBaseRight =
      headCenter + QVector3D(-d.headWidth * 0.45f, d.headHeight * 0.42f,
                             -d.headLength * 0.20f);
  QVector3D earTipRight =
      earBaseRight +
      rotateAroundY(QVector3D(-d.headWidth * 0.08f, d.headHeight * 0.42f,
                              -d.headLength * 0.10f),
                    earFlickR);

  out.mesh(getUnitCone(),
           coneFromTo(ctx.model, earTipLeft, earBaseLeft, d.headWidth * 0.11f),
           v.maneColor, nullptr, 1.0f);
  out.mesh(
      getUnitCone(),
      coneFromTo(ctx.model, earTipRight, earBaseRight, d.headWidth * 0.11f),
      v.maneColor, nullptr, 1.0f);

  QVector3D eyeLeft =
      headCenter + QVector3D(d.headWidth * 0.48f, d.headHeight * 0.10f,
                             d.headLength * 0.05f);
  QVector3D eyeRight =
      headCenter + QVector3D(-d.headWidth * 0.48f, d.headHeight * 0.10f,
                             d.headLength * 0.05f);
  QVector3D eyeBaseColor(0.10f, 0.10f, 0.10f);

  auto drawEye = [&](const QVector3D &pos) {
    {
      QMatrix4x4 eye = ctx.model;
      eye.translate(pos);
      eye.scale(d.headWidth * 0.14f);
      out.mesh(getUnitSphere(), eye, eyeBaseColor, nullptr, 1.0f);
    }
    {

      QMatrix4x4 pupil = ctx.model;
      pupil.translate(pos + QVector3D(0.0f, 0.0f, d.headWidth * 0.04f));
      pupil.scale(d.headWidth * 0.05f);
      out.mesh(getUnitSphere(), pupil, QVector3D(0.03f, 0.03f, 0.03f), nullptr,
               1.0f);
    }
    {

      QMatrix4x4 spec = ctx.model;
      spec.translate(pos + QVector3D(d.headWidth * 0.03f, d.headWidth * 0.03f,
                                     d.headWidth * 0.03f));
      spec.scale(d.headWidth * 0.02f);
      out.mesh(getUnitSphere(), spec, QVector3D(0.95f, 0.95f, 0.95f), nullptr,
               1.0f);
    }
  };
  drawEye(eyeLeft);
  drawEye(eyeRight);

  if (hasBlaze) {
    QMatrix4x4 blaze = ctx.model;
    blaze.translate(headCenter + QVector3D(0.0f, d.headHeight * 0.15f,
                                           d.headLength * 0.10f));
    blaze.scale(d.headWidth * 0.22f, d.headHeight * 0.32f,
                d.headLength * 0.10f);
    out.mesh(getUnitSphere(), blaze, QVector3D(0.92f, 0.92f, 0.90f), nullptr,
             1.0f);
  }

  QVector3D maneRoot =
      neckTop + QVector3D(0.0f, d.headHeight * 0.20f, -d.headLength * 0.20f);
  for (int i = 0; i < 12; ++i) {
    float t = i / 11.0f;
    QVector3D segStart = lerp(maneRoot, neckBase, t);
    segStart.setY(segStart.y() + (0.07f - t * 0.05f));
    float sway =
        (anim.isMoving ? std::sin((phase + t * 0.15f) * 2.0f * kPi) * 0.04f
                       : std::sin((anim.time * 0.8f + t * 2.3f)) * 0.02f);
    QVector3D segEnd =
        segStart + QVector3D(sway, 0.07f - t * 0.05f, -0.05f - t * 0.03f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, segStart, segEnd,
                             d.headWidth * (0.10f * (1.0f - t * 0.4f))),
             v.maneColor * (0.98f + t * 0.05f), nullptr, 1.0f);
  }

  QVector3D tailBase =
      rumpCenter + QVector3D(0.0f, d.bodyHeight * 0.36f, -d.bodyLength * 0.48f);
  for (int i = 0; i < 8; ++i) {
    float t = i / 8.0f;
    float swing =
        (anim.isMoving
             ? std::sin((phase + t * 0.10f) * 2.0f * kPi) *
                   (0.05f + 0.02f * (1.0f - t))
             : std::sin((phase * 0.7f + t * 0.20f) * 2.0f * kPi) * 0.04f);
    QVector3D segStart =
        tailBase + QVector3D(swing, -i * 0.06f, -t * d.tailLength * 0.65f);
    QVector3D segEnd = segStart + QVector3D(swing * 0.6f, -0.08f - t * 0.05f,
                                            -d.tailLength * 0.16f);
    float radius = d.bodyWidth * (0.22f - 0.025f * i);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, segStart, segEnd, radius),
             v.tailColor * (0.95f - 0.05f * i), nullptr, 1.0f);
  }

  auto drawLeg = [&](const QVector3D &anchor, float lateralSign,
                     float forwardBias, float phaseOffset, float sockChance) {
    float legPhase = std::fmod(phase + phaseOffset, 1.0f);
    float stride = 0.0f;
    float lift = 0.0f;

    if (anim.isMoving) {
      float angle = legPhase * 2.0f * kPi;
      stride = std::sin(angle) * g.strideSwing + forwardBias;
      float liftRaw = std::sin(angle);
      lift = liftRaw > 0.0f ? liftRaw * g.strideLift
                            : liftRaw * g.strideLift * 0.22f;
    } else {
      float idle = std::sin(legPhase * 2.0f * kPi);
      stride = idle * g.strideSwing * 0.06f + forwardBias;
      lift = idle * d.idleBobAmplitude * 2.0f;
    }

    bool tightenLegs = anim.isMoving;
    float shoulderOut = d.bodyWidth * (tightenLegs ? 0.48f : 0.58f);
    QVector3D shoulder = anchor + QVector3D(lateralSign * shoulderOut,
                                            0.05f + lift * 0.05f, stride);
    bool isRear = (forwardBias < 0.0f);

    float gallopAngle = legPhase * 2.0f * kPi;
    float hipSwing = anim.isMoving ? std::sin(gallopAngle) : 0.0f;
    float liftFactor =
        anim.isMoving
            ? std::max(0.0f, std::sin(gallopAngle + (isRear ? 0.35f : -0.25f)))
            : 0.0f;

    shoulder.setZ(shoulder.z() + hipSwing * (isRear ? -0.12f : 0.10f));
    if (tightenLegs)
      shoulder.setX(shoulder.x() - lateralSign * liftFactor * 0.05f);

    float thighLength = d.legLength * 0.55f;
    float hipPitch = hipSwing * (isRear ? 0.70f : 0.55f);
    float inwardLean = tightenLegs ? (-0.06f - liftFactor * 0.04f) : -0.015f;
    QVector3D thighDir(lateralSign * inwardLean, -std::cos(hipPitch) * 0.90f,
                       (isRear ? -1.0f : 1.0f) * std::sin(hipPitch) * 0.65f);
    if (thighDir.lengthSquared() > 1e-6f)
      thighDir.normalize();

    QVector3D knee = shoulder + thighDir * thighLength;
    knee.setY(knee.y() + liftFactor * thighLength * 0.28f);

    float kneeFlex =
        anim.isMoving
            ? clamp01(std::sin(gallopAngle + (isRear ? 0.65f : -0.45f)) * 0.7f +
                      0.45f)
            : 0.35f;

    float forearmLength = d.legLength * 0.30f;
    float bendCos = std::cos(kneeFlex * kPi * 0.5f);
    float bendSin = std::sin(kneeFlex * kPi * 0.5f);
    QVector3D forearmDir(0.0f, -bendCos,
                         (isRear ? -1.0f : 1.0f) * bendSin * 0.85f);
    if (forearmDir.lengthSquared() < 1e-6f)
      forearmDir = QVector3D(0.0f, -1.0f, 0.0f);
    else
      forearmDir.normalize();
    QVector3D cannon = knee + forearmDir * forearmLength;

    float pasternLength = d.legLength * 0.12f;
    QVector3D fetlock = cannon + QVector3D(0.0f, -pasternLength, 0.0f);

    float hoofPitch = anim.isMoving
                          ? (-0.20f + std::sin(legPhase * 2.0f * kPi +
                                               (isRear ? 0.2f : -0.1f)) *
                                          0.10f)
                          : 0.0f;
    QVector3D hoofDir = rotateAroundZ(QVector3D(0.0f, -1.0f, 0.0f), hoofPitch);
    QVector3D hoofTop = fetlock;
    QVector3D hoofBottom = hoofTop + hoofDir * d.hoofHeight;

    float shoulderR = d.bodyWidth * (isRear ? 0.48f : 0.42f);
    float thighBellyR = d.bodyWidth * (isRear ? 0.60f : 0.54f);
    float kneeR = d.bodyWidth * 0.26f;
    float cannonR = d.bodyWidth * 0.19f;
    float pasternR = d.bodyWidth * 0.14f;

    QVector3D thighBelly = shoulder + (knee - shoulder) * 0.62f;

    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, thighBelly, shoulder, thighBellyR),
             v.coatColor * 1.01f, nullptr, 1.0f);

    {
      QMatrix4x4 muscle = ctx.model;
      muscle.translate(thighBelly +
                       QVector3D(0.0f, 0.0f, isRear ? -0.015f : 0.020f));
      muscle.scale(thighBellyR * QVector3D(1.05f, 0.85f, 0.92f));
      out.mesh(getUnitSphere(), muscle, v.coatColor * 1.04f, nullptr, 1.0f);
    }

    out.mesh(getUnitCone(), coneFromTo(ctx.model, knee, thighBelly, kneeR),
             v.coatColor * 0.98f, nullptr, 1.0f);

    {

      QMatrix4x4 joint = ctx.model;
      joint.translate(knee + QVector3D(0.0f, 0.0f, isRear ? -0.025f : 0.030f));
      joint.scale(QVector3D(kneeR * 1.22f, kneeR * 1.08f, kneeR * 1.40f));
      out.mesh(getUnitSphere(), joint, v.coatColor * 0.95f, nullptr, 1.0f);
    }

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, knee, cannon, cannonR),
             v.coatColor * 0.94f, nullptr, 1.0f);

    {

      QMatrix4x4 joint = ctx.model;
      joint.translate(fetlock);
      joint.scale(pasternR * 1.18f);
      out.mesh(getUnitSphere(), joint, v.coatColor * 0.95f, nullptr, 1.0f);
    }

    float sock =
        sockChance > 0.78f ? 1.0f : (sockChance > 0.58f ? 0.55f : 0.0f);
    QVector3D distalColor =
        (sock > 0.0f) ? lighten(v.coatColor, 1.18f) : v.coatColor * 0.92f;
    float tSock = smoothstep(0.0f, 1.0f, sock);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cannon, fetlock, pasternR * 1.05f),
             lerp(v.coatColor * 0.94f, distalColor, tSock * 0.8f), nullptr,
             1.0f);

    QVector3D hoofColor = v.hoofColor;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, hoofTop, hoofBottom, pasternR * 0.95f),
             hoofColor, nullptr, 1.0f);

    {

      QVector3D toe = hoofBottom + QVector3D(0.0f, -d.hoofHeight * 0.15f, 0.0f);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, toe, hoofBottom, pasternR * 0.88f),
               hoofColor * 0.96f, nullptr, 1.0f);
    }
  };

  QVector3D frontAnchor = barrelCenter + QVector3D(0.0f, d.bodyHeight * 0.05f,
                                                   d.bodyLength * 0.28f);
  QVector3D rearAnchor = barrelCenter + QVector3D(0.0f, d.bodyHeight * 0.02f,
                                                  -d.bodyLength * 0.32f);

  drawLeg(frontAnchor, 1.0f, d.bodyLength * 0.30f, g.frontLegPhase,
          sockChanceFL);
  drawLeg(frontAnchor, -1.0f, d.bodyLength * 0.30f, g.frontLegPhase + 0.5f,
          sockChanceFR);
  drawLeg(rearAnchor, 1.0f, -d.bodyLength * 0.28f, g.rearLegPhase,
          sockChanceRL);
  drawLeg(rearAnchor, -1.0f, -d.bodyLength * 0.28f, g.rearLegPhase + 0.5f,
          sockChanceRR);

  float saddleTop = d.saddleHeight;
  QVector3D saddleCenter(0.0f, saddleTop - d.saddleThickness * 0.3f,
                         d.seatForwardOffset * 0.4f);
  {
    QMatrix4x4 saddle = ctx.model;
    saddle.translate(saddleCenter);
    saddle.scale(d.bodyWidth * 1.08f, d.saddleThickness, d.bodyLength * 0.32f);
    out.mesh(getUnitSphere(), saddle, v.saddleColor, nullptr, 1.0f);
  }

  QVector3D blanketCenter =
      saddleCenter + QVector3D(0.0f, -d.saddleThickness, 0.0f);
  {
    QMatrix4x4 blanket = ctx.model;
    blanket.translate(blanketCenter);
    blanket.scale(d.bodyWidth * 1.24f, d.saddleThickness * 0.35f,
                  d.bodyLength * 0.40f);
    out.mesh(getUnitSphere(), blanket, v.blanketColor * 1.02f, nullptr, 1.0f);
  }

  for (int i = 0; i < 2; ++i) {
    float side = (i == 0) ? 1.0f : -1.0f;
    QVector3D strapTop =
        saddleCenter +
        QVector3D(side * d.bodyWidth * 0.92f, d.saddleThickness * 0.35f, 0.0f);
    QVector3D strapBottom =
        strapTop + QVector3D(0.0f, -d.bodyHeight * 0.95f, 0.02f);
    out.mesh(
        getUnitCylinder(),
        cylinderBetween(ctx.model, strapTop, strapBottom, d.bodyWidth * 0.07f),
        v.tackColor * 0.94f, nullptr, 1.0f);
  }

  auto drawStirrup = [&](float sideSign) {
    QVector3D stirrupAttach =
        saddleCenter + QVector3D(sideSign * d.bodyWidth * 0.92f,
                                 -d.saddleThickness * 0.1f,
                                 d.seatForwardOffset * 0.3f);
    QVector3D stirrupBottom =
        stirrupAttach + QVector3D(0.0f, -d.stirrupDrop, 0.0f);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, stirrupAttach, stirrupBottom,
                             d.bodyWidth * 0.05f),
             v.tackColor, nullptr, 1.0f);

    QMatrix4x4 stirrup = ctx.model;
    stirrup.translate(stirrupBottom +
                      QVector3D(0.0f, -d.bodyWidth * 0.06f, 0.0f));
    stirrup.scale(d.bodyWidth * 0.20f, d.bodyWidth * 0.07f,
                  d.bodyWidth * 0.16f);
    out.mesh(getUnitSphere(), stirrup, QVector3D(0.66f, 0.65f, 0.62f), nullptr,
             1.0f);
  };

  drawStirrup(1.0f);
  drawStirrup(-1.0f);

  QVector3D cheekLeftTop =
      headCenter + QVector3D(d.headWidth * 0.60f, -d.headHeight * 0.10f,
                             d.headLength * 0.25f);
  QVector3D cheekLeftBottom =
      cheekLeftTop + QVector3D(0.0f, -d.headHeight, -d.headLength * 0.12f);
  QVector3D cheekRightTop =
      headCenter + QVector3D(-d.headWidth * 0.60f, -d.headHeight * 0.10f,
                             d.headLength * 0.25f);
  QVector3D cheekRightBottom =
      cheekRightTop + QVector3D(0.0f, -d.headHeight, -d.headLength * 0.12f);

  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, cheekLeftTop, cheekLeftBottom,
                           d.headWidth * 0.08f),
           v.tackColor, nullptr, 1.0f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, cheekRightTop, cheekRightBottom,
                           d.headWidth * 0.08f),
           v.tackColor, nullptr, 1.0f);

  QVector3D noseBandFront = muzzleCenter + QVector3D(0.0f, d.headHeight * 0.02f,
                                                     d.muzzleLength * 0.35f);
  QVector3D noseBandLeft =
      noseBandFront + QVector3D(d.headWidth * 0.55f, 0.0f, 0.0f);
  QVector3D noseBandRight =
      noseBandFront + QVector3D(-d.headWidth * 0.55f, 0.0f, 0.0f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, noseBandLeft, noseBandRight,
                           d.headWidth * 0.08f),
           v.tackColor * 0.92f, nullptr, 1.0f);

  QVector3D browBandFront =
      headCenter + QVector3D(0.0f, d.headHeight * 0.28f, d.headLength * 0.15f);
  QVector3D browBandLeft =
      browBandFront + QVector3D(d.headWidth * 0.58f, 0.0f, 0.0f);
  QVector3D browBandRight =
      browBandFront + QVector3D(-d.headWidth * 0.58f, 0.0f, 0.0f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, browBandLeft, browBandRight,
                           d.headWidth * 0.07f),
           v.tackColor, nullptr, 1.0f);

  QVector3D bitLeft =
      muzzleCenter + QVector3D(d.headWidth * 0.55f, -d.headHeight * 0.08f,
                               d.muzzleLength * 0.10f);
  QVector3D bitRight =
      muzzleCenter + QVector3D(-d.headWidth * 0.55f, -d.headHeight * 0.08f,
                               d.muzzleLength * 0.10f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, bitLeft, bitRight, d.headWidth * 0.05f),
           QVector3D(0.55f, 0.55f, 0.55f), nullptr, 1.0f);

  for (int i = 0; i < 2; ++i) {
    float side = (i == 0) ? 1.0f : -1.0f;
    QVector3D reinStart = (i == 0) ? bitLeft : bitRight;
    QVector3D reinEnd = saddleCenter + QVector3D(side * d.bodyWidth * 0.65f,
                                                 -d.saddleThickness * 0.3f,
                                                 d.seatForwardOffset * 0.15f);

    QVector3D mid = lerp(reinStart, reinEnd, 0.5f) +
                    QVector3D(0.0f, -d.bodyHeight * 0.10f, 0.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, reinStart, mid, d.bodyWidth * 0.02f),
             v.tackColor * 0.95f, nullptr, 1.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, mid, reinEnd, d.bodyWidth * 0.02f),
             v.tackColor * 0.95f, nullptr, 1.0f);
  }
}

} // namespace Render::GL
