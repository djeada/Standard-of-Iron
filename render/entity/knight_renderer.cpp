#include "knight_renderer.h"
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

struct KnightExtras {
  QVector3D metalColor;
  QVector3D shieldColor;
  float swordLength = 0.80f;
  float swordWidth = 0.065f;
  float shieldRadius = 0.18f;

  float guardHalfWidth = 0.12f;
  float handleRadius = 0.016f;
  float pommelRadius = 0.045f;
  float bladeRicasso = 0.16f;
  float bladeTaperBias = 0.65f;
  bool shieldCrossDecal = false;
  bool hasScabbard = true;
};

class KnightRenderer : public HumanoidRendererBase {
public:
  QVector3D getProportionScaling() const override {
    return QVector3D(1.40f, 1.05f, 1.10f);
  }

private:
  mutable std::unordered_map<uint32_t, KnightExtras> m_extrasCache;

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
      float attackPhase = std::fmod(anim.time * KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0f);

      QVector3D restPos(0.20f, HP::SHOULDER_Y + 0.05f, 0.15f);
      QVector3D preparePos(0.26f, HP::HEAD_TOP_Y + 0.18f, -0.06f);
      QVector3D raisedPos(0.25f, HP::HEAD_TOP_Y + 0.22f, 0.02f);
      QVector3D strikePos(0.30f, HP::WAIST_Y - 0.05f, 0.50f);
      QVector3D recoverPos(0.22f, HP::SHOULDER_Y + 0.02f, 0.22f);

      if (attackPhase < 0.18f) {

        float t = easeInOutCubic(attackPhase / 0.18f);
        pose.handR = restPos * (1.0f - t) + preparePos * t;
        pose.handL =
            QVector3D(-0.21f, HP::SHOULDER_Y - 0.02f - 0.03f * t, 0.15f);
      } else if (attackPhase < 0.32f) {

        float t = easeInOutCubic((attackPhase - 0.18f) / 0.14f);
        pose.handR = preparePos * (1.0f - t) + raisedPos * t;
        pose.handL = QVector3D(-0.21f, HP::SHOULDER_Y - 0.05f, 0.17f);
      } else if (attackPhase < 0.52f) {

        float t = (attackPhase - 0.32f) / 0.20f;
        t = t * t * t;
        pose.handR = raisedPos * (1.0f - t) + strikePos * t;
        pose.handL =
            QVector3D(-0.21f, HP::SHOULDER_Y - 0.03f * (1.0f - 0.5f * t),
                      0.17f + 0.20f * t);
      } else if (attackPhase < 0.72f) {

        float t = easeInOutCubic((attackPhase - 0.52f) / 0.20f);
        pose.handR = strikePos * (1.0f - t) + recoverPos * t;
        pose.handL = QVector3D(-0.20f, HP::SHOULDER_Y - 0.015f * (1.0f - t),
                               lerp(0.37f, 0.20f, t));
      } else {

        float t = smoothstep(0.72f, 1.0f, attackPhase);
        pose.handR = recoverPos * (1.0f - t) + restPos * t;
        pose.handL = QVector3D(-0.20f - 0.02f * (1.0f - t),
                               HP::SHOULDER_Y + armHeightJitter * (1.0f - t),
                               lerp(0.20f, 0.15f, t));
      }
    } else {

      pose.handR = QVector3D(0.30f + armAsymmetry,
                             HP::SHOULDER_Y - 0.02f + armHeightJitter, 0.35f);
      pose.handL = QVector3D(-0.22f - 0.5f * armAsymmetry,
                             HP::SHOULDER_Y + 0.5f * armHeightJitter, 0.18f);
    }
  }

  void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose, const AnimationInputs &anim,
                      ISubmitter &out) const override {
    uint32_t seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFu;

    KnightExtras extras;
    auto it = m_extrasCache.find(seed);
    if (it != m_extrasCache.end()) {
      extras = it->second;
    } else {
      extras = computeKnightExtras(seed, v);
      m_extrasCache[seed] = extras;

      if (m_extrasCache.size() > MAX_EXTRAS_CACHE_SIZE) {
        m_extrasCache.clear();
      }
    }

    bool isAttacking = anim.isAttacking && anim.isMelee;
    float attackPhase = 0.0f;
    if (isAttacking) {
      attackPhase = std::fmod(anim.time * KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0f);
    }

    drawSword(ctx, pose, v, extras, isAttacking, attackPhase, out);
    drawShield(ctx, pose, v, extras, out);

    if (!isAttacking && extras.hasScabbard) {
      drawScabbard(ctx, pose, v, extras, out);
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

    QVector3D crossCenter(0, pose.headPos.y() + pose.headR * 0.60f,
                          helmR * 0.75f);
    QVector3D brassColor = v.palette.metal * QVector3D(1.3f, 1.1f, 0.7f);

    QVector3D crossH1 = crossCenter + QVector3D(-0.04f, 0, 0);
    QVector3D crossH2 = crossCenter + QVector3D(0.04f, 0, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, crossH1, crossH2, 0.008f), brassColor,
             nullptr, 1.0f);

    QVector3D crossV1 = crossCenter + QVector3D(0, -0.04f, 0);
    QVector3D crossV2 = crossCenter + QVector3D(0, 0.04f, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, crossV1, crossV2, 0.008f), brassColor,
             nullptr, 1.0f);
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

    for (int i = 0; i < 4; ++i) {
      float y0 = HP::WAIST_Y + 0.04f - i * 0.038f;
      float y1 = y0 - 0.032f;
      float r0 = rWaist * (1.06f + i * 0.025f);
      out.mesh(
          getUnitCone(),
          coneFromTo(ctx.model, QVector3D(0, y0, 0), QVector3D(0, y1, 0), r0),
          steelColor * (0.96f - i * 0.02f), nullptr, 1.0f);

      if (i < 3) {
        drawRivet(QVector3D(r0 * 0.90f, y0 - 0.016f, 0));
      }
    }

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

    QVector3D helmTop(0, HP::HEAD_TOP_Y - HP::HEAD_RADIUS * 0.15f, 0);
    QMatrix4x4 crestBase = ctx.model;
    crestBase.translate(helmTop);
    crestBase.scale(0.025f, 0.015f, 0.025f);
    out.mesh(getUnitSphere(), crestBase, brassColor * 1.2f, nullptr, 1.0f);

    auto drawStud = [&](const QVector3D &pos) {
      QMatrix4x4 m = ctx.model;
      m.translate(pos);
      m.scale(0.008f);
      out.mesh(getUnitSphere(), m, brassColor * 1.3f, nullptr, 1.0f);
    };

    drawStud(helmTop + QVector3D(0.020f, 0, 0.020f));
    drawStud(helmTop + QVector3D(-0.020f, 0, 0.020f));
    drawStud(helmTop + QVector3D(0.020f, 0, -0.020f));
    drawStud(helmTop + QVector3D(-0.020f, 0, -0.020f));

    auto drawMantling = [&](const QVector3D &startPos,
                            const QVector3D &direction) {
      QVector3D currentPos = startPos;
      for (int i = 0; i < 4; ++i) {
        float segLen = 0.035f - i * 0.005f;
        float segR = 0.020f - i * 0.003f;
        QVector3D nextPos = currentPos + direction * segLen;
        nextPos.setY(nextPos.y() - 0.025f);

        out.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, currentPos, nextPos, segR),
                 mantlingColor * (1.1f - i * 0.06f), nullptr, 1.0f);

        currentPos = nextPos;
      }
    };

    QVector3D mantlingStart(0, HP::CHIN_Y + HP::HEAD_RADIUS * 0.25f, 0);
    drawMantling(mantlingStart + rightAxis * HP::HEAD_RADIUS * 0.95f,
                 rightAxis * 0.5f + QVector3D(0, -0.1f, -0.3f));
    drawMantling(mantlingStart - rightAxis * HP::HEAD_RADIUS * 0.95f,
                 -rightAxis * 0.5f + QVector3D(0, -0.1f, -0.3f));

    auto drawPauldronRivet = [&](const QVector3D &shoulder,
                                 const QVector3D &outward) {
      for (int i = 0; i < 3; ++i) {
        float segY = shoulder.y() + 0.025f - i * 0.045f;
        QVector3D rivetPos = shoulder + outward * (0.04f + i * 0.008f);
        rivetPos.setY(segY);

        drawStud(rivetPos);
      }
    };

    drawPauldronRivet(pose.shoulderL, -rightAxis);
    drawPauldronRivet(pose.shoulderR, rightAxis);

    QVector3D gorgetTop(0, yTopCover + 0.045f, 0);
    for (int i = 0; i < 6; ++i) {
      float angle = (i / 6.0f) * 2.0f * 3.14159265f;
      float x = HP::NECK_RADIUS * 2.58f * std::sin(angle);
      float z = HP::NECK_RADIUS * 2.58f * std::cos(angle);
      drawStud(gorgetTop + QVector3D(x, 0, z));
    }

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
  static KnightExtras computeKnightExtras(uint32_t seed,
                                          const HumanoidVariant &v) {
    KnightExtras e;

    e.metalColor = QVector3D(0.72f, 0.73f, 0.78f);

    float shieldHue = hash01(seed ^ 0x12345u);
    if (shieldHue < 0.45f) {
      e.shieldColor = v.palette.cloth * 1.10f;
    } else if (shieldHue < 0.90f) {
      e.shieldColor = v.palette.leather * 1.25f;
    } else {

      e.shieldColor = e.metalColor * 0.95f;
    }

    e.swordLength = 0.80f + (hash01(seed ^ 0xABCDu) - 0.5f) * 0.16f;
    e.swordWidth = 0.060f + (hash01(seed ^ 0x7777u) - 0.5f) * 0.010f;
    e.shieldRadius = 0.16f + (hash01(seed ^ 0xDEF0u) - 0.5f) * 0.04f;

    e.guardHalfWidth = 0.120f + (hash01(seed ^ 0x3456u) - 0.5f) * 0.020f;
    e.handleRadius = 0.016f + (hash01(seed ^ 0x88AAu) - 0.5f) * 0.003f;
    e.pommelRadius = 0.045f + (hash01(seed ^ 0x19C3u) - 0.5f) * 0.006f;

    e.bladeRicasso =
        clampf(0.14f + (hash01(seed ^ 0xBEEFu) - 0.5f) * 0.04f, 0.10f, 0.20f);
    e.bladeTaperBias = clamp01(0.6f + (hash01(seed ^ 0xFACEu) - 0.5f) * 0.2f);

    e.shieldCrossDecal = (hash01(seed ^ 0xA11Cu) > 0.55f);
    e.hasScabbard = (hash01(seed ^ 0x5CABu) > 0.15f);
    return e;
  }

  static void drawSword(const DrawContext &ctx, const HumanoidPose &pose,
                        const HumanoidVariant &v, const KnightExtras &extras,
                        bool isAttacking, float attackPhase, ISubmitter &out) {
    QVector3D gripPos = pose.handR;

    constexpr float kSwordYawDeg = 25.0f;
    QMatrix4x4 yawM;
    yawM.rotate(kSwordYawDeg, 0.0f, 1.0f, 0.0f);

    QVector3D upish = yawM.map(QVector3D(0.05f, 1.0f, 0.15f));
    QVector3D midish = yawM.map(QVector3D(0.08f, 0.20f, 1.0f));
    QVector3D downish = yawM.map(QVector3D(0.10f, -1.0f, 0.25f));
    if (upish.lengthSquared() > 1e-6f)
      upish.normalize();
    if (midish.lengthSquared() > 1e-6f)
      midish.normalize();
    if (downish.lengthSquared() > 1e-6f)
      downish.normalize();

    QVector3D swordDir = upish;

    if (isAttacking) {
      if (attackPhase < 0.18f) {
        float t = easeInOutCubic(attackPhase / 0.18f);
        swordDir = nlerp(upish, upish, t);
      } else if (attackPhase < 0.32f) {
        float t = easeInOutCubic((attackPhase - 0.18f) / 0.14f);
        swordDir = nlerp(upish, midish, t * 0.35f);
      } else if (attackPhase < 0.52f) {
        float t = (attackPhase - 0.32f) / 0.20f;
        t = t * t * t;
        if (t < 0.5f) {
          float u = t / 0.5f;
          swordDir = nlerp(upish, midish, u);
        } else {
          float u = (t - 0.5f) / 0.5f;
          swordDir = nlerp(midish, downish, u);
        }
      } else if (attackPhase < 0.72f) {
        float t = easeInOutCubic((attackPhase - 0.52f) / 0.20f);
        swordDir = nlerp(downish, midish, t);
      } else {
        float t = smoothstep(0.72f, 1.0f, attackPhase);
        swordDir = nlerp(midish, upish, t);
      }
    }

    QVector3D handleEnd = gripPos - swordDir * 0.10f;
    QVector3D bladeBase = gripPos;
    QVector3D bladeTip = gripPos + swordDir * extras.swordLength;

    out.mesh(
        getUnitCylinder(),
        cylinderBetween(ctx.model, handleEnd, bladeBase, extras.handleRadius),
        v.palette.leather, nullptr, 1.0f);

    QVector3D guardCenter = bladeBase;
    float gw = extras.guardHalfWidth;

    QVector3D guardRight =
        QVector3D::crossProduct(QVector3D(0, 1, 0), swordDir);
    if (guardRight.lengthSquared() < 1e-6f)
      guardRight = QVector3D::crossProduct(QVector3D(1, 0, 0), swordDir);
    guardRight.normalize();

    QVector3D guardL = guardCenter - guardRight * gw;
    QVector3D guardR = guardCenter + guardRight * gw;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, guardL, guardR, 0.014f),
             extras.metalColor, nullptr, 1.0f);

    QMatrix4x4 gl = ctx.model;
    gl.translate(guardL);
    gl.scale(0.018f);
    out.mesh(getUnitSphere(), gl, extras.metalColor, nullptr, 1.0f);
    QMatrix4x4 gr = ctx.model;
    gr.translate(guardR);
    gr.scale(0.018f);
    out.mesh(getUnitSphere(), gr, extras.metalColor, nullptr, 1.0f);

    float L = extras.swordLength;
    float baseW = extras.swordWidth;
    float bladeThickness = baseW * 0.15f;

    float ricassoLen = clampf(extras.bladeRicasso, 0.10f, L * 0.30f);
    QVector3D ricassoEnd = bladeBase + swordDir * ricassoLen;

    float midW = baseW * 0.95f;
    float tipW = baseW * 0.28f;
    float tipStartDist = lerp(ricassoLen, L, 0.70f);
    QVector3D tipStart = bladeBase + swordDir * tipStartDist;

    auto drawFlatSection = [&](const QVector3D &start, const QVector3D &end,
                               float width, const QVector3D &color) {
      QVector3D right = QVector3D::crossProduct(swordDir, QVector3D(0, 1, 0));
      if (right.lengthSquared() < 0.001f) {
        right = QVector3D::crossProduct(swordDir, QVector3D(1, 0, 0));
      }
      right.normalize();

      float offset = width * 0.33f;

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, start, end, bladeThickness), color,
               nullptr, 1.0f);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, start + right * offset,
                               end + right * offset, bladeThickness * 0.8f),
               color * 0.92f, nullptr, 1.0f);

      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, start - right * offset,
                               end - right * offset, bladeThickness * 0.8f),
               color * 0.92f, nullptr, 1.0f);
    };

    drawFlatSection(bladeBase, ricassoEnd, baseW, extras.metalColor);

    drawFlatSection(ricassoEnd, tipStart, midW, extras.metalColor);

    int tipSegments = 3;
    for (int i = 0; i < tipSegments; ++i) {
      float t0 = (float)i / tipSegments;
      float t1 = (float)(i + 1) / tipSegments;
      QVector3D segStart =
          tipStart + swordDir * ((bladeTip - tipStart).length() * t0);
      QVector3D segEnd =
          tipStart + swordDir * ((bladeTip - tipStart).length() * t1);
      float w = lerp(midW, tipW, t1);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, segStart, segEnd, bladeThickness),
               extras.metalColor * (1.0f - i * 0.03f), nullptr, 1.0f);
    }

    QVector3D fullerStart = bladeBase + swordDir * (ricassoLen + 0.02f);
    QVector3D fullerEnd = bladeBase + swordDir * (tipStartDist - 0.06f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, fullerStart, fullerEnd,
                             bladeThickness * 0.6f),
             extras.metalColor * 0.65f, nullptr, 1.0f);

    QVector3D pommel = handleEnd - swordDir * 0.02f;
    QMatrix4x4 pommelMat = ctx.model;
    pommelMat.translate(pommel);
    pommelMat.scale(extras.pommelRadius);
    out.mesh(getUnitSphere(), pommelMat, extras.metalColor, nullptr, 1.0f);

    if (isAttacking && attackPhase >= 0.32f && attackPhase < 0.56f) {
      float t = (attackPhase - 0.32f) / 0.24f;
      float alpha = clamp01(0.35f * (1.0f - t));
      QVector3D trailStart = bladeBase - swordDir * 0.05f;
      QVector3D trailEnd = bladeBase - swordDir * (0.28f + 0.15f * t);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, trailEnd, trailStart, baseW * 0.9f),
               extras.metalColor * 0.9f, nullptr, alpha);
    }
  }

  static void drawShieldDecal(const DrawContext &ctx, const QVector3D &center,
                              float radius, const QVector3D &,
                              const HumanoidVariant &v, ISubmitter &out) {

    QVector3D accent = v.palette.cloth * 1.2f;
    float barR = radius * 0.10f;

    QVector3D top = center + QVector3D(0.0f, radius * 0.95f, 0.0f);
    QVector3D bot = center - QVector3D(0.0f, radius * 0.95f, 0.0f);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, top, bot, barR),
             accent, nullptr, 1.0f);

    QVector3D left = center + QVector3D(-radius * 0.95f, 0.0f, 0.0f);
    QVector3D right = center + QVector3D(radius * 0.95f, 0.0f, 0.0f);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, left, right, barR),
             accent, nullptr, 1.0f);
  }

  static void drawShieldRing(const DrawContext &ctx, const QVector3D &center,
                             float radius, float thickness,
                             const QVector3D &color, ISubmitter &out) {

    const int segments = 12;
    for (int i = 0; i < segments; ++i) {
      float a0 = (float)i / segments * 2.0f * 3.14159265f;
      float a1 = (float)(i + 1) / segments * 2.0f * 3.14159265f;
      QVector3D p0(center.x() + radius * std::cos(a0),
                   center.y() + radius * std::sin(a0), center.z());
      QVector3D p1(center.x() + radius * std::cos(a1),
                   center.y() + radius * std::sin(a1), center.z());
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, p0, p1, thickness),
               color, nullptr, 1.0f);
    }
  }

  static void drawShield(const DrawContext &ctx, const HumanoidPose &pose,
                         const HumanoidVariant &v, const KnightExtras &extras,
                         ISubmitter &out) {

    const float scaleFactor = 2.5f;
    const float R = extras.shieldRadius * scaleFactor;

    const float yawDeg = -70.0f;
    QMatrix4x4 rot;
    rot.rotate(yawDeg, 0.0f, 1.0f, 0.0f);

    const QVector3D n = rot.map(QVector3D(0.0f, 0.0f, 1.0f));
    const QVector3D axisX = rot.map(QVector3D(1.0f, 0.0f, 0.0f));
    const QVector3D axisY = rot.map(QVector3D(0.0f, 1.0f, 0.0f));

    QVector3D shieldCenter =
        pose.handL + axisX * (-R * 0.35f) + axisY * (-0.05f) + n * (0.06f);

    const float plateHalf = 0.0015f;
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
      out.mesh(getUnitCylinder(), m, v.palette.leather * 0.8f, nullptr, 1.0f);
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

    drawRingRotated(R, 0.010f * scaleFactor, extras.metalColor * 0.95f);
    drawRingRotated(R * 0.72f, 0.006f * scaleFactor, v.palette.leather * 0.90f);

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shieldCenter + n * (0.02f * scaleFactor));
      m.scale(0.045f * scaleFactor);
      out.mesh(getUnitSphere(), m, extras.metalColor, nullptr, 1.0f);
    }

    {

      QVector3D gripA = shieldCenter - axisX * 0.035f - n * 0.030f;
      QVector3D gripB = shieldCenter + axisX * 0.035f - n * 0.030f;
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, gripA, gripB, 0.010f),
               v.palette.leather, nullptr, 1.0f);
    }

    if (extras.shieldCrossDecal && (extras.shieldColor != extras.metalColor)) {
      float decalR = R * 0.85f;
      float barR = decalR * 0.10f;

      QVector3D centerFront = shieldCenter + n * (plateFull * 0.5f + 0.0015f);

      QVector3D top = centerFront + axisY * (decalR * 0.95f);
      QVector3D bot = centerFront - axisY * (decalR * 0.95f);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, top, bot, barR),
               v.palette.cloth * 1.2f, nullptr, 1.0f);

      QVector3D left = centerFront - axisX * (decalR * 0.95f);
      QVector3D right = centerFront + axisX * (decalR * 0.95f);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, left, right, barR),
               v.palette.cloth * 1.2f, nullptr, 1.0f);
    }
  }

  static void drawScabbard(const DrawContext &ctx, const HumanoidPose &,
                           const HumanoidVariant &v, const KnightExtras &extras,
                           ISubmitter &out) {
    using HP = HumanProportions;

    QVector3D hip(0.10f, HP::WAIST_Y - 0.04f, -0.02f);
    QVector3D tip = hip + QVector3D(-0.05f, -0.22f, -0.12f);
    float sheathR = extras.swordWidth * 0.85f;

    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, hip, tip, sheathR),
             v.palette.leather * 0.9f, nullptr, 1.0f);

    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, tip, tip + QVector3D(-0.02f, -0.02f, -0.02f),
                        sheathR),
             extras.metalColor, nullptr, 1.0f);

    QVector3D strapA = hip + QVector3D(0.00f, 0.03f, 0.00f);
    QVector3D belt = QVector3D(0.12f, HP::WAIST_Y + 0.01f, 0.02f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, strapA, belt, 0.006f),
             v.palette.leather, nullptr, 1.0f);
  }
};

void registerKnightRenderer(Render::GL::EntityRendererRegistry &registry) {
  static KnightRenderer renderer;
  registry.registerRenderer(
      "knight", [](const DrawContext &ctx, ISubmitter &out) {
        static KnightRenderer staticRenderer;
        Shader *knightShader = nullptr;
        if (ctx.backend) {
          knightShader = ctx.backend->shader(QStringLiteral("knight"));
        }
        Renderer *sceneRenderer = dynamic_cast<Renderer *>(&out);
        if (sceneRenderer && knightShader) {
          sceneRenderer->setCurrentShader(knightShader);
        }
        staticRenderer.render(ctx, out);
        if (sceneRenderer) {
          sceneRenderer->setCurrentShader(nullptr);
        }
      });
}

} 
