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

struct SpearmanExtras {
  QVector3D spearShaftColor;
  QVector3D spearheadColor;
  QVector3D shieldColor;
  float spearLength = 1.20f;
  float spearShaftRadius = 0.020f;
  float spearheadLength = 0.18f;
  float shieldRadius = 0.16f;
  bool hasShield = true;
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

    if (anim.isAttacking && anim.isMelee) {
      const float attackCycleTime = 0.8f;
      float attackPhase = std::fmod(anim.time * (1.0f / attackCycleTime), 1.0f);

      QVector3D guardPos(0.25f, HP::SHOULDER_Y + 0.10f, 0.20f);
      QVector3D preparePos(0.30f, HP::SHOULDER_Y + 0.35f, -0.10f);
      QVector3D thrustPos(0.30f, HP::SHOULDER_Y + 0.15f, 0.80f);
      QVector3D recoverPos(0.25f, HP::SHOULDER_Y + 0.05f, 0.35f);

      if (attackPhase < 0.20f) {
        float t = easeInOutCubic(attackPhase / 0.20f);
        pose.handR = guardPos * (1.0f - t) + preparePos * t;
        pose.handL = QVector3D(-0.20f, HP::SHOULDER_Y - 0.05f, 0.15f);
      } else if (attackPhase < 0.30f) {
        pose.handR = preparePos;
        pose.handL = QVector3D(-0.20f, HP::SHOULDER_Y - 0.05f, 0.15f);
      } else if (attackPhase < 0.50f) {
        float t = (attackPhase - 0.30f) / 0.20f;
        t = t * t * t;
        pose.handR = preparePos * (1.0f - t) + thrustPos * t;
        pose.handL =
            QVector3D(-0.20f, HP::SHOULDER_Y - 0.05f * (1.0f - t * 0.5f),
                      0.15f + 0.15f * t);
      } else if (attackPhase < 0.70f) {
        float t = easeInOutCubic((attackPhase - 0.50f) / 0.20f);
        pose.handR = thrustPos * (1.0f - t) + recoverPos * t;
        pose.handL = QVector3D(-0.20f, HP::SHOULDER_Y - 0.025f * (1.0f - t),
                               lerp(0.30f, 0.18f, t));
      } else {
        float t = smoothstep(0.70f, 1.0f, attackPhase);
        pose.handR = recoverPos * (1.0f - t) + guardPos * t;
        pose.handL = QVector3D(-0.20f - 0.02f * (1.0f - t),
                               HP::SHOULDER_Y + armHeightJitter * (1.0f - t),
                               lerp(0.18f, 0.15f, t));
      }
    } else {
      pose.handR = QVector3D(0.28f + armAsymmetry,
                             HP::SHOULDER_Y - 0.02f + armHeightJitter, 0.30f);
      pose.handL = QVector3D(-0.22f - 0.5f * armAsymmetry,
                             HP::SHOULDER_Y + 0.5f * armHeightJitter, 0.18f);
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

    drawSpear(ctx, pose, v, extras, isAttacking, attackPhase, out);
    if (extras.hasShield) {
      drawShield(ctx, pose, v, extras, out);
    }
  }

  void drawHelmet(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose, ISubmitter &out) const override {
    using HP = HumanProportions;

    QVector3D ironColor = v.palette.metal * QVector3D(0.88f, 0.90f, 0.92f);

    float helmR = pose.headR * 1.12f;
    QVector3D helmBot(0, pose.headPos.y() - pose.headR * 0.15f, 0);
    QVector3D helmTop(0, pose.headPos.y() + pose.headR * 1.25f, 0);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, helmBot, helmTop, helmR), ironColor,
             nullptr, 1.0f);

    QVector3D capTop(0, pose.headPos.y() + pose.headR * 1.32f, 0);
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

    ring(QVector3D(0, pose.headPos.y() + pose.headR * 0.95f, 0), helmR * 1.01f,
         0.012f, ironColor * 1.06f);
    ring(QVector3D(0, pose.headPos.y() - pose.headR * 0.02f, 0), helmR * 1.01f,
         0.012f, ironColor * 1.06f);

    float visorY = pose.headPos.y() + pose.headR * 0.10f;
    float visorZ = helmR * 0.68f;

    for (int i = 0; i < 3; ++i) {
      float y = visorY + pose.headR * (0.18f - i * 0.12f);
      QVector3D visorL(-helmR * 0.30f, y, visorZ);
      QVector3D visorR(helmR * 0.30f, y, visorZ);
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

      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, stripTop, stripBot, r),
               leatherColor * (0.98f - i * 0.02f), nullptr, 1.0f);
    }
  }

  void drawShoulderDecorations(const DrawContext &ctx, const HumanoidVariant &v,
                               const HumanoidPose &pose, float yTopCover,
                               float yNeck, const QVector3D &rightAxis,
                               ISubmitter &out) const override {
  }

private:
  static SpearmanExtras computeSpearmanExtras(uint32_t seed,
                                              const HumanoidVariant &v) {
    SpearmanExtras e;

    e.spearShaftColor = v.palette.leather * QVector3D(0.85f, 0.75f, 0.65f);
    e.spearheadColor = QVector3D(0.75f, 0.76f, 0.80f);

    float shieldHue = hash01(seed ^ 0x12345u);
    if (shieldHue < 0.50f) {
      e.shieldColor = v.palette.cloth * 1.05f;
    } else {
      e.shieldColor = v.palette.leather * 1.20f;
    }

    e.spearLength = 1.15f + (hash01(seed ^ 0xABCDu) - 0.5f) * 0.10f;
    e.spearShaftRadius = 0.018f + (hash01(seed ^ 0x7777u) - 0.5f) * 0.003f;
    e.spearheadLength = 0.16f + (hash01(seed ^ 0xBEEFu) - 0.5f) * 0.04f;
    e.shieldRadius = 0.15f + (hash01(seed ^ 0xDEF0u) - 0.5f) * 0.03f;

    e.hasShield = (hash01(seed ^ 0x5555u) > 0.20f);
    return e;
  }

  static void drawSpear(const DrawContext &ctx, const HumanoidPose &pose,
                        const HumanoidVariant &v, const SpearmanExtras &extras,
                        bool isAttacking, float attackPhase, ISubmitter &out) {
    QVector3D gripPos = pose.handR;

    QVector3D spearDir = QVector3D(0.08f, 0.15f, 1.0f);
    if (spearDir.lengthSquared() > 1e-6f)
      spearDir.normalize();

    if (isAttacking) {
      if (attackPhase >= 0.30f && attackPhase < 0.50f) {
        float t = (attackPhase - 0.30f) / 0.20f;
        QVector3D attackDir = QVector3D(0.05f, 0.02f, 1.0f);
        if (attackDir.lengthSquared() > 1e-6f)
          attackDir.normalize();
        
        spearDir = spearDir * (1.0f - t) + attackDir * t;
        if (spearDir.lengthSquared() > 1e-6f)
          spearDir.normalize();
      }
    }

    QVector3D shaftBase = gripPos - spearDir * 0.25f;
    QVector3D shaftTip = gripPos + spearDir * extras.spearLength;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, shaftBase, shaftTip,
                             extras.spearShaftRadius),
             extras.spearShaftColor, nullptr, 1.0f);

    QVector3D spearheadBase = shaftTip;
    QVector3D spearheadTip = shaftTip + spearDir * extras.spearheadLength;

    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, spearheadBase, spearheadTip,
                        extras.spearShaftRadius * 1.8f),
             extras.spearheadColor, nullptr, 1.0f);

    QVector3D gripEnd = gripPos + spearDir * 0.08f;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, gripPos, gripEnd,
                             extras.spearShaftRadius * 1.4f),
             v.palette.leather, nullptr, 1.0f);
  }

  static void drawShield(const DrawContext &ctx, const HumanoidPose &pose,
                         const HumanoidVariant &v, const SpearmanExtras &extras,
                         ISubmitter &out) {
    const float scaleFactor = 2.2f;
    const float R = extras.shieldRadius * scaleFactor;

    const float yawDeg = -65.0f;
    QMatrix4x4 rot;
    rot.rotate(yawDeg, 0.0f, 1.0f, 0.0f);

    const QVector3D n = rot.map(QVector3D(0.0f, 0.0f, 1.0f));
    const QVector3D axisX = rot.map(QVector3D(1.0f, 0.0f, 0.0f));
    const QVector3D axisY = rot.map(QVector3D(0.0f, 1.0f, 0.0f));

    QVector3D shieldCenter =
        pose.handL + axisX * (-R * 0.30f) + axisY * (-0.04f) + n * (0.05f);

    const float plateHalf = 0.0012f;
    const float plateFull = plateHalf * 2.0f;

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shieldCenter + n * plateHalf);
      m.rotate(yawDeg, 0.0f, 1.0f, 0.0f);
      m.scale(R, R, plateFull);
      out.mesh(getUnitCylinder(), m, extras.shieldColor, nullptr, 1.0f);
    }

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shieldCenter - n * plateHalf);
      m.rotate(yawDeg, 0.0f, 1.0f, 0.0f);
      m.scale(R * 0.985f, R * 0.985f, plateFull);
      out.mesh(getUnitCylinder(), m, v.palette.leather * 0.85f, nullptr, 1.0f);
    }

    auto drawRingRotated = [&](float radius, float thickness,
                               const QVector3D &color) {
      const int segments = 16;
      for (int i = 0; i < segments; ++i) {
        float a0 = (float)i / segments * 2.0f * 3.14159265f;
        float a1 = (float)(i + 1) / segments * 2.0f * 3.14159265f;

        QVector3D v0 =
            QVector3D(radius * std::cos(a0), radius * std::sin(a0), 0.0f);
        QVector3D v1 =
            QVector3D(radius * std::cos(a1), radius * std::sin(a1), 0.0f);

        QVector3D p0 = shieldCenter + rot.map(v0);
        QVector3D p1 = shieldCenter + rot.map(v1);

        out.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, p0, p1, thickness), color, nullptr,
                 1.0f);
      }
    };

    drawRingRotated(R, 0.008f * scaleFactor,
                    QVector3D(0.78f, 0.79f, 0.83f) * 0.95f);

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shieldCenter + n * (0.018f * scaleFactor));
      m.scale(0.038f * scaleFactor);
      out.mesh(getUnitSphere(), m, QVector3D(0.76f, 0.77f, 0.81f), nullptr,
               1.0f);
    }

    {
      QVector3D gripA = shieldCenter - axisX * 0.030f - n * 0.025f;
      QVector3D gripB = shieldCenter + axisX * 0.030f - n * 0.025f;
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, gripA, gripB, 0.008f),
               v.palette.leather, nullptr, 1.0f);
    }
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
