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

inline QVector3D coatGradient(const QVector3D &coat, float verticalFactor,
                              float longitudinalFactor, float seed) {
  float highlight = saturate(0.55f + verticalFactor * 0.35f -
                             longitudinalFactor * 0.20f + seed * 0.08f);
  QVector3D bright = lighten(coat, 1.08f);
  QVector3D shadow = darken(coat, 0.86f);
  return shadow * (1.0f - highlight) + bright * highlight;
}

inline QVector3D lerp3(const QVector3D &a, const QVector3D &b, float t) {
  return QVector3D(a.x() + (b.x() - a.x()) * t,
                   a.y() + (b.y() - a.y()) * t,
                   a.z() + (b.z() - a.z()) * t);
}

inline QMatrix4x4 scaledSphere(const QMatrix4x4 &model, const QVector3D &center,
                               const QVector3D &scale) {
  QMatrix4x4 m = model;
  m.translate(center);
  m.scale(scale);
  return m;
}

inline void drawCylinder(ISubmitter &out, const QMatrix4x4 &model,
                         const QVector3D &a, const QVector3D &b, float radius,
                         const QVector3D &color, float alpha = 1.0f) {
  out.mesh(getUnitCylinder(), cylinderBetween(model, a, b, radius), color,
           nullptr, alpha);
}

inline void drawCone(ISubmitter &out, const QMatrix4x4 &model,
                     const QVector3D &tip, const QVector3D &base, float radius,
                     const QVector3D &color, float alpha = 1.0f) {
  out.mesh(getUnitCone(), coneFromTo(model, tip, base, radius), color, nullptr,
           alpha);
}

inline QVector3D bezier(const QVector3D &p0, const QVector3D &p1,
                        const QVector3D &p2, float t) {
  float u = 1.0f - t;
  return p0 * (u * u) + p1 * (2.0f * u * t) + p2 * (t * t);
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

  d.bodyLength = randBetween(seed, 0x12u, 0.88f, 0.98f);
  d.bodyWidth = randBetween(seed, 0x34u, 0.18f, 0.22f);
  d.bodyHeight = randBetween(seed, 0x56u, 0.40f, 0.46f);
  d.barrelCenterY = randBetween(seed, 0x78u, 0.05f, 0.09f);

  d.neckLength = randBetween(seed, 0x9Au, 0.42f, 0.50f);
  d.neckRise = randBetween(seed, 0xBCu, 0.26f, 0.32f);
  d.headLength = randBetween(seed, 0xDEu, 0.28f, 0.34f);
  d.headWidth = randBetween(seed, 0xF1u, 0.14f, 0.17f);
  d.headHeight = randBetween(seed, 0x1357u, 0.18f, 0.22f);
  d.muzzleLength = randBetween(seed, 0x2468u, 0.13f, 0.16f);

  d.legLength = randBetween(seed, 0x369Cu, 1.05f, 1.18f);
  d.hoofHeight = randBetween(seed, 0x48AEu, 0.080f, 0.095f);

  d.tailLength = randBetween(seed, 0x5ABCu, 0.38f, 0.48f);

  d.saddleThickness = randBetween(seed, 0x6CDEu, 0.035f, 0.045f);
  d.seatForwardOffset = randBetween(seed, 0x7531u, 0.010f, 0.035f);
  d.stirrupOut = d.bodyWidth * randBetween(seed, 0x8642u, 0.75f, 0.88f);
  d.stirrupDrop = randBetween(seed, 0x9753u, 0.28f, 0.32f);

  d.idleBobAmplitude = randBetween(seed, 0xA864u, 0.004f, 0.007f);
  d.moveBobAmplitude = randBetween(seed, 0xB975u, 0.024f, 0.032f);

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

  profile.gait.cycleTime = randBetween(seed, 0xAA12u, 0.60f, 0.72f);
  profile.gait.frontLegPhase = randBetween(seed, 0xBB34u, 0.08f, 0.16f);
  float diagonalLead = randBetween(seed, 0xCC56u, 0.44f, 0.54f);
  profile.gait.rearLegPhase =
      std::fmod(profile.gait.frontLegPhase + diagonalLead, 1.0f);
  profile.gait.strideSwing = randBetween(seed, 0xDD78u, 0.26f, 0.32f);
  profile.gait.strideLift = randBetween(seed, 0xEE9Au, 0.10f, 0.14f);

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
  float riderLean = hash01(vhash ^ 0x606u) * 0.12f - 0.06f;
  float reinSlack = hash01(vhash ^ 0x707u) * 0.08f + 0.02f;

  const float coatSeedA = hash01(vhash ^ 0x701u);
  const float coatSeedB = hash01(vhash ^ 0x702u);
  const float coatSeedC = hash01(vhash ^ 0x703u);
  const float coatSeedD = hash01(vhash ^ 0x704u);

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
    QVector3D chestColor =
        coatGradient(v.coatColor, 0.75f, 0.20f, coatSeedA);
    out.mesh(getUnitSphere(), chest, chestColor, nullptr, 1.0f);
  }

  {
    QMatrix4x4 withers = ctx.model;
    withers.translate(chestCenter + QVector3D(0.0f, d.bodyHeight * 0.55f,
                                              -d.bodyLength * 0.03f));
    withers.scale(d.bodyWidth * 0.75f, d.bodyHeight * 0.35f,
                  d.bodyLength * 0.18f);
    QVector3D witherColor =
        coatGradient(v.coatColor, 0.88f, 0.35f, coatSeedB);
    out.mesh(getUnitSphere(), withers, witherColor, nullptr, 1.0f);
  }

  {
    QMatrix4x4 belly = ctx.model;
    belly.translate(bellyCenter);
    belly.scale(d.bodyWidth * 0.98f, d.bodyHeight * 0.64f,
                d.bodyLength * 0.40f);
    QVector3D bellyColor =
        coatGradient(v.coatColor, 0.25f, -0.10f, coatSeedC);
    out.mesh(getUnitSphere(), belly, bellyColor, nullptr, 1.0f);
  }

  for (int i = 0; i < 2; ++i) {
    float side = (i == 0) ? 1.0f : -1.0f;
    QMatrix4x4 ribs = ctx.model;
    ribs.translate(barrelCenter + QVector3D(side * d.bodyWidth * 0.90f,
                                            -d.bodyHeight * 0.10f,
                                            -d.bodyLength * 0.05f));
    ribs.scale(d.bodyWidth * 0.38f, d.bodyHeight * 0.42f, d.bodyLength * 0.30f);
    QVector3D ribColor =
        coatGradient(v.coatColor, 0.45f, 0.05f, coatSeedD + side * 0.05f);
    out.mesh(getUnitSphere(), ribs, ribColor, nullptr, 1.0f);
  }

  {
    QMatrix4x4 rump = ctx.model;
    rump.translate(rumpCenter);
    rump.scale(d.bodyWidth * 1.18f, d.bodyHeight * 1.00f, d.bodyLength * 0.36f);
    QVector3D rumpColor =
        coatGradient(v.coatColor, 0.62f, -0.28f, coatSeedA * 0.7f);
    out.mesh(getUnitSphere(), rump, rumpColor, nullptr, 1.0f);
  }

  for (int i = 0; i < 2; ++i) {
    float side = (i == 0) ? 1.0f : -1.0f;
    QMatrix4x4 hip = ctx.model;
    hip.translate(rumpCenter + QVector3D(side * d.bodyWidth * 0.95f,
                                         -d.bodyHeight * 0.10f,
                                         -d.bodyLength * 0.08f));
    hip.scale(d.bodyWidth * 0.45f, d.bodyHeight * 0.42f, d.bodyLength * 0.26f);
    QVector3D hipColor =
        coatGradient(v.coatColor, 0.58f, -0.18f, coatSeedB + side * 0.06f);
    out.mesh(getUnitSphere(), hip, hipColor, nullptr, 1.0f);

    QMatrix4x4 haunch = ctx.model;
    haunch.translate(rumpCenter +
                     QVector3D(side * d.bodyWidth * 0.88f,
                               d.bodyHeight * 0.24f,
                               -d.bodyLength * 0.20f));
    haunch.scale(QVector3D(d.bodyWidth * 0.32f, d.bodyHeight * 0.28f,
                           d.bodyLength * 0.18f));
    QVector3D haunchColor =
        coatGradient(v.coatColor, 0.72f, -0.26f, coatSeedC + side * 0.04f);
    out.mesh(getUnitSphere(), haunch, lighten(haunchColor, 1.02f), nullptr,
             1.0f);
  }

  QVector3D withersPeak = chestCenter +
                          QVector3D(0.0f, d.bodyHeight * 0.62f,
                                    -d.bodyLength * 0.06f);
  QVector3D croupPeak = rumpCenter + QVector3D(0.0f, d.bodyHeight * 0.46f,
                                               -d.bodyLength * 0.18f);

  {
    QMatrix4x4 spine = ctx.model;
    spine.translate(lerp(withersPeak, croupPeak, 0.42f));
    spine.scale(QVector3D(d.bodyWidth * 0.50f, d.bodyHeight * 0.14f,
                          d.bodyLength * 0.54f));
    QVector3D spineColor =
        coatGradient(v.coatColor, 0.74f, -0.06f, coatSeedD * 0.92f);
    out.mesh(getUnitSphere(), spine, spineColor, nullptr, 1.0f);
  }

  for (int i = 0; i < 2; ++i) {
    float side = (i == 0) ? 1.0f : -1.0f;
    QVector3D scapulaTop = withersPeak +
                           QVector3D(side * d.bodyWidth * 0.52f,
                                     d.bodyHeight * 0.08f,
                                     d.bodyLength * 0.06f);
    QVector3D scapulaBase = chestCenter +
                            QVector3D(side * d.bodyWidth * 0.70f,
                                      -d.bodyHeight * 0.02f,
                                      d.bodyLength * 0.06f);
    QVector3D scapulaMid = lerp(scapulaTop, scapulaBase, 0.55f);
    drawCylinder(out, ctx.model, scapulaTop, scapulaMid, d.bodyWidth * 0.18f,
                 coatGradient(v.coatColor, 0.82f, 0.16f,
                              coatSeedA + side * 0.05f));

    QMatrix4x4 shoulderCap = ctx.model;
    shoulderCap.translate(scapulaBase +
                          QVector3D(0.0f, d.bodyHeight * 0.04f,
                                    d.bodyLength * 0.02f));
    shoulderCap.scale(QVector3D(d.bodyWidth * 0.32f, d.bodyHeight * 0.24f,
                                d.bodyLength * 0.18f));
    out.mesh(getUnitSphere(), shoulderCap,
             coatGradient(v.coatColor, 0.66f, 0.12f,
                          coatSeedB + side * 0.07f),
             nullptr, 1.0f);
  }

  {
    QMatrix4x4 sternum = ctx.model;
    sternum.translate(barrelCenter + QVector3D(0.0f, -d.bodyHeight * 0.40f,
                                               d.bodyLength * 0.28f));
    sternum.scale(QVector3D(d.bodyWidth * 0.50f, d.bodyHeight * 0.14f,
                            d.bodyLength * 0.12f));
    out.mesh(getUnitSphere(), sternum,
             coatGradient(v.coatColor, 0.18f, 0.18f, coatSeedA * 0.4f),
             nullptr, 1.0f);
  }

  QVector3D neckBase =
      chestCenter + QVector3D(0.0f, d.bodyHeight * 0.38f, d.bodyLength * 0.06f);
  QVector3D neckTop = neckBase + QVector3D(0.0f, d.neckRise, d.neckLength);
  float neckRadius = d.bodyWidth * 0.42f;

  QVector3D neckMid =
      lerp(neckBase, neckTop, 0.55f) +
      QVector3D(0.0f, d.bodyHeight * 0.02f, d.bodyLength * 0.02f);
  QVector3D neckColorBase =
      coatGradient(v.coatColor, 0.78f, 0.12f, coatSeedC * 0.6f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(ctx.model, neckBase, neckMid, neckRadius * 1.00f),
           neckColorBase, nullptr, 1.0f);
  out.mesh(getUnitCylinder(),
          cylinderBetween(ctx.model, neckMid, neckTop, neckRadius * 0.86f),
           lighten(neckColorBase, 1.03f), nullptr, 1.0f);

  {
    QVector3D jugularStart = lerp(neckBase, neckTop, 0.42f) +
                             QVector3D(d.bodyWidth * 0.18f,
                                       -d.bodyHeight * 0.06f,
                                       d.bodyLength * 0.04f);
    QVector3D jugularEnd = jugularStart +
                           QVector3D(0.0f, -d.bodyHeight * 0.24f,
                                     d.bodyLength * 0.06f);
    drawCylinder(out, ctx.model, jugularStart, jugularEnd,
                 neckRadius * 0.18f,
                 lighten(neckColorBase, 1.08f), 0.85f);
  }

  // Mane cards along the neck (compute after neck base/top defined)
  const int maneSections = 8;
  QVector3D maneColor = lerp3(v.maneColor, QVector3D(0.12f, 0.09f, 0.08f),
                              0.35f);
  for (int i = 0; i < maneSections; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(maneSections - 1);
    QVector3D spine = lerp(neckBase, neckTop, t) +
                      QVector3D(0.0f, d.bodyHeight * 0.12f, 0.0f);
    float length = lerp(0.14f, 0.08f, t) * d.bodyHeight * 1.4f;
    QVector3D tip = spine + QVector3D(0.0f, length * 1.2f, 0.02f * length);
    drawCone(out, ctx.model, tip, spine, d.bodyWidth * lerp(0.25f, 0.12f, t),
             maneColor, 1.0f);
  }

  QVector3D headCenter =
      neckTop + QVector3D(0.0f, d.headHeight * (0.10f - headNod * 0.15f),
                          d.headLength * 0.40f);

  {
    QMatrix4x4 skull = ctx.model;
    skull.translate(headCenter + QVector3D(0.0f, d.headHeight * 0.10f,
                                           -d.headLength * 0.10f));
    skull.scale(d.headWidth * 0.95f, d.headHeight * 0.90f,
                d.headLength * 0.80f);
    QVector3D skullColor =
        coatGradient(v.coatColor, 0.82f, 0.30f, coatSeedD * 0.8f);
    out.mesh(getUnitSphere(), skull, skullColor, nullptr, 1.0f);
  }

  for (int i = 0; i < 2; ++i) {
    float side = (i == 0) ? 1.0f : -1.0f;
    QMatrix4x4 cheek = ctx.model;
    cheek.translate(headCenter + QVector3D(side * d.headWidth * 0.55f,
                                           -d.headHeight * 0.15f, 0.0f));
    cheek.scale(d.headWidth * 0.45f, d.headHeight * 0.50f,
                d.headLength * 0.60f);
    QVector3D cheekColor =
        coatGradient(v.coatColor, 0.70f, 0.18f, coatSeedA * 0.9f);
    out.mesh(getUnitSphere(), cheek, cheekColor, nullptr, 1.0f);
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

  // Simple bridle straps
  QVector3D bridleBase = muzzleCenter + QVector3D(0.0f, -d.headHeight * 0.05f,
                                                 d.muzzleLength * 0.20f);
  QVector3D cheekAnchorLeft = headCenter +
                              QVector3D(d.headWidth * 0.55f,
                                        d.headHeight * 0.05f,
                                        -d.headLength * 0.05f);
  QVector3D cheekAnchorRight = headCenter +
                               QVector3D(-d.headWidth * 0.55f,
                                         d.headHeight * 0.05f,
                                         -d.headLength * 0.05f);
  QVector3D brow = headCenter +
                   QVector3D(0.0f, d.headHeight * 0.38f,
                             -d.headLength * 0.28f);
  QVector3D tackColor = lighten(v.tackColor, 0.9f);
  drawCylinder(out, ctx.model, bridleBase, cheekAnchorLeft,
               d.headWidth * 0.07f, tackColor);
  drawCylinder(out, ctx.model, bridleBase, cheekAnchorRight,
               d.headWidth * 0.07f, tackColor);
  drawCylinder(out, ctx.model, cheekAnchorLeft, brow, d.headWidth * 0.05f,
               tackColor);
  drawCylinder(out, ctx.model, cheekAnchorRight, brow, d.headWidth * 0.05f,
               tackColor);

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

  {
    QVector3D forelockBase = headCenter +
                             QVector3D(0.0f, d.headHeight * 0.28f,
                                       -d.headLength * 0.18f);
    for (int i = 0; i < 3; ++i) {
      float offset = (i - 1) * d.headWidth * 0.10f;
      QVector3D strandBase = forelockBase + QVector3D(offset, 0.0f, 0.0f);
      QVector3D strandTip = strandBase +
                            QVector3D(offset * 0.4f,
                                      -d.headHeight * 0.25f,
                                      d.headLength * 0.12f);
      drawCone(out, ctx.model, strandTip, strandBase,
               d.headWidth * 0.10f,
               v.maneColor * (0.94f + 0.03f * i), 0.96f);
    }
  }

  QVector3D tailBase =
      rumpCenter + QVector3D(0.0f, d.bodyHeight * 0.36f, -d.bodyLength * 0.48f);
  QVector3D tailCtrl = tailBase + QVector3D(0.0f, -d.tailLength * 0.20f,
                                            -d.tailLength * 0.28f);
  QVector3D tailEnd = tailBase + QVector3D(0.0f, -d.tailLength,
                                           -d.tailLength * 0.70f);
  QVector3D tailColor = lerp3(v.tailColor, v.maneColor, 0.35f);
  QVector3D prevTail = tailBase;
  for (int i = 1; i <= 8; ++i) {
    float t = static_cast<float>(i) / 8.0f;
    QVector3D p = bezier(tailBase, tailCtrl, tailEnd, t);
    float swing = (anim.isMoving ? std::sin((phase + t * 0.12f) * 2.0f * kPi)
                                 : std::sin((phase * 0.7f + t * 0.3f) * 2.0f * kPi)) *
                  (0.04f + 0.015f * (1.0f - t));
    p.setX(p.x() + swing);
    float radius = d.bodyWidth * (0.20f - 0.018f * i);
    drawCylinder(out, ctx.model, prevTail, p, radius, tailColor);
    prevTail = p;
  }

  {
    QMatrix4x4 tailKnot = ctx.model;
    tailKnot.translate(tailBase + QVector3D(0.0f, -d.bodyHeight * 0.06f,
                                            -d.bodyLength * 0.02f));
    tailKnot.scale(QVector3D(d.bodyWidth * 0.24f, d.bodyWidth * 0.18f,
                             d.bodyWidth * 0.20f));
    out.mesh(getUnitSphere(), tailKnot,
             lighten(tailColor, 0.92f), nullptr, 1.0f);
  }

  for (int i = 0; i < 3; ++i) {
    float spread = (i - 1) * d.bodyWidth * 0.14f;
    QVector3D fanBase = tailEnd + QVector3D(spread * 0.15f,
                                            -d.bodyWidth * 0.05f,
                                            -d.tailLength * 0.08f);
    QVector3D fanTip = fanBase + QVector3D(spread,
                                           -d.tailLength * 0.32f,
                                           -d.tailLength * 0.22f);
    drawCone(out, ctx.model, fanTip, fanBase, d.bodyWidth * 0.24f,
             tailColor * (0.96f + 0.02f * i), 0.88f);
  }

  auto renderHoof = [&](const QVector3D &hoofTop, const QVector3D &hoofBottom,
                        float wallRadius, const QVector3D &hoofColor,
                        bool isRear) {
    QVector3D wallTint = lighten(hoofColor, isRear ? 1.04f : 1.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, hoofTop, hoofBottom, wallRadius),
             wallTint, nullptr, 1.0f);

    QVector3D toe = hoofBottom + QVector3D(0.0f, -d.hoofHeight * 0.14f, 0.0f);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, toe, hoofBottom, wallRadius * 0.90f),
             wallTint * 0.96f, nullptr, 1.0f);

    QMatrix4x4 sole = ctx.model;
    sole.translate(lerp(hoofTop, hoofBottom, 0.88f) +
                   QVector3D(0.0f, -d.hoofHeight * 0.05f, 0.0f));
    sole.scale(QVector3D(wallRadius * 1.08f, wallRadius * 0.28f,
                         wallRadius * 1.02f));
    out.mesh(getUnitSphere(), sole, lighten(hoofColor, 1.12f), nullptr, 1.0f);

    QMatrix4x4 coronet = ctx.model;
    coronet.translate(lerp(hoofTop, hoofBottom, 0.12f));
    coronet.scale(QVector3D(wallRadius * 1.05f, wallRadius * 0.24f,
                            wallRadius * 1.05f));
    out.mesh(getUnitSphere(), coronet, lighten(hoofColor, 1.06f), nullptr,
             1.0f);
  };

  auto drawLeg = [&](const QVector3D &anchor, float lateralSign,
                     float forwardBias, float phaseOffset, float sockChance) {
    float legPhase = std::fmod(phase + phaseOffset, 1.0f);
    float stride = 0.0f;
    float lift = 0.0f;

    if (anim.isMoving) {
      float angle = legPhase * 2.0f * kPi;
      stride = std::sin(angle) * g.strideSwing * 0.75f + forwardBias;
      float liftRaw = std::sin(angle);
      lift = liftRaw > 0.0f ? liftRaw * g.strideLift
                            : liftRaw * g.strideLift * 0.22f;
    } else {
      float idle = std::sin(legPhase * 2.0f * kPi);
      stride = idle * g.strideSwing * 0.06f + forwardBias;
      lift = idle * d.idleBobAmplitude * 2.0f;
    }

    bool tightenLegs = anim.isMoving;
    float shoulderOut =
        d.bodyWidth * (tightenLegs ? 0.44f : 0.58f);
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

    float thighLength = d.legLength * (isRear ? 0.62f : 0.56f);
    float hipPitch = hipSwing * (isRear ? 0.62f : 0.50f);
    float inwardLean = tightenLegs ? (-0.06f - liftFactor * 0.045f) : -0.012f;
    QVector3D thighDir(lateralSign * inwardLean, -std::cos(hipPitch) * 0.90f,
                       (isRear ? -1.0f : 1.0f) * std::sin(hipPitch) * 0.65f);
    if (thighDir.lengthSquared() > 1e-6f)
      thighDir.normalize();

    QVector3D knee = shoulder + thighDir * thighLength;
    knee.setY(knee.y() + liftFactor * thighLength * 0.28f);

    QVector3D girdleTop =
        (isRear ? croupPeak : withersPeak) +
        QVector3D(lateralSign * d.bodyWidth * (isRear ? 0.44f : 0.48f),
                  isRear ? -d.bodyHeight * 0.06f : d.bodyHeight * 0.04f,
                  (isRear ? -d.bodyLength * 0.06f : d.bodyLength * 0.05f));
    girdleTop.setZ(girdleTop.z() + hipSwing * (isRear ? -0.08f : 0.05f));
    girdleTop.setX(girdleTop.x() - lateralSign * liftFactor * 0.03f);

    QVector3D socket = shoulder +
                       QVector3D(0.0f, d.bodyWidth * 0.12f,
                                 isRear ? -d.bodyLength * 0.03f
                                        : d.bodyLength * 0.02f);
    drawCylinder(out, ctx.model, girdleTop, socket,
                 d.bodyWidth * (isRear ? 0.20f : 0.18f),
                 coatGradient(v.coatColor, isRear ? 0.70f : 0.80f,
                              isRear ? -0.20f : 0.22f,
                              coatSeedB + lateralSign * 0.03f));

    QMatrix4x4 socketCap = ctx.model;
    socketCap.translate(socket +
                        QVector3D(0.0f, -d.bodyWidth * 0.04f,
                                  isRear ? -d.bodyLength * 0.02f
                                         : d.bodyLength * 0.03f));
    socketCap.scale(QVector3D(d.bodyWidth * (isRear ? 0.36f : 0.32f),
                              d.bodyWidth * 0.28f,
                              d.bodyLength * 0.18f));
    out.mesh(getUnitSphere(), socketCap,
             coatGradient(v.coatColor, isRear ? 0.60f : 0.68f,
                          isRear ? -0.24f : 0.18f,
                          coatSeedC + lateralSign * 0.02f),
             nullptr, 1.0f);

    float kneeFlex =
        anim.isMoving
            ? clamp01(std::sin(gallopAngle + (isRear ? 0.65f : -0.45f)) * 0.55f +
                      0.42f)
            : 0.32f;

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

    float thighBellyR = d.bodyWidth * (isRear ? 0.58f : 0.50f);
    float kneeR = d.bodyWidth * (isRear ? 0.22f : 0.20f);
    float cannonR = d.bodyWidth * 0.16f;
    float pasternR = d.bodyWidth * 0.11f;

    QVector3D thighBelly = shoulder + (knee - shoulder) * 0.62f;

    QVector3D thighColor =
        coatGradient(v.coatColor, isRear ? 0.48f : 0.58f,
                      isRear ? -0.22f : 0.18f, coatSeedA + lateralSign * 0.07f);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, thighBelly, shoulder, thighBellyR),
             thighColor, nullptr, 1.0f);

    {
      QMatrix4x4 muscle = ctx.model;
      muscle.translate(thighBelly +
                       QVector3D(0.0f, 0.0f, isRear ? -0.015f : 0.020f));
      muscle.scale(thighBellyR * QVector3D(1.05f, 0.85f, 0.92f));
      out.mesh(getUnitSphere(), muscle,
               lighten(thighColor, 1.03f), nullptr, 1.0f);
    }

    QVector3D kneeColor = darken(thighColor, 0.96f);
    out.mesh(getUnitCone(), coneFromTo(ctx.model, knee, thighBelly, kneeR),
             kneeColor, nullptr, 1.0f);

    {
      QMatrix4x4 joint = ctx.model;
      joint.translate(knee +
                      QVector3D(0.0f, 0.0f, isRear ? -0.028f : 0.034f));
      joint.scale(QVector3D(kneeR * 1.18f, kneeR * 1.06f, kneeR * 1.36f));
      out.mesh(getUnitSphere(), joint, darken(kneeColor, 0.90f), nullptr,
               1.0f);
    }

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, knee, cannon, cannonR),
             darken(thighColor, 0.93f), nullptr, 1.0f);

    {
      QMatrix4x4 tendon = ctx.model;
      tendon.translate(lerp(knee, cannon, 0.55f) +
                       QVector3D(0.0f, 0.0f,
                                 isRear ? -cannonR * 0.35f : cannonR * 0.35f));
      tendon.scale(QVector3D(cannonR * 0.45f, cannonR * 0.95f, cannonR * 0.55f));
      out.mesh(getUnitSphere(), tendon,
               darken(thighColor, isRear ? 0.88f : 0.90f), nullptr, 1.0f);
    }

    {
      QMatrix4x4 joint = ctx.model;
      joint.translate(fetlock);
      joint.scale(QVector3D(pasternR * 1.12f, pasternR * 1.05f,
                            pasternR * 1.26f));
      out.mesh(getUnitSphere(), joint, darken(thighColor, 0.92f), nullptr,
               1.0f);
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
    renderHoof(hoofTop, hoofBottom, pasternR * 0.96f, hoofColor, isRear);

    if (sock > 0.0f) {
      QVector3D featherTip = lerp(fetlock, hoofTop, 0.35f) +
                             QVector3D(0.0f, -pasternR * 0.60f, 0.0f);
      drawCone(out, ctx.model, featherTip, fetlock, pasternR * 0.85f,
               lerp(distalColor, v.coatColor, 0.25f), 0.85f);
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
  QVector3D saddleCenter(0.0f, saddleTop - d.saddleThickness * 0.35f,
                         -d.bodyLength * 0.05f + d.seatForwardOffset * 0.25f);
  {
    QMatrix4x4 saddle = ctx.model;
    saddle.translate(saddleCenter);
    saddle.scale(d.bodyWidth * 1.10f, d.saddleThickness * 1.05f,
                 d.bodyLength * 0.34f);
    out.mesh(getUnitSphere(), saddle, v.saddleColor, nullptr, 1.0f);
  }

  QVector3D blanketCenter =
      saddleCenter + QVector3D(0.0f, -d.saddleThickness, 0.0f);
  {
    QMatrix4x4 blanket = ctx.model;
    blanket.translate(blanketCenter);
    blanket.scale(d.bodyWidth * 1.26f, d.saddleThickness * 0.38f,
                  d.bodyLength * 0.42f);
    out.mesh(getUnitSphere(), blanket, v.blanketColor * 1.02f, nullptr, 1.0f);
  }

  {
    QMatrix4x4 cantle = ctx.model;
    cantle.translate(saddleCenter +
                     QVector3D(0.0f, d.saddleThickness * 0.72f,
                               -d.bodyLength * 0.12f));
    cantle.scale(QVector3D(d.bodyWidth * 0.52f, d.saddleThickness * 0.60f,
                           d.bodyLength * 0.18f));
    out.mesh(getUnitSphere(), cantle, lighten(v.saddleColor, 1.05f), nullptr,
             1.0f);
  }

  {
    QMatrix4x4 pommel = ctx.model;
    pommel.translate(saddleCenter +
                     QVector3D(0.0f, d.saddleThickness * 0.58f,
                               d.bodyLength * 0.16f));
    pommel.scale(QVector3D(d.bodyWidth * 0.40f, d.saddleThickness * 0.48f,
                           d.bodyLength * 0.14f));
    out.mesh(getUnitSphere(), pommel, darken(v.saddleColor, 0.92f), nullptr,
             1.0f);
  }

  for (int i = 0; i < 6; ++i) {
    float t = static_cast<float>(i) / 5.0f;
    QMatrix4x4 stitch = ctx.model;
    stitch.translate(blanketCenter +
                     QVector3D(d.bodyWidth * (t - 0.5f) * 1.1f,
                               -d.saddleThickness * 0.35f,
                               d.bodyLength * 0.28f));
    stitch.scale(QVector3D(d.bodyWidth * 0.05f, d.bodyWidth * 0.02f,
                           d.bodyWidth * 0.12f));
    out.mesh(getUnitSphere(), stitch, v.blanketColor * 0.75f, nullptr, 0.9f);
  }

  for (int i = 0; i < 2; ++i) {
    float side = (i == 0) ? 1.0f : -1.0f;
    QVector3D strapTop =
        saddleCenter +
        QVector3D(side * d.bodyWidth * 0.92f, d.saddleThickness * 0.32f,
                   d.bodyLength * 0.02f);
    QVector3D strapBottom =
        strapTop + QVector3D(0.0f, -d.bodyHeight * 0.94f, -d.bodyLength * 0.06f);
    out.mesh(
        getUnitCylinder(),
        cylinderBetween(ctx.model, strapTop, strapBottom, d.bodyWidth * 0.065f),
        v.tackColor * 0.94f, nullptr, 1.0f);

    QMatrix4x4 buckle = ctx.model;
    buckle.translate(lerp(strapTop, strapBottom, 0.87f) +
                     QVector3D(0.0f, 0.0f, -d.bodyLength * 0.02f));
    buckle.scale(QVector3D(d.bodyWidth * 0.16f, d.bodyWidth * 0.12f,
                           d.bodyWidth * 0.05f));
    out.mesh(getUnitSphere(), buckle, QVector3D(0.42f, 0.39f, 0.35f), nullptr,
             1.0f);
  }

  for (int i = 0; i < 2; ++i) {
    float side = (i == 0) ? 1.0f : -1.0f;
    QVector3D breastAnchor = chestCenter +
                             QVector3D(side * d.bodyWidth * 0.70f,
                                       -d.bodyHeight * 0.10f,
                                       d.bodyLength * 0.18f);
    QVector3D breastToSaddle = saddleCenter +
                               QVector3D(side * d.bodyWidth * 0.48f,
                                         -d.saddleThickness * 0.20f,
                                         d.bodyLength * 0.10f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, breastAnchor, breastToSaddle,
                             d.bodyWidth * 0.055f),
             v.tackColor * 0.92f, nullptr, 1.0f);
  }

  QVector3D stirrupAttachLeft =
      saddleCenter + QVector3D(d.bodyWidth * 0.92f,
                               -d.saddleThickness * 0.10f,
                               d.seatForwardOffset * 0.28f);
  QVector3D stirrupAttachRight =
      saddleCenter + QVector3D(-d.bodyWidth * 0.92f,
                               -d.saddleThickness * 0.10f,
                               d.seatForwardOffset * 0.28f);
  QVector3D stirrupBottomLeft =
      stirrupAttachLeft + QVector3D(0.0f, -d.stirrupDrop, 0.0f);
  QVector3D stirrupBottomRight =
      stirrupAttachRight + QVector3D(0.0f, -d.stirrupDrop, 0.0f);

  auto drawRider = [&]() {
    QVector3D riderCoat(0.23f, 0.23f, 0.26f);
    QVector3D riderCloth = lighten(v.blanketColor, 1.15f);
    QVector3D riderSkin(1.0f, 0.86f, 0.72f);
    QVector3D riderLeather = darken(v.saddleColor, 0.88f);
    QVector3D riderSteel(0.72f, 0.73f, 0.78f);

    QVector3D pelvisCenter = saddleCenter +
                             QVector3D(riderLean * d.bodyWidth,
                                       d.saddleThickness * 0.68f,
                                       -d.bodyLength * 0.08f);
    QVector3D spineMid = pelvisCenter +
                         QVector3D(riderLean * d.bodyWidth * 0.35f,
                                   d.bodyHeight * 0.32f,
                                   d.bodyLength * 0.02f);
    QVector3D torsoTop = spineMid +
                         QVector3D(riderLean * d.bodyWidth * 0.25f,
                                   d.bodyHeight * 0.28f,
                                   0.03f);
    QVector3D neckBase = torsoTop + QVector3D(0.0f, d.bodyHeight * 0.10f,
                                             0.02f);

    QMatrix4x4 pelvis = ctx.model;
    pelvis.translate(pelvisCenter);
    pelvis.scale(QVector3D(d.bodyWidth * 0.52f, d.bodyWidth * 0.34f,
                           d.bodyWidth * 0.48f));
    out.mesh(getUnitSphere(), pelvis, riderCoat * 0.92f, nullptr, 1.0f);

    drawCylinder(out, ctx.model, pelvisCenter, spineMid,
                 d.bodyWidth * 0.36f, riderCoat * 0.96f);
    drawCylinder(out, ctx.model, spineMid, torsoTop, d.bodyWidth * 0.30f,
                 riderCoat * 0.98f);

    {
      QMatrix4x4 chest = ctx.model;
      chest.translate(spineMid + QVector3D(0.0f, d.bodyHeight * 0.20f, 0.0f));
      chest.scale(QVector3D(d.bodyWidth * 0.60f, d.bodyWidth * 0.46f,
                            d.bodyWidth * 0.32f));
      out.mesh(getUnitSphere(), chest, riderCoat * 1.02f, nullptr, 1.0f);
    }

    for (int i = 0; i < 2; ++i) {
      float side = (i == 0) ? 1.0f : -1.0f;
      QMatrix4x4 shoulderPad = ctx.model;
      shoulderPad.translate(torsoTop +
                            QVector3D(side * d.bodyWidth * 0.40f,
                                      -d.bodyWidth * 0.02f,
                                      0.02f));
      shoulderPad.scale(QVector3D(d.bodyWidth * 0.22f, d.bodyWidth * 0.16f,
                                  d.bodyWidth * 0.18f));
      out.mesh(getUnitSphere(), shoulderPad, riderCloth * 0.92f, nullptr,
               1.0f);
    }

    QVector3D handLeftTarget =
        bridleBase + QVector3D(d.bodyWidth * 0.08f, -d.headHeight * 0.06f,
                               d.headLength * 0.22f);
    handLeftTarget.setY(handLeftTarget.y() - reinSlack * 0.45f);
    handLeftTarget.setZ(handLeftTarget.z() - reinSlack * 0.12f);

    QVector3D swordGrip =
        pelvisCenter + QVector3D(-d.bodyWidth * (0.54f - riderLean * 0.20f),
                                 d.bodyHeight * 0.46f,
                                 d.bodyLength * 0.12f);
    QVector3D handRightTarget =
        swordGrip + QVector3D(0.0f, -d.bodyWidth * 0.05f, 0.0f);

    auto drawArm = [&](float sideSign, const QVector3D &handTarget,
                       bool swordHand) {
      QVector3D shoulder = torsoTop +
                           QVector3D(sideSign * d.bodyWidth * 0.42f,
                                     -d.bodyWidth * 0.04f,
                                     d.bodyLength * 0.03f);
      if (swordHand) {
        shoulder.setZ(shoulder.z() - d.bodyLength * 0.06f);
        shoulder.setY(shoulder.y() + d.bodyWidth * 0.02f);
      } else {
        shoulder.setZ(shoulder.z() + reinSlack * 0.20f * sideSign);
      }

      QVector3D elbow = shoulder +
                        QVector3D(sideSign * d.bodyWidth * (swordHand ? 0.20f : 0.14f),
                                  -d.bodyWidth * (swordHand ? 0.32f : 0.26f),
                                  d.bodyLength * (swordHand ? 0.02f : 0.10f));
      if (!swordHand) {
        elbow.setZ(elbow.z() + reinSlack * 0.12f * sideSign);
      } else {
        elbow.setX(elbow.x() + sideSign * d.bodyWidth * 0.02f);
      }

      QVector3D wrist = handTarget +
                        QVector3D(sideSign * d.bodyWidth * (swordHand ? 0.01f : 0.02f),
                                  -d.bodyWidth * 0.03f,
                                  -d.bodyLength * (swordHand ? -0.01f : 0.02f));

      drawCylinder(out, ctx.model, shoulder, elbow, d.bodyWidth * 0.11f,
                   riderCloth * (swordHand ? 0.98f : 0.96f));
      drawCylinder(out, ctx.model, elbow, wrist,
                   d.bodyWidth * (swordHand ? 0.085f : 0.09f),
                   riderCloth * 0.90f);

      if (!swordHand) {
        drawCylinder(out, ctx.model, wrist, handTarget, d.bodyWidth * 0.075f,
                     riderLeather * 0.85f);
      } else {
        drawCylinder(out, ctx.model, wrist, handTarget, d.bodyWidth * 0.070f,
                     riderLeather * 0.92f);
      }

      QMatrix4x4 glove = ctx.model;
      QVector3D gloveOffset =
          swordHand ? QVector3D(0.0f, -d.bodyWidth * 0.04f,
                                d.bodyWidth * 0.02f)
                    : QVector3D(0.0f, -d.bodyWidth * 0.05f, 0.0f);
      glove.translate(handTarget + gloveOffset);
      glove.scale(QVector3D(d.bodyWidth * 0.11f, d.bodyWidth * 0.14f,
                            d.bodyWidth * 0.09f));
      QVector3D gloveColor = swordHand ? riderLeather * 0.96f
                                       : riderSkin * 0.96f;
      out.mesh(getUnitSphere(), glove, gloveColor, nullptr, 1.0f);
    };

    drawArm(1.0f, handLeftTarget, false);
    drawArm(-1.0f, handRightTarget, true);

    QVector3D helmetTop = neckBase + QVector3D(0.0f, d.bodyHeight * 0.32f,
                                               0.04f);
    QMatrix4x4 neck = ctx.model;
    neck.translate(lerp(torsoTop, neckBase, 0.6f));
    neck.scale(QVector3D(d.bodyWidth * 0.22f, d.bodyWidth * 0.24f,
                         d.bodyWidth * 0.20f));
    out.mesh(getUnitSphere(), neck, riderSkin * 0.88f, nullptr, 1.0f);

    QMatrix4x4 head = ctx.model;
    head.translate(helmetTop + QVector3D(0.0f, -d.bodyWidth * 0.12f, 0.0f));
    head.scale(d.bodyWidth * 0.30f);
    out.mesh(getUnitSphere(), head, riderSkin * 0.95f, nullptr, 1.0f);

    QMatrix4x4 helm = ctx.model;
    helm.translate(helmetTop + QVector3D(0.0f, d.bodyWidth * 0.08f, 0.0f));
    helm.scale(d.bodyWidth * 0.34f, d.bodyWidth * 0.18f, d.bodyWidth * 0.34f);
    out.mesh(getUnitSphere(), helm, riderCloth * 0.82f, nullptr, 1.0f);

    QMatrix4x4 visor = ctx.model;
    visor.translate(helmetTop + QVector3D(0.0f, d.bodyWidth * 0.02f,
                                          d.bodyWidth * 0.15f));
    visor.scale(QVector3D(d.bodyWidth * 0.32f, d.bodyWidth * 0.08f,
                          d.bodyWidth * 0.16f));
    out.mesh(getUnitSphere(), visor, riderCoat * 0.75f, nullptr, 1.0f);

    auto drawLeg = [&](float sideSign,
                       const QVector3D &stirrupBottom) {
      QVector3D hip = pelvisCenter +
                      QVector3D(sideSign * d.bodyWidth * 0.34f,
                                -d.bodyWidth * 0.01f,
                                d.bodyWidth * 0.06f);
      QVector3D knee = hip +
                       QVector3D(sideSign * d.bodyWidth * 0.08f,
                                 -d.stirrupDrop * 0.74f,
                                 d.bodyLength * 0.18f);
      QVector3D ankle = stirrupBottom +
                        QVector3D(sideSign * d.bodyWidth * 0.02f,
                                  d.bodyWidth * 0.05f,
                                  d.bodyWidth * 0.05f);
      QVector3D toe = ankle +
                      QVector3D(sideSign * d.bodyWidth * 0.12f,
                                -d.bodyWidth * 0.04f,
                                d.bodyWidth * 0.10f);

      drawCylinder(out, ctx.model, hip, knee, d.bodyWidth * 0.12f,
                   riderCloth * 0.96f);
      drawCylinder(out, ctx.model, knee, ankle, d.bodyWidth * 0.095f,
                   riderCloth * 0.90f);

      QMatrix4x4 kneeCap = ctx.model;
      kneeCap.translate(knee);
      kneeCap.scale(QVector3D(d.bodyWidth * 0.12f, d.bodyWidth * 0.10f,
                              d.bodyWidth * 0.14f));
      out.mesh(getUnitSphere(), kneeCap, riderCloth * 0.86f, nullptr, 1.0f);

      drawCylinder(out, ctx.model, ankle, toe, d.bodyWidth * 0.08f,
                   riderCoat * 0.75f);

      QMatrix4x4 boot = ctx.model;
      boot.translate(ankle + QVector3D(0.0f, -d.bodyWidth * 0.06f, 0.0f));
      boot.scale(QVector3D(d.bodyWidth * 0.16f, d.bodyWidth * 0.14f,
                           d.bodyWidth * 0.20f));
      out.mesh(getUnitSphere(), boot, riderLeather, nullptr, 1.0f);

      QMatrix4x4 spur = ctx.model;
      spur.translate(ankle + QVector3D(-sideSign * d.bodyWidth * 0.10f,
                                       -d.bodyWidth * 0.01f,
                                       -d.bodyWidth * 0.06f));
      spur.scale(QVector3D(d.bodyWidth * 0.06f, d.bodyWidth * 0.06f,
                           d.bodyWidth * 0.08f));
      out.mesh(getUnitSphere(), spur, QVector3D(0.62f, 0.62f, 0.64f), nullptr,
               1.0f);
    };

    drawLeg(1.0f, stirrupBottomLeft);
    drawLeg(-1.0f, stirrupBottomRight);

    drawCylinder(out, ctx.model, handLeftTarget,
                 bridleBase + QVector3D(0.0f, -d.headHeight * 0.02f, 0.0f),
                 d.bodyWidth * 0.038f, riderLeather * 0.75f, 0.90f);

    QVector3D swordHandleTop =
        swordGrip + QVector3D(-d.bodyWidth * 0.02f, d.bodyHeight * 0.18f,
                              d.bodyLength * 0.04f);
    QVector3D swordHandleBottom =
        swordGrip + QVector3D(0.0f, -d.bodyWidth * 0.08f,
                              -d.bodyLength * 0.02f);

    drawCylinder(out, ctx.model, swordGrip, swordHandleTop,
                 d.bodyWidth * 0.045f, riderLeather * 0.88f);

    QMatrix4x4 pommel = ctx.model;
    pommel.translate(swordHandleBottom);
    pommel.scale(d.bodyWidth * 0.12f);
    out.mesh(getUnitSphere(), pommel, riderLeather * 0.75f, nullptr, 1.0f);

    QVector3D guardCenter = swordHandleTop +
                            QVector3D(0.0f, d.bodyWidth * 0.015f, 0.0f);
    QVector3D guardL = guardCenter + QVector3D(d.bodyWidth * 0.18f,
                                              d.bodyWidth * 0.03f,
                                              -d.bodyWidth * 0.02f);
    QVector3D guardR = guardCenter + QVector3D(-d.bodyWidth * 0.18f,
                                              d.bodyWidth * 0.03f,
                                              -d.bodyWidth * 0.02f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, guardL, guardR, d.bodyWidth * 0.020f),
             riderSteel * 1.05f, nullptr, 1.0f);

    QMatrix4x4 guardCore = ctx.model;
    guardCore.translate(guardCenter);
    guardCore.scale(d.bodyWidth * 0.05f);
    out.mesh(getUnitSphere(), guardCore, riderSteel * 1.08f, nullptr, 1.0f);

    QVector3D bladeBase = guardCenter +
                          QVector3D(-d.bodyWidth * 0.01f, d.bodyWidth * 0.02f,
                                    d.bodyWidth * 0.01f);
    QVector3D bladeCtrl = bladeBase +
                          QVector3D(-d.bodyWidth * 0.14f,
                                    d.bodyHeight * 0.55f,
                                    d.bodyLength * 0.28f);
    QVector3D bladeTip = bladeBase +
                         QVector3D(-d.bodyWidth * 0.06f,
                                   d.bodyHeight * 0.95f,
                                   d.bodyLength * 0.36f);

    QVector3D prev = bladeBase;
    const int bladeSegments = 6;
    for (int i = 1; i <= bladeSegments; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(bladeSegments);
      QVector3D p = bezier(bladeBase, bladeCtrl, bladeTip, t);
      float radius = d.bodyWidth * lerp(0.060f, 0.020f, t);
      QVector3D bladeColor = riderSteel * (1.08f - 0.10f * t) +
                             QVector3D(0.02f, 0.02f, 0.02f) * t;
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, prev, p, radius),
               bladeColor, nullptr, 1.0f);
      prev = p;
    }

    out.mesh(getUnitCone(),
             coneFromTo(ctx.model,
                        bladeTip + QVector3D(-d.bodyWidth * 0.02f,
                                             d.bodyWidth * 0.08f,
                                             -d.bodyWidth * 0.02f),
                        bladeTip, d.bodyWidth * 0.020f),
             riderSteel * 1.12f, nullptr, 1.0f);
  };

  drawRider();

  auto drawStirrup = [&](const QVector3D &attach,
                         const QVector3D &bottom) {
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, attach, bottom, d.bodyWidth * 0.048f),
             v.tackColor * 0.98f, nullptr, 1.0f);

    QMatrix4x4 leatherLoop = ctx.model;
    leatherLoop.translate(lerp(attach, bottom, 0.30f) +
                          QVector3D(0.0f, 0.0f, d.bodyWidth * 0.02f));
    leatherLoop.scale(QVector3D(d.bodyWidth * 0.18f, d.bodyWidth * 0.05f,
                                d.bodyWidth * 0.10f));
    out.mesh(getUnitSphere(), leatherLoop, v.tackColor * 0.92f, nullptr,
             1.0f);

    QMatrix4x4 stirrup = ctx.model;
    stirrup.translate(bottom + QVector3D(0.0f, -d.bodyWidth * 0.06f, 0.0f));
    stirrup.scale(d.bodyWidth * 0.20f, d.bodyWidth * 0.07f,
                  d.bodyWidth * 0.16f);
    out.mesh(getUnitSphere(), stirrup, QVector3D(0.66f, 0.65f, 0.62f), nullptr,
             1.0f);
  };

  drawStirrup(stirrupAttachLeft, stirrupBottomLeft);
  drawStirrup(stirrupAttachRight, stirrupBottomRight);

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
    QVector3D reinEnd = saddleCenter + QVector3D(side * d.bodyWidth * 0.62f,
                                                 -d.saddleThickness * 0.32f,
                                                 d.seatForwardOffset * 0.10f);

    QVector3D mid = lerp(reinStart, reinEnd, 0.46f) +
                    QVector3D(0.0f, -d.bodyHeight * (0.08f + reinSlack),
                              0.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, reinStart, mid, d.bodyWidth * 0.02f),
             v.tackColor * 0.95f, nullptr, 1.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, mid, reinEnd, d.bodyWidth * 0.02f),
             v.tackColor * 0.95f, nullptr, 1.0f);
  }
}

} // namespace Render::GL
