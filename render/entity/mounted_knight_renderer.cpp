#include "mounted_knight_renderer.h"
#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../gl/backend.h"
#include "../gl/mesh.h"
#include "../gl/primitives.h"
#include "../gl/shader.h"
#include "../humanoid_base.h"
#include "../humanoid_math.h"
#include "../humanoid_specs.h"
#include "../palette.h"
#include "../scene_renderer.h"
#include "../submitter.h"
#include "registry.h"
#include <unordered_map>

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Render::GL {

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

static constexpr std::size_t MAX_EXTRAS_CACHE_SIZE = 10000;

static inline float easeInOutCubic(float t) {
  t = clamp01(t);
  return t < 0.5f ? 4.0f * t * t * t
                  : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

static inline float smoothstep(float a, float b, float x) {
  x = clamp01((x - a) / (b - a));
  return x * x * (3.0f - 2.0f * x);
}

static inline float lerp(float a, float b, float t) {
  return a * (1.0f - t) + b * t;
}

static inline QVector3D nlerp(const QVector3D &a, const QVector3D &b, float t) {
  QVector3D v = a * (1.0f - t) + b * t;
  if (v.lengthSquared() > 1e-6f)
    v.normalize();
  return v;
}

struct MountedKnightExtras {
  QVector3D metalColor;
  QVector3D horseColor;
  QVector3D saddleColor;
  float lanceLength = 1.20f;
  float lanceWidth = 0.035f;
  bool hasLance = true;
  bool hasCavalryShield = false;
};

class MountedKnightRenderer : public HumanoidRendererBase {
public:
  QVector3D getProportionScaling() const override {
    return QVector3D(1.40f, 1.05f, 1.10f);
  }

private:
  mutable std::unordered_map<uint32_t, MountedKnightExtras> m_extrasCache;

public:
  void getVariant(const DrawContext &ctx, uint32_t seed,
                  HumanoidVariant &v) const override {
    QVector3D teamTint = resolveTeamTint(ctx);
    v.palette = makeHumanoidPalette(teamTint, seed);
  }

  void customizePose(const DrawContext &ctx, const AnimationInputs &anim,
                     uint32_t seed, HumanoidPose &pose) const override {
    using HP = HumanProportions;

    float armHeightJitter = (hash01(seed ^ 0xABCDu) - 0.5f) * 0.03f;
    float armAsymmetry = (hash01(seed ^ 0xDEF0u) - 0.5f) * 0.04f;

    const float saddleHeight = -0.12f;
    const float offsetY = saddleHeight - pose.pelvisPos.y();

    if (anim.isAttacking && anim.isMelee) {
      const float attackCycleTime = 0.8f;
      float attackPhase = std::fmod(anim.time * (1.0f / attackCycleTime), 1.0f);

      QVector3D restPos(0.35f, HP::SHOULDER_Y + 0.10f + offsetY, 0.20f);
      QVector3D preparePos(0.40f, HP::HEAD_TOP_Y + 0.25f + offsetY, -0.10f);
      QVector3D raisedPos(0.38f, HP::HEAD_TOP_Y + 0.30f + offsetY, 0.05f);
      QVector3D strikePos(0.45f, HP::SHOULDER_Y - 0.10f + offsetY, 0.70f);
      QVector3D recoverPos(0.35f, HP::SHOULDER_Y + 0.05f + offsetY, 0.30f);

      if (attackPhase < 0.20f) {
        float t = easeInOutCubic(attackPhase / 0.20f);
        pose.handR = restPos * (1.0f - t) + preparePos * t;
        pose.handL = QVector3D(
            -0.28f, HP::SHOULDER_Y - 0.02f - 0.03f * t + offsetY, 0.10f);
      } else if (attackPhase < 0.35f) {
        float t = easeInOutCubic((attackPhase - 0.20f) / 0.15f);
        pose.handR = preparePos * (1.0f - t) + raisedPos * t;
        pose.handL = QVector3D(-0.28f, HP::SHOULDER_Y - 0.05f + offsetY, 0.12f);
      } else if (attackPhase < 0.55f) {
        float t = (attackPhase - 0.35f) / 0.20f;
        t = t * t * t;
        pose.handR = raisedPos * (1.0f - t) + strikePos * t;
        pose.handL = QVector3D(
            -0.28f, HP::SHOULDER_Y - 0.03f * (1.0f - 0.5f * t) + offsetY,
            0.12f + 0.25f * t);
      } else if (attackPhase < 0.75f) {
        float t = easeInOutCubic((attackPhase - 0.55f) / 0.20f);
        pose.handR = strikePos * (1.0f - t) + recoverPos * t;
        pose.handL =
            QVector3D(-0.26f, HP::SHOULDER_Y - 0.015f * (1.0f - t) + offsetY,
                      lerp(0.37f, 0.15f, t));
      } else {
        float t = smoothstep(0.75f, 1.0f, attackPhase);
        pose.handR = recoverPos * (1.0f - t) + restPos * t;
        pose.handL =
            QVector3D(-0.26f - 0.02f * (1.0f - t),
                      HP::SHOULDER_Y + armHeightJitter * (1.0f - t) + offsetY,
                      lerp(0.15f, 0.10f, t));
      }
    } else {
      pose.handR =
          QVector3D(0.35f + armAsymmetry,
                    HP::SHOULDER_Y + 0.00f + armHeightJitter + offsetY, 0.40f);
      pose.handL =
          QVector3D(-0.28f - 0.5f * armAsymmetry,
                    HP::SHOULDER_Y + 0.5f * armHeightJitter + offsetY, 0.12f);
    }

    pose.headPos.setY(pose.headPos.y() + offsetY);
    pose.neckBase.setY(pose.neckBase.y() + offsetY);
    pose.shoulderL.setY(pose.shoulderL.y() + offsetY);
    pose.shoulderR.setY(pose.shoulderR.y() + offsetY);
    pose.pelvisPos.setY(saddleHeight);

    pose.shoulderL.setZ(0.0f);
    pose.shoulderR.setZ(0.0f);

    const float shoulderWidth = HP::TORSO_TOP_R * 0.98f;
    pose.shoulderL.setX(-shoulderWidth);
    pose.shoulderR.setX(shoulderWidth);

    const float stirrupFootSpacing = 0.20f;
    pose.footL = QVector3D(-stirrupFootSpacing, -0.22f, 0.0f);
    pose.footR = QVector3D(stirrupFootSpacing, -0.22f, 0.0f);

    const float kneeY = (pose.pelvisPos.y() + pose.footL.y()) * 0.5f;
    pose.kneeL = QVector3D(pose.footL.x(), kneeY, 0.0f);
    pose.kneeR = QVector3D(pose.footR.x(), kneeY, 0.0f);
  }

  void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose, const AnimationInputs &anim,
                      ISubmitter &out) const override {
    uint32_t seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFu;

    MountedKnightExtras extras;
    auto it = m_extrasCache.find(seed);
    if (it != m_extrasCache.end()) {
      extras = it->second;
    } else {
      extras = computeMountedKnightExtras(seed, v);
      m_extrasCache[seed] = extras;

      if (m_extrasCache.size() > MAX_EXTRAS_CACHE_SIZE) {
        m_extrasCache.clear();
      }
    }

    drawHorse(ctx, pose, v, extras, anim, out);

    bool isAttacking = anim.isAttacking && anim.isMelee;
    float attackPhase = 0.0f;
    if (isAttacking) {
      float attackCycleTime = 0.8f;
      attackPhase = std::fmod(anim.time * (1.0f / attackCycleTime), 1.0f);
    }

    if (extras.hasLance) {
      drawLance(ctx, pose, v, extras, isAttacking, attackPhase, out);
    }

    if (extras.hasCavalryShield) {
      drawCavalryShield(ctx, pose, v, extras, out);
    }
  }

  void drawHelmet(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose, ISubmitter &out) const override {
    using HP = HumanProportions;

    auto ring = [&](const QVector3D &center, float r, float h,
                    const QVector3D &col) {
      QVector3D a = center + QVector3D(0, h * 0.5f, 0);
      QVector3D b = center - QVector3D(0, h * 0.5f, 0);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r), col,
               nullptr, 1.0f);
    };

    QVector3D steelColor = v.palette.metal * QVector3D(0.95f, 0.96f, 1.0f);

    float helmR = pose.headR * 1.15f;
    QVector3D helmBot(0, pose.headPos.y() - pose.headR * 0.20f, 0);
    QVector3D helmTop(0, pose.headPos.y() + pose.headR * 1.40f, 0);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, helmBot, helmTop, helmR), steelColor,
             nullptr, 1.0f);

    QVector3D capTop(0, pose.headPos.y() + pose.headR * 1.48f, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, helmTop, capTop, helmR * 0.98f),
             steelColor * 1.05f, nullptr, 1.0f);

    ring(QVector3D(0, pose.headPos.y() + pose.headR * 1.25f, 0), helmR * 1.02f,
         0.015f, steelColor * 1.08f);
    ring(QVector3D(0, pose.headPos.y() + pose.headR * 0.50f, 0), helmR * 1.02f,
         0.015f, steelColor * 1.08f);
    ring(QVector3D(0, pose.headPos.y() - pose.headR * 0.05f, 0), helmR * 1.02f,
         0.015f, steelColor * 1.08f);

    float visorY = pose.headPos.y() + pose.headR * 0.15f;
    float visorZ = helmR * 0.72f;

    QVector3D visorHL(-helmR * 0.35f, visorY, visorZ);
    QVector3D visorHR(helmR * 0.35f, visorY, visorZ);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, visorHL, visorHR, 0.012f),
             QVector3D(0.1f, 0.1f, 0.1f), nullptr, 1.0f);

    QVector3D visorVT(0, visorY + helmR * 0.25f, visorZ);
    QVector3D visorVB(0, visorY - helmR * 0.25f, visorZ);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, visorVB, visorVT, 0.012f),
             QVector3D(0.1f, 0.1f, 0.1f), nullptr, 1.0f);

    auto drawBreathingHole = [&](float x, float y) {
      QVector3D pos(x, pose.headPos.y() + y, helmR * 0.70f);
      QMatrix4x4 m = ctx.model;
      m.translate(pos);
      m.scale(0.010f);
      out.mesh(getUnitSphere(), m, QVector3D(0.1f, 0.1f, 0.1f), nullptr, 1.0f);
    };

    for (int i = 0; i < 4; ++i) {
      drawBreathingHole(helmR * 0.50f, pose.headR * (0.05f - i * 0.10f));
    }

    for (int i = 0; i < 4; ++i) {
      drawBreathingHole(-helmR * 0.50f, pose.headR * (0.05f - i * 0.10f));
    }

    QVector3D plumeBase(0, pose.headPos.y() + pose.headR * 1.50f, 0);
    QVector3D brassColor = v.palette.metal * QVector3D(1.3f, 1.1f, 0.7f);

    QMatrix4x4 plume = ctx.model;
    plume.translate(plumeBase);
    plume.scale(0.030f, 0.015f, 0.030f);
    out.mesh(getUnitSphere(), plume, brassColor * 1.2f, nullptr, 1.0f);

    for (int i = 0; i < 5; ++i) {
      float offset = i * 0.025f;
      QVector3D featherStart =
          plumeBase + QVector3D(0, 0.005f, -0.020f + offset * 0.5f);
      QVector3D featherEnd = featherStart + QVector3D(0, 0.15f - i * 0.015f,
                                                      -0.08f + offset * 0.3f);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, featherStart, featherEnd, 0.008f),
               v.palette.cloth * (1.1f - i * 0.05f), nullptr, 1.0f);
    }
  }

  void drawArmorOverlay(const DrawContext &ctx, const HumanoidVariant &v,
                        const HumanoidPose &pose, float yTopCover, float torsoR,
                        float shoulderHalfSpan, float upperArmR,
                        const QVector3D &rightAxis,
                        ISubmitter &out) const override {
    using HP = HumanProportions;

    auto ring = [&](const QVector3D &center, float r, float h,
                    const QVector3D &col) {
      QVector3D a = center + QVector3D(0, h * 0.5f, 0);
      QVector3D b = center - QVector3D(0, h * 0.5f, 0);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r), col,
               nullptr, 1.0f);
    };

    QVector3D steelColor = v.palette.metal * QVector3D(0.95f, 0.96f, 1.0f);
    QVector3D darkSteel = steelColor * 0.85f;
    QVector3D brassColor = v.palette.metal * QVector3D(1.3f, 1.1f, 0.7f);

    QVector3D bpTop(0, yTopCover + 0.02f, 0);
    QVector3D bpMid(0, (yTopCover + HP::WAIST_Y) * 0.5f + 0.04f, 0);
    QVector3D bpBot(0, HP::WAIST_Y + 0.06f, 0);
    float rChest = torsoR * 1.18f;
    float rWaist = torsoR * 1.14f;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, bpTop, bpMid, rChest), steelColor,
             nullptr, 1.0f);

    QVector3D bpMidLow(0, (bpMid.y() + bpBot.y()) * 0.5f, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, bpMid, bpMidLow, rChest * 0.98f),
             steelColor * 0.99f, nullptr, 1.0f);

    out.mesh(getUnitCone(), coneFromTo(ctx.model, bpBot, bpMidLow, rWaist),
             steelColor * 0.98f, nullptr, 1.0f);

    auto drawRivet = [&](const QVector3D &pos) {
      QMatrix4x4 m = ctx.model;
      m.translate(pos);
      m.scale(0.012f);
      out.mesh(getUnitSphere(), m, brassColor, nullptr, 1.0f);
    };

    for (int i = 0; i < 8; ++i) {
      float angle = (i / 8.0f) * 2.0f * 3.14159265f;
      float x = rChest * std::sin(angle) * 0.95f;
      float z = rChest * std::cos(angle) * 0.95f;
      drawRivet(QVector3D(x, bpMid.y() + 0.08f, z));
    }

    auto drawPauldron = [&](const QVector3D &shoulder,
                            const QVector3D &outward) {
      for (int i = 0; i < 4; ++i) {
        float segY = shoulder.y() + 0.04f - i * 0.045f;
        float segR = upperArmR * (2.5f - i * 0.12f);
        QVector3D segPos = shoulder + outward * (0.02f + i * 0.008f);
        segPos.setY(segY);

        out.mesh(getUnitSphere(), sphereAt(ctx.model, segPos, segR),
                 i == 0 ? steelColor * 1.05f : steelColor * (1.0f - i * 0.03f),
                 nullptr, 1.0f);

        if (i < 3) {
          drawRivet(segPos + QVector3D(0, 0.015f, 0.03f));
        }
      }
    };

    drawPauldron(pose.shoulderL, -rightAxis);
    drawPauldron(pose.shoulderR, rightAxis);

    auto drawArmPlate = [&](const QVector3D &shoulder, const QVector3D &elbow) {
      QVector3D dir = (elbow - shoulder);
      float len = dir.length();
      if (len < 1e-5f)
        return;
      dir /= len;

      for (int i = 0; i < 3; ++i) {
        float t0 = 0.10f + i * 0.25f;
        float t1 = t0 + 0.22f;
        QVector3D a = shoulder + dir * (t0 * len);
        QVector3D b = shoulder + dir * (t1 * len);
        float r = upperArmR * (1.32f - i * 0.04f);

        out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r),
                 steelColor * (0.98f - i * 0.02f), nullptr, 1.0f);

        if (i < 2) {
          drawRivet(b);
        }
      }
    };

    drawArmPlate(pose.shoulderL, pose.elbowL);
    drawArmPlate(pose.shoulderR, pose.elbowR);

    QVector3D gorgetTop(0, yTopCover + 0.025f, 0);
    QVector3D gorgetBot(0, yTopCover - 0.012f, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, gorgetBot, gorgetTop,
                             HP::NECK_RADIUS * 2.6f),
             steelColor * 1.08f, nullptr, 1.0f);

    ring(gorgetTop, HP::NECK_RADIUS * 2.62f, 0.010f, brassColor);
  }

  void drawShoulderDecorations(const DrawContext &ctx, const HumanoidVariant &v,
                               const HumanoidPose &pose, float yTopCover,
                               float yNeck, const QVector3D &rightAxis,
                               ISubmitter &out) const override {
    using HP = HumanProportions;

    QVector3D brassColor = v.palette.metal * QVector3D(1.3f, 1.1f, 0.7f);
    QVector3D chainmailColor = v.palette.metal * QVector3D(0.85f, 0.88f, 0.92f);
    QVector3D mantlingColor = v.palette.cloth;

    for (int i = 0; i < 5; ++i) {
      float y = yNeck - i * 0.022f;
      float r = HP::NECK_RADIUS * (1.85f + i * 0.08f);
      QVector3D ringPos(0, y, 0);
      QVector3D a = ringPos + QVector3D(0, 0.010f, 0);
      QVector3D b = ringPos - QVector3D(0, 0.010f, 0);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r),
               chainmailColor * (1.0f - i * 0.04f), nullptr, 1.0f);
    }

    auto drawStud = [&](const QVector3D &pos) {
      QMatrix4x4 m = ctx.model;
      m.translate(pos);
      m.scale(0.008f);
      out.mesh(getUnitSphere(), m, brassColor * 1.3f, nullptr, 1.0f);
    };

    QVector3D beltCenter(0, HP::WAIST_Y + 0.03f, HP::TORSO_BOT_R * 1.15f);
    QMatrix4x4 buckle = ctx.model;
    buckle.translate(beltCenter);
    buckle.scale(0.035f, 0.025f, 0.012f);
    out.mesh(getUnitSphere(), buckle, brassColor * 1.25f, nullptr, 1.0f);

    QVector3D buckleH1 = beltCenter + QVector3D(-0.025f, 0, 0.005f);
    QVector3D buckleH2 = beltCenter + QVector3D(0.025f, 0, 0.005f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, buckleH1, buckleH2, 0.006f),
             brassColor * 1.4f, nullptr, 1.0f);

    QVector3D buckleV1 = beltCenter + QVector3D(0, -0.018f, 0.005f);
    QVector3D buckleV2 = beltCenter + QVector3D(0, 0.018f, 0.005f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, buckleV1, buckleV2, 0.006f),
             brassColor * 1.4f, nullptr, 1.0f);
  }

private:
  static MountedKnightExtras
  computeMountedKnightExtras(uint32_t seed, const HumanoidVariant &v) {
    MountedKnightExtras e;

    e.metalColor = QVector3D(0.72f, 0.73f, 0.78f);

    float horseHue = hash01(seed ^ 0x23456u);
    if (horseHue < 0.30f) {
      e.horseColor = QVector3D(0.15f, 0.12f, 0.10f);
    } else if (horseHue < 0.60f) {
      e.horseColor = QVector3D(0.35f, 0.28f, 0.22f);
    } else if (horseHue < 0.85f) {
      e.horseColor = QVector3D(0.25f, 0.20f, 0.18f);
    } else {
      e.horseColor = QVector3D(0.45f, 0.40f, 0.35f);
    }

    e.saddleColor = v.palette.leather * 0.85f;

    e.lanceLength = 1.15f + (hash01(seed ^ 0xABCDu) - 0.5f) * 0.20f;
    e.lanceWidth = 0.032f + (hash01(seed ^ 0x7777u) - 0.5f) * 0.008f;

    e.hasLance = (hash01(seed ^ 0xFACEu) > 0.25f);
    e.hasCavalryShield = (hash01(seed ^ 0xCAFEu) > 0.60f);

    return e;
  }

  static void drawHorse(const DrawContext &ctx, const HumanoidPose &pose,
                        const HumanoidVariant &v,
                        const MountedKnightExtras &extras,
                        const AnimationInputs &anim, ISubmitter &out) {
    using HP = HumanProportions;

    QVector3D horseBodyBase(0, -0.30f, 0);

    float bodyLength = 0.55f;
    float bodyWidth = 0.22f;
    float bodyHeight = 0.24f;

    QVector3D bodyFront = horseBodyBase + QVector3D(0, 0, bodyLength * 0.5f);
    QVector3D bodyBack = horseBodyBase - QVector3D(0, 0, bodyLength * 0.5f);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, bodyBack, bodyFront, bodyWidth),
             extras.horseColor, nullptr, 1.0f);

    QVector3D chestPos = bodyFront + QVector3D(0, 0.05f, 0.05f);
    QMatrix4x4 chest = ctx.model;
    chest.translate(chestPos);
    chest.scale(bodyWidth * 1.1f, bodyHeight * 0.9f, bodyWidth * 0.8f);
    out.mesh(getUnitSphere(), chest, extras.horseColor * 1.05f, nullptr, 1.0f);

    QVector3D neckBase = bodyFront + QVector3D(0, 0.08f, 0.12f);
    QVector3D neckTop = neckBase + QVector3D(0, 0.28f, 0.10f);
    float neckRadius = 0.08f;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, neckBase, neckTop, neckRadius),
             extras.horseColor, nullptr, 1.0f);

    QVector3D headPos = neckTop + QVector3D(0, 0.10f, 0.12f);
    QMatrix4x4 head = ctx.model;
    head.translate(headPos);
    head.scale(0.10f, 0.12f, 0.16f);
    out.mesh(getUnitSphere(), head, extras.horseColor * 1.05f, nullptr, 1.0f);

    QVector3D muzzlePos = headPos + QVector3D(0, -0.05f, 0.15f);
    QMatrix4x4 muzzle = ctx.model;
    muzzle.translate(muzzlePos);
    muzzle.scale(0.06f, 0.07f, 0.09f);
    out.mesh(getUnitSphere(), muzzle, extras.horseColor * 0.95f, nullptr, 1.0f);

    QVector3D earL = headPos + QVector3D(0.06f, 0.12f, 0.02f);
    QVector3D earLtip = earL + QVector3D(0.02f, 0.06f, -0.01f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, earL, earLtip, 0.015f),
             extras.horseColor * 0.90f, nullptr, 1.0f);

    QVector3D earR = headPos + QVector3D(-0.06f, 0.12f, 0.02f);
    QVector3D earRtip = earR + QVector3D(-0.02f, 0.06f, -0.01f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, earR, earRtip, 0.015f),
             extras.horseColor * 0.90f, nullptr, 1.0f);

    float legLength = 0.50f;
    float legRadius = 0.045f;

    auto drawLeg = [&](const QVector3D &start, float xOffset, float zOffset) {
      QVector3D legTop = start + QVector3D(xOffset, -0.05f, zOffset);
      QVector3D legBot = legTop + QVector3D(0, -legLength, 0);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, legTop, legBot, legRadius),
               extras.horseColor, nullptr, 1.0f);

      QVector3D hoofTop = legBot;
      QVector3D hoofBot = hoofTop + QVector3D(0, -0.06f, 0);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, hoofTop, hoofBot, legRadius * 1.1f),
               QVector3D(0.15f, 0.12f, 0.10f), nullptr, 1.0f);
    };

    drawLeg(bodyFront, 0.12f, 0.15f);
    drawLeg(bodyFront, -0.12f, 0.15f);
    drawLeg(bodyBack, 0.12f, -0.10f);
    drawLeg(bodyBack, -0.12f, -0.10f);

    QVector3D tailBase = bodyBack + QVector3D(0, 0.10f, -0.15f);
    for (int i = 0; i < 4; ++i) {
      float segLen = 0.08f - i * 0.015f;
      float segR = 0.025f - i * 0.004f;
      QVector3D segStart = tailBase + QVector3D(0, -i * 0.06f, -i * 0.10f);
      QVector3D segEnd = segStart + QVector3D(0, -0.05f, -segLen);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, segStart, segEnd, segR),
               extras.horseColor * (0.85f - i * 0.05f), nullptr, 1.0f);
    }

    QVector3D saddlePos = horseBodyBase + QVector3D(0, bodyHeight * 0.5f, 0);
    QMatrix4x4 saddle = ctx.model;
    saddle.translate(saddlePos);
    saddle.scale(bodyWidth * 1.15f, 0.08f, bodyLength * 0.45f);
    out.mesh(getUnitSphere(), saddle, extras.saddleColor, nullptr, 1.0f);

    QVector3D blanketPos = saddlePos + QVector3D(0, -0.05f, 0);
    QMatrix4x4 blanket = ctx.model;
    blanket.translate(blanketPos);
    blanket.scale(bodyWidth * 1.20f, 0.02f, bodyLength * 0.55f);
    out.mesh(getUnitSphere(), blanket, v.palette.cloth, nullptr, 1.0f);

    for (int i = 0; i < 3; ++i) {
      float angle = (i / 3.0f) * 3.14159265f;
      float x = std::sin(angle) * bodyWidth * 0.9f;
      float z = (i - 1) * 0.12f;

      QVector3D strapA = saddlePos + QVector3D(x, 0.03f, z);
      QVector3D strapB = strapA + QVector3D(0, -bodyHeight * 0.8f, 0);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, strapA, strapB, 0.012f),
               extras.saddleColor * 0.90f, nullptr, 1.0f);
    }

    QVector3D maneStart = neckTop + QVector3D(0, 0.08f, -0.05f);
    for (int i = 0; i < 6; ++i) {
      float t = i / 6.0f;
      QVector3D segPos = neckTop * (1.0f - t) + neckBase * t;
      segPos.setY(segPos.y() + 0.08f - i * 0.01f);

      QVector3D segEnd = segPos + QVector3D(0, 0.05f - i * 0.005f, -0.02f);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, segPos, segEnd, 0.010f),
               extras.horseColor * (0.80f - i * 0.03f), nullptr, 1.0f);
    }
  }

  static void drawLance(const DrawContext &ctx, const HumanoidPose &pose,
                        const HumanoidVariant &v,
                        const MountedKnightExtras &extras, bool isAttacking,
                        float attackPhase, ISubmitter &out) {
    QVector3D gripPos = pose.handR;

    constexpr float kLanceYawDeg = 15.0f;
    QMatrix4x4 yawM;
    yawM.rotate(kLanceYawDeg, 0.0f, 1.0f, 0.0f);

    QVector3D upish = yawM.map(QVector3D(0.10f, 0.95f, 0.30f));
    QVector3D midish = yawM.map(QVector3D(0.15f, 0.50f, 0.85f));
    QVector3D downish = yawM.map(QVector3D(0.20f, -0.20f, 1.00f));
    if (upish.lengthSquared() > 1e-6f)
      upish.normalize();
    if (midish.lengthSquared() > 1e-6f)
      midish.normalize();
    if (downish.lengthSquared() > 1e-6f)
      downish.normalize();

    QVector3D lanceDir = upish;

    if (isAttacking) {
      if (attackPhase < 0.20f) {
        float t = easeInOutCubic(attackPhase / 0.20f);
        lanceDir = nlerp(upish, upish, t);
      } else if (attackPhase < 0.35f) {
        float t = easeInOutCubic((attackPhase - 0.20f) / 0.15f);
        lanceDir = nlerp(upish, midish, t * 0.40f);
      } else if (attackPhase < 0.55f) {
        float t = (attackPhase - 0.35f) / 0.20f;
        t = t * t * t;
        if (t < 0.5f) {
          float u = t / 0.5f;
          lanceDir = nlerp(upish, midish, u);
        } else {
          float u = (t - 0.5f) / 0.5f;
          lanceDir = nlerp(midish, downish, u);
        }
      } else if (attackPhase < 0.75f) {
        float t = easeInOutCubic((attackPhase - 0.55f) / 0.20f);
        lanceDir = nlerp(downish, midish, t);
      } else {
        float t = smoothstep(0.75f, 1.0f, attackPhase);
        lanceDir = nlerp(midish, upish, t);
      }
    }

    QVector3D lanceBase = gripPos - lanceDir * 0.12f;
    QVector3D lanceTip = gripPos + lanceDir * extras.lanceLength;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, lanceBase, gripPos,
                             extras.lanceWidth * 1.2f),
             v.palette.leather, nullptr, 1.0f);

    float shaftSeg = extras.lanceLength * 0.85f;
    QVector3D shaftEnd = gripPos + lanceDir * shaftSeg;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, gripPos, shaftEnd, extras.lanceWidth),
             QVector3D(0.45f, 0.35f, 0.28f), nullptr, 1.0f);

    out.mesh(
        getUnitCone(),
        coneFromTo(ctx.model, lanceTip, shaftEnd, extras.lanceWidth * 0.9f),
        extras.metalColor, nullptr, 1.0f);

    QVector3D bannerPos = gripPos + lanceDir * (extras.lanceLength * 0.35f);
    for (int i = 0; i < 3; ++i) {
      QVector3D segStart = bannerPos + QVector3D(0, 0, 0.02f * i);
      QVector3D segEnd = segStart + QVector3D(0.08f - i * 0.015f, 0, 0);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, segStart, segEnd, 0.005f),
               v.palette.cloth * (1.1f - i * 0.08f), nullptr, 1.0f);
    }

    if (isAttacking && attackPhase >= 0.35f && attackPhase < 0.60f) {
      float t = (attackPhase - 0.35f) / 0.25f;
      float alpha = clamp01(0.30f * (1.0f - t));
      QVector3D trailStart = lanceTip;
      QVector3D trailEnd = lanceTip - lanceDir * (0.25f + 0.15f * t);
      out.mesh(
          getUnitCone(),
          coneFromTo(ctx.model, trailEnd, trailStart, extras.lanceWidth * 0.8f),
          extras.metalColor * 0.95f, nullptr, alpha);
    }
  }

  static void drawCavalryShield(const DrawContext &ctx,
                                const HumanoidPose &pose,
                                const HumanoidVariant &v,
                                const MountedKnightExtras &extras,
                                ISubmitter &out) {
    const float scaleFactor = 2.0f;
    const float R = 0.15f * scaleFactor;

    const float yawDeg = -70.0f;
    QMatrix4x4 rot;
    rot.rotate(yawDeg, 0.0f, 1.0f, 0.0f);

    const QVector3D n = rot.map(QVector3D(0.0f, 0.0f, 1.0f));
    const QVector3D axisX = rot.map(QVector3D(1.0f, 0.0f, 0.0f));
    const QVector3D axisY = rot.map(QVector3D(0.0f, 1.0f, 0.0f));

    QVector3D shieldCenter =
        pose.handL + axisX * (-R * 0.30f) + axisY * (-0.05f) + n * (0.05f);

    const float plateHalf = 0.0012f;
    const float plateFull = plateHalf * 2.0f;

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shieldCenter + n * plateHalf);
      m.rotate(yawDeg, 0.0f, 1.0f, 0.0f);
      m.scale(R, R, plateFull);
      out.mesh(getUnitCylinder(), m, v.palette.cloth * 1.15f, nullptr, 1.0f);
    }

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shieldCenter - n * plateHalf);
      m.rotate(yawDeg, 0.0f, 1.0f, 0.0f);
      m.scale(R * 0.985f, R * 0.985f, plateFull);
      out.mesh(getUnitCylinder(), m, v.palette.leather * 0.8f, nullptr, 1.0f);
    }

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shieldCenter + n * (0.015f * scaleFactor));
      m.scale(0.035f * scaleFactor);
      out.mesh(getUnitSphere(), m, extras.metalColor, nullptr, 1.0f);
    }

    {
      QVector3D gripA = shieldCenter - axisX * 0.025f - n * 0.025f;
      QVector3D gripB = shieldCenter + axisX * 0.025f - n * 0.025f;
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, gripA, gripB, 0.008f),
               v.palette.leather, nullptr, 1.0f);
    }
  }
};

void registerMountedKnightRenderer(
    Render::GL::EntityRendererRegistry &registry) {
  static MountedKnightRenderer renderer;
  registry.registerRenderer(
      "mounted_knight", [](const DrawContext &ctx, ISubmitter &out) {
        static MountedKnightRenderer staticRenderer;
        Shader *mountedKnightShader = nullptr;
        if (ctx.backend) {
          mountedKnightShader =
              ctx.backend->shader(QStringLiteral("mounted_knight"));
        }
        Renderer *sceneRenderer = dynamic_cast<Renderer *>(&out);
        if (sceneRenderer && mountedKnightShader) {
          sceneRenderer->setCurrentShader(mountedKnightShader);
        }
        staticRenderer.render(ctx, out);
        if (sceneRenderer) {
          sceneRenderer->setCurrentShader(nullptr);
        }
      });
}

} // namespace Render::GL
