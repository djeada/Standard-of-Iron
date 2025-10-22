#include "spearman_renderer.h"
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
#include "renderer_constants.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace Render::GL {

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::easeInOutCubic;
using Render::Geom::lerp;
using Render::Geom::smoothstep;
using Render::Geom::sphereAt;

struct SpearmanExtras {
  QVector3D spearShaftColor;
  QVector3D spearheadColor;
  float spearLength = 1.20f;
  float spearShaftRadius = 0.020f;
  float spearheadLength = 0.18f;
};

class SpearmanRenderer : public HumanoidRendererBase {
public:
  QVector3D getProportionScaling() const override {
    return QVector3D(1.10f, 1.02f, 1.05f);
  }

private:
  mutable std::unordered_map<uint32_t, SpearmanExtras> m_extrasCache;

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

    if (anim.isInHoldMode || anim.isExitingHold) {
      float t = anim.isInHoldMode ? 1.0f : (1.0f - anim.holdExitProgress);

      float kneelDepth = 0.35f * t;
      float pelvisY = HP::WAIST_Y - kneelDepth;
      pose.pelvisPos.setY(pelvisY);

      float stanceNarrow = 0.10f;

      float leftKneeY = HP::GROUND_Y + 0.06f * t;
      float leftKneeZ = -0.08f * t;
      pose.kneeL = QVector3D(-stanceNarrow, leftKneeY, leftKneeZ);
      pose.footL = QVector3D(-stanceNarrow - 0.02f, HP::GROUND_Y,
                             leftKneeZ - HP::LOWER_LEG_LEN * 0.90f * t);

      float rightKneeY =
          HP::WAIST_Y * 0.45f * (1.0f - t) + HP::WAIST_Y * 0.30f * t;
      pose.kneeR = QVector3D(stanceNarrow + 0.05f, rightKneeY, 0.15f * t);
      pose.footR = QVector3D(stanceNarrow + 0.08f, HP::GROUND_Y, 0.25f * t);

      float upperBodyDrop = kneelDepth;
      pose.shoulderL.setY(HP::SHOULDER_Y - upperBodyDrop);
      pose.shoulderR.setY(HP::SHOULDER_Y - upperBodyDrop);
      pose.neckBase.setY(HP::NECK_BASE_Y - upperBodyDrop);

      float loweredChinY = HP::CHIN_Y - upperBodyDrop;

      pose.headPos.setY(loweredChinY + pose.headR);

      float forwardLean = 0.08f * t;
      pose.shoulderL.setZ(pose.shoulderL.z() + forwardLean);
      pose.shoulderR.setZ(pose.shoulderR.z() + forwardLean);
      pose.neckBase.setZ(pose.neckBase.z() + forwardLean * 0.8f);
      pose.headPos.setZ(pose.headPos.z() + forwardLean * 0.7f);

      float loweredShoulderY = HP::SHOULDER_Y - upperBodyDrop;

      pose.handR =
          QVector3D(0.18f * (1.0f - t) + 0.22f * t,
                    loweredShoulderY * (1.0f - t) + (pelvisY + 0.05f) * t,
                    0.15f * (1.0f - t) + 0.20f * t);

      pose.handL = QVector3D(
          0.0f, loweredShoulderY * (1.0f - t) + (loweredShoulderY - 0.10f) * t,
          0.30f * (1.0f - t) + 0.55f * t);

      QVector3D shoulderToHandR = pose.handR - pose.shoulderR;
      float armLengthR = shoulderToHandR.length();
      QVector3D armDirR = shoulderToHandR.normalized();
      pose.elbowR = pose.shoulderR + armDirR * (armLengthR * 0.5f) +
                    QVector3D(0.08f, -0.15f, -0.05f);

      QVector3D shoulderToHandL = pose.handL - pose.shoulderL;
      float armLengthL = shoulderToHandL.length();
      QVector3D armDirL = shoulderToHandL.normalized();
      pose.elbowL = pose.shoulderL + armDirL * (armLengthL * 0.5f) +
                    QVector3D(-0.08f, -0.12f, 0.05f);

    } else if (anim.isAttacking && anim.isMelee && !anim.isInHoldMode) {
      const float attackCycleTime = 0.8f;
      float attackPhase = std::fmod(anim.time * (1.0f / attackCycleTime), 1.0f);

      QVector3D guardPos(0.28f, HP::SHOULDER_Y + 0.05f, 0.25f);
      QVector3D preparePos(0.35f, HP::SHOULDER_Y + 0.08f, 0.05f);
      QVector3D thrustPos(0.32f, HP::SHOULDER_Y + 0.10f, 0.90f);
      QVector3D recoverPos(0.28f, HP::SHOULDER_Y + 0.06f, 0.40f);

      if (attackPhase < 0.20f) {

        float t = easeInOutCubic(attackPhase / 0.20f);
        pose.handR = guardPos * (1.0f - t) + preparePos * t;

        pose.handL = QVector3D(-0.10f, HP::SHOULDER_Y - 0.05f,
                               0.20f * (1.0f - t) + 0.08f * t);
      } else if (attackPhase < 0.30f) {

        pose.handR = preparePos;
        pose.handL = QVector3D(-0.10f, HP::SHOULDER_Y - 0.05f, 0.08f);
      } else if (attackPhase < 0.50f) {

        float t = (attackPhase - 0.30f) / 0.20f;
        t = t * t * t;
        pose.handR = preparePos * (1.0f - t) + thrustPos * t;

        pose.handL =
            QVector3D(-0.10f + 0.05f * t, HP::SHOULDER_Y - 0.05f + 0.03f * t,
                      0.08f + 0.45f * t);
      } else if (attackPhase < 0.70f) {

        float t = easeInOutCubic((attackPhase - 0.50f) / 0.20f);
        pose.handR = thrustPos * (1.0f - t) + recoverPos * t;
        pose.handL = QVector3D(-0.05f * (1.0f - t) - 0.10f * t,
                               HP::SHOULDER_Y - 0.02f * (1.0f - t) - 0.06f * t,
                               lerp(0.53f, 0.35f, t));
      } else {

        float t = smoothstep(0.70f, 1.0f, attackPhase);
        pose.handR = recoverPos * (1.0f - t) + guardPos * t;
        pose.handL = QVector3D(-0.10f - 0.02f * (1.0f - t),
                               HP::SHOULDER_Y - 0.06f + 0.01f * t +
                                   armHeightJitter * (1.0f - t),
                               lerp(0.35f, 0.25f, t));
      }
    } else {
      pose.handR = QVector3D(0.28f + armAsymmetry,
                             HP::SHOULDER_Y - 0.02f + armHeightJitter, 0.30f);

      pose.handL =
          QVector3D(-0.08f - 0.5f * armAsymmetry,
                    HP::SHOULDER_Y - 0.08f + 0.5f * armHeightJitter, 0.45f);

      QVector3D shoulderToHand = pose.handR - pose.shoulderR;
      float armLength = shoulderToHand.length();
      QVector3D armDir = shoulderToHand.normalized();

      pose.elbowR = pose.shoulderR + armDir * (armLength * 0.5f) +
                    QVector3D(0.06f, -0.12f, -0.04f);
    }
  }

  void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose, const AnimationInputs &anim,
                      ISubmitter &out) const override {
    uint32_t seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFu;

    SpearmanExtras extras;
    auto it = m_extrasCache.find(seed);
    if (it != m_extrasCache.end()) {
      extras = it->second;
    } else {
      extras = computeSpearmanExtras(seed, v);
      m_extrasCache[seed] = extras;

      if (m_extrasCache.size() > MAX_EXTRAS_CACHE_SIZE) {
        m_extrasCache.clear();
      }
    }

    bool isAttacking = anim.isAttacking && anim.isMelee;
    float attackPhase = 0.0f;
    if (isAttacking) {
      float attackCycleTime = 0.8f;
      attackPhase = std::fmod(anim.time * (1.0f / attackCycleTime), 1.0f);
    }

    drawSpear(ctx, pose, v, extras, anim, isAttacking, attackPhase, out);
  }

  void drawHelmet(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose, ISubmitter &out) const override {
    using HP = HumanProportions;

    QVector3D ironColor = v.palette.metal * QVector3D(0.88f, 0.90f, 0.92f);

    float helmR = pose.headR * 1.12f;

    QVector3D helmBot(pose.headPos.x(), pose.headPos.y() - pose.headR * 0.15f,
                      pose.headPos.z());
    QVector3D helmTop(pose.headPos.x(), pose.headPos.y() + pose.headR * 1.25f,
                      pose.headPos.z());

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, helmBot, helmTop, helmR), ironColor,
             nullptr, 1.0f);

    QVector3D capTop(pose.headPos.x(), pose.headPos.y() + pose.headR * 1.32f,
                     pose.headPos.z());
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, helmTop, capTop, helmR * 0.96f),
             ironColor * 1.04f, nullptr, 1.0f);

    auto ring = [&](const QVector3D &center, float r, float h,
                    const QVector3D &col) {
      QVector3D a = center + QVector3D(0, h * 0.5f, 0);
      QVector3D b = center - QVector3D(0, h * 0.5f, 0);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r), col,
               nullptr, 1.0f);
    };

    ring(QVector3D(pose.headPos.x(), pose.headPos.y() + pose.headR * 0.95f,
                   pose.headPos.z()),
         helmR * 1.01f, 0.012f, ironColor * 1.06f);
    ring(QVector3D(pose.headPos.x(), pose.headPos.y() - pose.headR * 0.02f,
                   pose.headPos.z()),
         helmR * 1.01f, 0.012f, ironColor * 1.06f);

    float visorY = pose.headPos.y() + pose.headR * 0.10f;
    float visorZ = pose.headPos.z() + helmR * 0.68f;

    for (int i = 0; i < 3; ++i) {
      float y = visorY + pose.headR * (0.18f - i * 0.12f);
      QVector3D visorL(pose.headPos.x() - helmR * 0.30f, y, visorZ);
      QVector3D visorR(pose.headPos.x() + helmR * 0.30f, y, visorZ);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, visorL, visorR, 0.010f),
               QVector3D(0.15f, 0.15f, 0.15f), nullptr, 1.0f);
    }
  }

  void drawArmorOverlay(const DrawContext &ctx, const HumanoidVariant &v,
                        const HumanoidPose &pose, float yTopCover, float torsoR,
                        float shoulderHalfSpan, float upperArmR,
                        const QVector3D &rightAxis,
                        ISubmitter &out) const override {
    using HP = HumanProportions;

    QVector3D ironColor = v.palette.metal * QVector3D(0.88f, 0.90f, 0.92f);
    QVector3D leatherColor = v.palette.leather * 0.95f;

    QVector3D chestTop(0, yTopCover + 0.02f, 0);
    QVector3D chestBot(0, HP::WAIST_Y + 0.08f, 0);
    float rChest = torsoR * 1.14f;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, chestTop, chestBot, rChest), ironColor,
             nullptr, 1.0f);

    auto drawPauldron = [&](const QVector3D &shoulder,
                            const QVector3D &outward) {
      for (int i = 0; i < 3; ++i) {
        float segY = shoulder.y() + 0.03f - i * 0.040f;
        float segR = upperArmR * (2.2f - i * 0.10f);
        QVector3D segPos = shoulder + outward * (0.015f + i * 0.006f);
        segPos.setY(segY);

        out.mesh(getUnitSphere(), sphereAt(ctx.model, segPos, segR),
                 i == 0 ? ironColor * 1.04f : ironColor * (1.0f - i * 0.02f),
                 nullptr, 1.0f);
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

      for (int i = 0; i < 2; ++i) {
        float t0 = 0.12f + i * 0.28f;
        float t1 = t0 + 0.24f;
        QVector3D a = shoulder + dir * (t0 * len);
        QVector3D b = shoulder + dir * (t1 * len);
        float r = upperArmR * (1.26f - i * 0.03f);

        out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r),
                 ironColor * (0.96f - i * 0.02f), nullptr, 1.0f);
      }
    };

    drawArmPlate(pose.shoulderL, pose.elbowL);
    drawArmPlate(pose.shoulderR, pose.elbowR);

    for (int i = 0; i < 3; ++i) {
      float y = HP::WAIST_Y + 0.06f - i * 0.035f;
      float r = torsoR * (1.12f + i * 0.020f);
      QVector3D stripTop(0, y, 0);
      QVector3D stripBot(0, y - 0.030f, 0);

      out.mesh(getUnitCone(), coneFromTo(ctx.model, stripTop, stripBot, r),
               leatherColor * (0.98f - i * 0.02f), nullptr, 1.0f);
    }
  }

  void drawShoulderDecorations(const DrawContext &ctx, const HumanoidVariant &v,
                               const HumanoidPose &pose, float yTopCover,
                               float yNeck, const QVector3D &rightAxis,
                               ISubmitter &out) const override {}

private:
  static SpearmanExtras computeSpearmanExtras(uint32_t seed,
                                              const HumanoidVariant &v) {
    SpearmanExtras e;

    e.spearShaftColor = v.palette.leather * QVector3D(0.85f, 0.75f, 0.65f);
    e.spearheadColor = QVector3D(0.75f, 0.76f, 0.80f);

    e.spearLength = 1.15f + (hash01(seed ^ 0xABCDu) - 0.5f) * 0.10f;
    e.spearShaftRadius = 0.018f + (hash01(seed ^ 0x7777u) - 0.5f) * 0.003f;
    e.spearheadLength = 0.16f + (hash01(seed ^ 0xBEEFu) - 0.5f) * 0.04f;

    return e;
  }

  static void drawSpear(const DrawContext &ctx, const HumanoidPose &pose,
                        const HumanoidVariant &v, const SpearmanExtras &extras,
                        const AnimationInputs &anim, bool isAttacking,
                        float attackPhase, ISubmitter &out) {
    QVector3D gripPos = pose.handR;

    QVector3D spearDir = QVector3D(0.05f, 0.55f, 0.85f);
    if (spearDir.lengthSquared() > 1e-6f)
      spearDir.normalize();

    if (anim.isInHoldMode || anim.isExitingHold) {
      float t = anim.isInHoldMode ? 1.0f : (1.0f - anim.holdExitProgress);

      QVector3D bracedDir = QVector3D(0.05f, 0.40f, 0.91f);
      if (bracedDir.lengthSquared() > 1e-6f)
        bracedDir.normalize();

      spearDir = spearDir * (1.0f - t) + bracedDir * t;
      if (spearDir.lengthSquared() > 1e-6f)
        spearDir.normalize();
    } else if (isAttacking) {
      if (attackPhase >= 0.30f && attackPhase < 0.50f) {
        float t = (attackPhase - 0.30f) / 0.20f;

        QVector3D attackDir = QVector3D(0.03f, -0.15f, 1.0f);
        if (attackDir.lengthSquared() > 1e-6f)
          attackDir.normalize();

        spearDir = spearDir * (1.0f - t) + attackDir * t;
        if (spearDir.lengthSquared() > 1e-6f)
          spearDir.normalize();
      }
    }

    QVector3D shaftBase = gripPos - spearDir * 0.28f;
    QVector3D shaftMid = gripPos + spearDir * (extras.spearLength * 0.5f);
    QVector3D shaftTip = gripPos + spearDir * extras.spearLength;

    shaftMid.setY(shaftMid.y() + 0.02f);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, shaftBase, shaftMid,
                             extras.spearShaftRadius),
             extras.spearShaftColor, nullptr, 1.0f);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, shaftMid, shaftTip,
                             extras.spearShaftRadius * 0.95f),
             extras.spearShaftColor * 0.98f, nullptr, 1.0f);

    QVector3D spearheadBase = shaftTip;
    QVector3D spearheadTip = shaftTip + spearDir * extras.spearheadLength;

    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, spearheadBase, spearheadTip,
                        extras.spearShaftRadius * 1.8f),
             extras.spearheadColor, nullptr, 1.0f);

    QVector3D gripEnd = gripPos + spearDir * 0.10f;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, gripPos, gripEnd,
                             extras.spearShaftRadius * 1.5f),
             v.palette.leather * 0.92f, nullptr, 1.0f);
  }
};

void registerSpearmanRenderer(Render::GL::EntityRendererRegistry &registry) {
  static SpearmanRenderer renderer;
  registry.registerRenderer(
      "spearman", [](const DrawContext &ctx, ISubmitter &out) {
        static SpearmanRenderer staticRenderer;
        Shader *spearmanShader = nullptr;
        if (ctx.backend) {
          spearmanShader = ctx.backend->shader(QStringLiteral("spearman"));
        }
        Renderer *sceneRenderer = dynamic_cast<Renderer *>(&out);
        if (sceneRenderer && spearmanShader) {
          sceneRenderer->setCurrentShader(spearmanShader);
        }
        staticRenderer.render(ctx, out);
        if (sceneRenderer) {
          sceneRenderer->setCurrentShader(nullptr);
        }
      });
}

} // namespace Render::GL
