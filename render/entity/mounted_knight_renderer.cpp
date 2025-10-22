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
#include "horse_renderer.h"
#include "registry.h"
#include "renderer_constants.h"
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
using Render::Geom::easeInOutCubic;
using Render::Geom::lerp;
using Render::Geom::nlerp;
using Render::Geom::smoothstep;
using Render::Geom::sphereAt;

struct MountedKnightExtras {
  QVector3D metalColor;
  HorseProfile horseProfile;
  float swordLength = 0.85f;
  float swordWidth = 0.045f;
  bool hasSword = true;
  bool hasCavalryShield = false;
};

class MountedKnightRenderer : public HumanoidRendererBase {
public:
  QVector3D getProportionScaling() const override {
    return QVector3D(1.40f, 1.05f, 1.10f);
  }

private:
  mutable std::unordered_map<uint32_t, MountedKnightExtras> m_extrasCache;
  HorseRenderer m_horseRenderer;

public:
  void getVariant(const DrawContext &ctx, uint32_t seed,
                  HumanoidVariant &v) const override {
    QVector3D teamTint = resolveTeamTint(ctx);
    v.palette = makeHumanoidPalette(teamTint, seed);
  }

  void customizePose(const DrawContext &ctx, const AnimationInputs &anim,
                     uint32_t seed, HumanoidPose &pose) const override {
    using HP = HumanProportions;

    const float armHeightJitter = (hash01(seed ^ 0xABCDu) - 0.5f) * 0.03f;
    const float armAsymmetry = (hash01(seed ^ 0xDEF0u) - 0.5f) * 0.04f;

    uint32_t horseSeed = seed;
    if (ctx.entity) {
      horseSeed = static_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFu);
    }

    const HorseDimensions dims = makeHorseDimensions(horseSeed);

    const float saddleHeight = dims.saddleHeight;
    const float offsetY = saddleHeight - pose.pelvisPos.y();

    pose.pelvisPos.setY(pose.pelvisPos.y() + offsetY);
    pose.headPos.setY(pose.headPos.y() + offsetY);
    pose.neckBase.setY(pose.neckBase.y() + offsetY);
    pose.shoulderL.setY(pose.shoulderL.y() + offsetY);
    pose.shoulderR.setY(pose.shoulderR.y() + offsetY);

    const float leanForward = dims.seatForwardOffset * 0.08f;
    pose.shoulderL.setZ(pose.shoulderL.z() + leanForward);
    pose.shoulderR.setZ(pose.shoulderR.z() + leanForward);

    const float stirrupForward = dims.seatForwardOffset - 0.035f;
    const float stirrupHeight = saddleHeight - dims.stirrupDrop;

    pose.footYOffset = 0.0f;
    pose.footL = QVector3D(-dims.stirrupOut, stirrupHeight, stirrupForward);
    pose.footR = QVector3D(dims.stirrupOut, stirrupHeight, stirrupForward);

    const float kneeY = stirrupHeight + (saddleHeight - stirrupHeight) * 0.62f;
    const float kneeZ = stirrupForward * 0.60f + 0.06f;

    pose.kneeL = QVector3D(-dims.stirrupOut * 0.92f, kneeY, kneeZ);
    pose.kneeR = QVector3D(dims.stirrupOut * 0.92f, kneeY, kneeZ);

    const float reinForward = dims.seatForwardOffset + 0.22f;
    const float shoulderHeight = pose.shoulderL.y();
    const float reinSpread = HP::SHOULDER_WIDTH * 0.36f;

    QVector3D restHandR(reinSpread, shoulderHeight - 0.05f + armHeightJitter,
                        reinForward);
    QVector3D restHandL(-reinSpread * 0.85f,
                        shoulderHeight - 0.08f - armHeightJitter * 0.4f,
                        reinForward - 0.05f);

    restHandR.setX(restHandR.x() + armAsymmetry * 0.45f);
    restHandL.setX(restHandL.x() - armAsymmetry * 0.55f);

    pose.elbowL = QVector3D(pose.shoulderL.x() * 0.4f + restHandL.x() * 0.6f,
                            (pose.shoulderL.y() + restHandL.y()) * 0.5f - 0.08f,
                            (pose.shoulderL.z() + restHandL.z()) * 0.5f);
    pose.elbowR = QVector3D(pose.shoulderR.x() * 0.4f + restHandR.x() * 0.6f,
                            (pose.shoulderR.y() + restHandR.y()) * 0.5f - 0.08f,
                            (pose.shoulderR.z() + restHandR.z()) * 0.5f);

    if (anim.isAttacking && anim.isMelee) {
      const float attackCycleTime = 0.70f;
      float attackPhase = std::fmod(anim.time / attackCycleTime, 1.0f);

      QVector3D restPos = restHandR;
      QVector3D windupPos = QVector3D(
          restHandR.x() + 0.32f, shoulderHeight + 0.15f, reinForward - 0.35f);
      QVector3D raisedPos = QVector3D(
          reinSpread + 0.38f, shoulderHeight + 0.28f, reinForward - 0.25f);
      QVector3D slashPos = QVector3D(
          -reinSpread * 0.65f, shoulderHeight - 0.08f, reinForward + 0.85f);
      QVector3D followThrough = QVector3D(
          -reinSpread * 0.85f, shoulderHeight - 0.15f, reinForward + 0.60f);
      QVector3D recoverPos = QVector3D(
          reinSpread * 0.45f, shoulderHeight - 0.05f, reinForward + 0.25f);

      if (attackPhase < 0.18f) {

        float t = easeInOutCubic(attackPhase / 0.18f);
        pose.handR = restPos * (1.0f - t) + windupPos * t;
      } else if (attackPhase < 0.30f) {

        float t = easeInOutCubic((attackPhase - 0.18f) / 0.12f);
        pose.handR = windupPos * (1.0f - t) + raisedPos * t;
      } else if (attackPhase < 0.48f) {

        float t = (attackPhase - 0.30f) / 0.18f;
        t = t * t * t;
        pose.handR = raisedPos * (1.0f - t) + slashPos * t;
      } else if (attackPhase < 0.62f) {

        float t = easeInOutCubic((attackPhase - 0.48f) / 0.14f);
        pose.handR = slashPos * (1.0f - t) + followThrough * t;
      } else if (attackPhase < 0.80f) {

        float t = easeInOutCubic((attackPhase - 0.62f) / 0.18f);
        pose.handR = followThrough * (1.0f - t) + recoverPos * t;
      } else {

        float t = smoothstep(0.80f, 1.0f, attackPhase);
        pose.handR = recoverPos * (1.0f - t) + restPos * t;
      }

      float reinTension = clamp01((attackPhase - 0.10f) * 2.2f);
      pose.handL = restHandL +
                   QVector3D(0.0f, -0.015f * reinTension, 0.10f * reinTension);

      pose.elbowR =
          QVector3D(pose.shoulderR.x() * 0.3f + pose.handR.x() * 0.7f,
                    (pose.shoulderR.y() + pose.handR.y()) * 0.5f - 0.12f,
                    (pose.shoulderR.z() + pose.handR.z()) * 0.5f);
      pose.elbowL =
          QVector3D(pose.shoulderL.x() * 0.4f + pose.handL.x() * 0.6f,
                    (pose.shoulderL.y() + pose.handL.y()) * 0.5f - 0.08f,
                    (pose.shoulderL.z() + pose.handL.z()) * 0.5f);
    } else {
      pose.handR = restHandR;
      pose.handL = restHandL;
    }
  }

  void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose, const AnimationInputs &anim,
                      ISubmitter &out) const override {
    uint32_t horseSeed = 0u;
    if (ctx.entity) {
      horseSeed = static_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFu);
    }

    MountedKnightExtras extras;
    auto it = m_extrasCache.find(horseSeed);
    if (it != m_extrasCache.end()) {
      extras = it->second;
    } else {
      extras = computeMountedKnightExtras(horseSeed, v);
      m_extrasCache[horseSeed] = extras;

      if (m_extrasCache.size() > MAX_EXTRAS_CACHE_SIZE) {
        m_extrasCache.clear();
      }
    }

    m_horseRenderer.render(ctx, anim, extras.horseProfile, out);

    bool isAttacking = anim.isAttacking && anim.isMelee;
    float attackPhase = 0.0f;
    if (isAttacking) {
      float attackCycleTime = 0.7f;
      attackPhase = std::fmod(anim.time * (1.0f / attackCycleTime), 1.0f);
    }

    if (extras.hasSword) {
      drawSword(ctx, pose, v, extras, isAttacking, attackPhase, out);
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
    QVector3D bpMid(0, (yTopCover + pose.pelvisPos.y()) * 0.5f + 0.04f, 0);
    QVector3D bpBot(0, pose.pelvisPos.y() + 0.06f, 0);
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

    e.horseProfile = makeHorseProfile(seed, v.palette.leather, v.palette.cloth);

    e.swordLength = 0.82f + (hash01(seed ^ 0xABCDu) - 0.5f) * 0.12f;
    e.swordWidth = 0.042f + (hash01(seed ^ 0x7777u) - 0.5f) * 0.008f;

    e.hasSword = (hash01(seed ^ 0xFACEu) > 0.15f);
    e.hasCavalryShield = (hash01(seed ^ 0xCAFEu) > 0.60f);

    return e;
  }

  static void drawSword(const DrawContext &ctx, const HumanoidPose &pose,
                        const HumanoidVariant &v,
                        const MountedKnightExtras &extras, bool isAttacking,
                        float attackPhase, ISubmitter &out) {

    const QVector3D gripPos = pose.handR;

    QVector3D swordDir(0.0f, 0.15f, 1.0f);
    swordDir.normalize();

    QVector3D worldUp(0.0f, 1.0f, 0.0f);
    QVector3D rightAxis = QVector3D::crossProduct(worldUp, swordDir);
    if (rightAxis.lengthSquared() < 1e-6f)
      rightAxis = QVector3D(1.0f, 0.0f, 0.0f);
    rightAxis.normalize();
    QVector3D upAxis = QVector3D::crossProduct(swordDir, rightAxis);
    upAxis.normalize();

    const QVector3D steel = extras.metalColor;
    const QVector3D steelHi = steel * 1.18f;
    const QVector3D steelLo = steel * 0.92f;
    const QVector3D leather = v.palette.leather;
    const QVector3D pommelCol =
        v.palette.metal * QVector3D(1.25f, 1.10f, 0.75f);

    const float pommelOffset = 0.10f;
    const float gripLen = 0.16f;
    const float gripRad = 0.017f;
    const float guardHalf = 0.11f;
    const float guardRad = 0.012f;
    const float guardCurve = 0.03f;

    const QVector3D pommelPos = gripPos - swordDir * pommelOffset;
    out.mesh(getUnitSphere(), sphereAt(ctx.model, pommelPos, 0.028f), pommelCol,
             nullptr, 1.0f);

    {
      QVector3D neckA = pommelPos + swordDir * 0.010f;
      QVector3D neckB = gripPos - swordDir * 0.005f;
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, neckA, neckB, 0.0125f), steelLo,
               nullptr, 1.0f);

      QVector3D peen = pommelPos - swordDir * 0.012f;
      out.mesh(getUnitCone(), coneFromTo(ctx.model, peen, pommelPos, 0.010f),
               steel, nullptr, 1.0f);
    }

    const QVector3D gripA = gripPos - swordDir * 0.005f;
    const QVector3D gripB = gripPos + swordDir * (gripLen - 0.005f);
    const int wrapRings = 5;
    for (int i = 0; i < wrapRings; ++i) {
      float t0 = (float)i / wrapRings;
      float t1 = (float)(i + 1) / wrapRings;
      QVector3D a = gripA + swordDir * (gripLen * t0);
      QVector3D b = gripA + swordDir * (gripLen * t1);

      float rMid = gripRad * (0.96f + 0.08f * std::sin((t0 + t1) * 3.14159f));
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, rMid),
               leather * 0.98f, nullptr, 1.0f);
    }

    const QVector3D guardCenter = gripB + swordDir * 0.010f;
    {
      const int segs = 4;
      QVector3D prev =
          guardCenter - rightAxis * guardHalf + (-upAxis * guardCurve);
      for (int s = 1; s <= segs; ++s) {
        float u = -1.0f + 2.0f * (float)s / segs;
        QVector3D p = guardCenter + rightAxis * (guardHalf * u) +
                      (-upAxis * guardCurve * (1.0f - u * u));
        out.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, prev, p, guardRad), steelHi,
                 nullptr, 1.0f);
        prev = p;
      }

      QVector3D Lend =
          guardCenter - rightAxis * guardHalf + (-upAxis * guardCurve);
      QVector3D Rend =
          guardCenter + rightAxis * guardHalf + (-upAxis * guardCurve);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, Lend - rightAxis * 0.030f, Lend,
                          guardRad * 1.12f),
               steelHi, nullptr, 1.0f);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, Rend + rightAxis * 0.030f, Rend,
                          guardRad * 1.12f),
               steelHi, nullptr, 1.0f);

      out.mesh(getUnitSphere(),
               sphereAt(ctx.model, guardCenter, guardRad * 0.9f), steel,
               nullptr, 1.0f);
    }

    const float bladeLen = std::max(0.0f, extras.swordLength - 0.14f);
    const QVector3D bladeRoot = guardCenter + swordDir * 0.020f;
    const QVector3D bladeTip = bladeRoot + swordDir * bladeLen;

    const QVector3D ricassoEnd = bladeRoot + swordDir * (bladeLen * 0.08f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, bladeRoot, ricassoEnd,
                             extras.swordWidth * 0.32f),
             steelHi, nullptr, 1.0f);

    const QVector3D fullerA = bladeRoot + swordDir * (bladeLen * 0.10f);
    const QVector3D fullerB = bladeRoot + swordDir * (bladeLen * 0.80f);
    out.mesh(
        getUnitCylinder(),
        cylinderBetween(ctx.model, fullerA, fullerB, extras.swordWidth * 0.10f),
        steelLo, nullptr, 1.0f);

    const float baseR = extras.swordWidth * 0.26f;
    const float midR = extras.swordWidth * 0.16f;
    const float preTipR = extras.swordWidth * 0.09f;

    QVector3D s0 = ricassoEnd;
    QVector3D s1 = bladeRoot + swordDir * (bladeLen * 0.55f);
    QVector3D s2 = bladeRoot + swordDir * (bladeLen * 0.88f);

    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, s0, s1, baseR),
             steelHi, nullptr, 1.0f);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, s1, s2, midR),
             steelHi, nullptr, 1.0f);

    {
      float edgeR = extras.swordWidth * 0.03f;
      QVector3D eA = bladeRoot + swordDir * (bladeLen * 0.10f);
      QVector3D eB = bladeTip - swordDir * (bladeLen * 0.06f);
      QVector3D leftEdgeA = eA + rightAxis * (baseR * 0.95f);
      QVector3D leftEdgeB = eB + rightAxis * (preTipR * 0.95f);
      QVector3D rightEdgeA = eA - rightAxis * (baseR * 0.95f);
      QVector3D rightEdgeB = eB - rightAxis * (preTipR * 0.95f);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, leftEdgeA, leftEdgeB, edgeR),
               steel * 1.08f, nullptr, 1.0f);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, rightEdgeA, rightEdgeB, edgeR),
               steel * 1.08f, nullptr, 1.0f);
    }

    out.mesh(
        getUnitCylinder(),
        cylinderBetween(ctx.model, s2, bladeTip - swordDir * 0.020f, preTipR),
        steelHi, nullptr, 1.0f);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, bladeTip, bladeTip - swordDir * 0.060f,
                        preTipR * 0.95f),
             steelHi * 1.04f, nullptr, 1.0f);

    {
      QVector3D shoulderL0 = bladeRoot + rightAxis * (baseR * 1.05f);
      QVector3D shoulderL1 = shoulderL0 - rightAxis * (baseR * 0.45f);
      QVector3D shoulderR0 = bladeRoot - rightAxis * (baseR * 1.05f);
      QVector3D shoulderR1 = shoulderR0 + rightAxis * (baseR * 0.45f);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, shoulderL1, shoulderL0, baseR * 0.22f),
               steel, nullptr, 1.0f);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, shoulderR1, shoulderR0, baseR * 0.22f),
               steel, nullptr, 1.0f);
    }

    if (isAttacking && attackPhase >= 0.28f && attackPhase < 0.58f) {
      float t = (attackPhase - 0.28f) / 0.30f;
      float alpha = clamp01(0.40f * (1.0f - t * t));
      QVector3D sweep = (-rightAxis * 0.18f - swordDir * 0.10f) * t;

      QVector3D trailTip = bladeTip + sweep;
      QVector3D trailRoot = bladeRoot + sweep * 0.6f;

      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, trailRoot, trailTip, baseR * 1.10f),
               steel * 0.90f, nullptr, alpha);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, trailRoot + upAxis * 0.01f, trailTip,
                          baseR * 0.75f),
               steel * 0.80f, nullptr, alpha * 0.7f);
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
