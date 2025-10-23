#include "archer_renderer.h"
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
using Render::Geom::sphereAt;

struct ArcherExtras {
  QVector3D stringCol;
  QVector3D fletch;
  QVector3D metalHead;
  float bowRodR = 0.035f;
  float stringR = 0.008f;
  float bowDepth = 0.25f;
  float bowX = 0.0f;
  float bowTopY;
  float bowBotY;
};

class ArcherRenderer : public HumanoidRendererBase {
public:
  QVector3D getProportionScaling() const override {

    return QVector3D(0.94f, 1.01f, 0.96f);
  }

private:
  mutable std::unordered_map<uint32_t, ArcherExtras> m_extrasCache;

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

    float bowX = 0.0f;

    if (anim.isInHoldMode || anim.isExitingHold) {

      float t = anim.isInHoldMode ? 1.0f : (1.0f - anim.holdExitProgress);

      float kneelDepth = 0.45f * t;

      float pelvisY = HP::WAIST_Y - kneelDepth;
      pose.pelvisPos.setY(pelvisY);

      float stanceNarrow = 0.12f;

      float leftKneeY = HP::GROUND_Y + 0.08f * t;
      float leftKneeZ = -0.05f * t;

      pose.kneeL = QVector3D(-stanceNarrow, leftKneeY, leftKneeZ);

      pose.footL = QVector3D(-stanceNarrow - 0.03f, HP::GROUND_Y,
                             leftKneeZ - HP::LOWER_LEG_LEN * 0.95f * t);

      float rightFootZ = 0.30f * t;
      pose.footR =
          QVector3D(stanceNarrow, HP::GROUND_Y + pose.footYOffset, rightFootZ);

      float rightKneeY = pelvisY - 0.10f;
      float rightKneeZ = rightFootZ - 0.05f;

      pose.kneeR = QVector3D(stanceNarrow, rightKneeY, rightKneeZ);

      float upperBodyDrop = kneelDepth;

      pose.shoulderL.setY(HP::SHOULDER_Y - upperBodyDrop);
      pose.shoulderR.setY(HP::SHOULDER_Y - upperBodyDrop);
      pose.neckBase.setY(HP::NECK_BASE_Y - upperBodyDrop);
      pose.headPos.setY((HP::HEAD_TOP_Y + HP::CHIN_Y) * 0.5f - upperBodyDrop);

      float forwardLean = 0.10f * t;
      pose.shoulderL.setZ(pose.shoulderL.z() + forwardLean);
      pose.shoulderR.setZ(pose.shoulderR.z() + forwardLean);
      pose.neckBase.setZ(pose.neckBase.z() + forwardLean * 0.8f);
      pose.headPos.setZ(pose.headPos.z() + forwardLean * 0.7f);

      QVector3D holdHandL(bowX - 0.15f, pose.shoulderL.y() + 0.30f, 0.55f);
      QVector3D holdHandR(bowX + 0.12f, pose.shoulderR.y() + 0.15f, 0.10f);
      QVector3D normalHandL(bowX - 0.05f + armAsymmetry,
                            HP::SHOULDER_Y + 0.05f + armHeightJitter, 0.55f);
      QVector3D normalHandR(0.15f - armAsymmetry * 0.5f,
                            HP::SHOULDER_Y + 0.15f + armHeightJitter * 0.8f,
                            0.20f);

      pose.handL = normalHandL * (1.0f - t) + holdHandL * t;
      pose.handR = normalHandR * (1.0f - t) + holdHandR * t;
    } else {
      pose.handL = QVector3D(bowX - 0.05f + armAsymmetry,
                             HP::SHOULDER_Y + 0.05f + armHeightJitter, 0.55f);
      pose.handR =
          QVector3D(0.15f - armAsymmetry * 0.5f,
                    HP::SHOULDER_Y + 0.15f + armHeightJitter * 0.8f, 0.20f);
    }

    if (anim.isAttacking && !anim.isInHoldMode) {
      float attackPhase = fmod(anim.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0f);

      if (anim.isMelee) {
        QVector3D restPos(0.25f, HP::SHOULDER_Y, 0.10f);
        QVector3D raisedPos(0.30f, HP::HEAD_TOP_Y + 0.2f, -0.05f);
        QVector3D strikePos(0.35f, HP::WAIST_Y, 0.45f);

        if (attackPhase < 0.25f) {
          float t = attackPhase / 0.25f;
          t = t * t;
          pose.handR = restPos * (1.0f - t) + raisedPos * t;
          pose.handL = QVector3D(-0.15f, HP::SHOULDER_Y - 0.1f * t, 0.20f);
        } else if (attackPhase < 0.35f) {
          pose.handR = raisedPos;
          pose.handL = QVector3D(-0.15f, HP::SHOULDER_Y - 0.1f, 0.20f);
        } else if (attackPhase < 0.55f) {
          float t = (attackPhase - 0.35f) / 0.2f;
          t = t * t * t;
          pose.handR = raisedPos * (1.0f - t) + strikePos * t;
          pose.handL =
              QVector3D(-0.15f, HP::SHOULDER_Y - 0.1f * (1.0f - t * 0.5f),
                        0.20f + 0.15f * t);
        } else {
          float t = (attackPhase - 0.55f) / 0.45f;
          t = 1.0f - (1.0f - t) * (1.0f - t);
          pose.handR = strikePos * (1.0f - t) + restPos * t;
          pose.handL = QVector3D(-0.15f, HP::SHOULDER_Y - 0.05f * (1.0f - t),
                                 0.35f * (1.0f - t) + 0.20f * t);
        }
      } else {
        QVector3D aimPos(0.18f, HP::SHOULDER_Y + 0.18f, 0.35f);
        QVector3D drawPos(0.22f, HP::SHOULDER_Y + 0.10f, -0.30f);
        QVector3D releasePos(0.18f, HP::SHOULDER_Y + 0.20f, 0.10f);

        if (attackPhase < 0.20f) {
          float t = attackPhase / 0.20f;
          t = t * t;
          pose.handR = aimPos * (1.0f - t) + drawPos * t;
          pose.handL = QVector3D(bowX - 0.05f, HP::SHOULDER_Y + 0.05f, 0.55f);

          float shoulderTwist = t * 0.08f;
          pose.shoulderR.setY(pose.shoulderR.y() + shoulderTwist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulderTwist * 0.5f);
        } else if (attackPhase < 0.50f) {
          pose.handR = drawPos;
          pose.handL = QVector3D(bowX - 0.05f, HP::SHOULDER_Y + 0.05f, 0.55f);

          float shoulderTwist = 0.08f;
          pose.shoulderR.setY(pose.shoulderR.y() + shoulderTwist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulderTwist * 0.5f);
        } else if (attackPhase < 0.58f) {
          float t = (attackPhase - 0.50f) / 0.08f;
          t = t * t * t;
          pose.handR = drawPos * (1.0f - t) + releasePos * t;
          pose.handL = QVector3D(bowX - 0.05f, HP::SHOULDER_Y + 0.05f, 0.55f);

          float shoulderTwist = 0.08f * (1.0f - t * 0.6f);
          pose.shoulderR.setY(pose.shoulderR.y() + shoulderTwist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulderTwist * 0.5f);

          pose.headPos.setZ(pose.headPos.z() - t * 0.04f);
        } else {
          float t = (attackPhase - 0.58f) / 0.42f;
          t = 1.0f - (1.0f - t) * (1.0f - t);
          pose.handR = releasePos * (1.0f - t) + aimPos * t;
          pose.handL = QVector3D(bowX - 0.05f, HP::SHOULDER_Y + 0.05f, 0.55f);

          float shoulderTwist = 0.08f * 0.4f * (1.0f - t);
          pose.shoulderR.setY(pose.shoulderR.y() + shoulderTwist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulderTwist * 0.5f);

          pose.headPos.setZ(pose.headPos.z() - 0.04f * (1.0f - t));
        }
      }
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

  void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose, const AnimationInputs &anim,
                      ISubmitter &out) const override {
    using HP = HumanProportions;

    QVector3D teamTint = resolveTeamTint(ctx);
    uint32_t seed = 0u;
    if (ctx.entity) {
      auto *unit = ctx.entity->getComponent<Engine::Core::UnitComponent>();
      if (unit)
        seed ^= uint32_t(unit->ownerId * 2654435761u);
      seed ^= uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFu);
    }

    ArcherExtras extras;
    auto it = m_extrasCache.find(seed);
    if (it != m_extrasCache.end()) {
      extras = it->second;
    } else {
      extras.metalHead = Render::Geom::clampVec01(v.palette.metal * 1.15f);
      extras.stringCol = QVector3D(0.30f, 0.30f, 0.32f);
      auto tint = [&](float k) {
        return QVector3D(clamp01(teamTint.x() * k), clamp01(teamTint.y() * k),
                         clamp01(teamTint.z() * k));
      };
      extras.fletch = tint(0.9f);
      extras.bowTopY = HP::SHOULDER_Y + 0.55f;
      extras.bowBotY = HP::WAIST_Y - 0.25f;

      m_extrasCache[seed] = extras;

      if (m_extrasCache.size() > MAX_EXTRAS_CACHE_SIZE) {
        m_extrasCache.clear();
      }
    }

    drawQuiver(ctx, v, pose, extras, seed, out);

    float attackPhase = 0.0f;
    if (anim.isAttacking && !anim.isMelee) {
      attackPhase = fmod(anim.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0f);
    }
    drawBowAndArrow(ctx, pose, v, extras, anim.isAttacking && !anim.isMelee,
                    attackPhase, out);
  }

  void drawHelmet(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose, ISubmitter &out) const override {
    using HP = HumanProportions;

    QVector3D helmetColor = v.palette.metal * QVector3D(1.08f, 0.98f, 0.78f);
    QVector3D helmetAccent = helmetColor * 1.12f;

    QVector3D helmetTop(0, pose.headPos.y() + pose.headR * 1.28f, 0);
    QVector3D helmetBot(0, pose.headPos.y() + pose.headR * 0.08f, 0);
    float helmetR = pose.headR * 1.10f;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, helmetBot, helmetTop, helmetR),
             helmetColor, nullptr, 1.0f);

    QVector3D apexPos(0, pose.headPos.y() + pose.headR * 1.48f, 0);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, helmetTop, apexPos, helmetR * 0.97f),
             helmetAccent, nullptr, 1.0f);

    auto ring = [&](const QVector3D &center, float r, float h,
                    const QVector3D &col) {
      QVector3D a = center + QVector3D(0, h * 0.5f, 0);
      QVector3D b = center - QVector3D(0, h * 0.5f, 0);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r), col,
               nullptr, 1.0f);
    };

    QVector3D browPos(0, pose.headPos.y() + pose.headR * 0.35f, 0);
    ring(browPos, helmetR * 1.07f, 0.020f, helmetAccent);

    ring(QVector3D(0, pose.headPos.y() + pose.headR * 0.65f, 0),
         helmetR * 1.03f, 0.015f, helmetColor * 1.05f);
    ring(QVector3D(0, pose.headPos.y() + pose.headR * 0.95f, 0),
         helmetR * 1.01f, 0.012f, helmetColor * 1.03f);

    float cheekW = pose.headR * 0.48f;
    QVector3D cheekTop(0, pose.headPos.y() + pose.headR * 0.22f, 0);
    QVector3D cheekBot(0, pose.headPos.y() - pose.headR * 0.42f, 0);

    QVector3D cheekLTop = cheekTop + QVector3D(-cheekW, 0, pose.headR * 0.38f);
    QVector3D cheekLBot =
        cheekBot + QVector3D(-cheekW * 0.82f, 0, pose.headR * 0.28f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cheekLBot, cheekLTop, 0.028f),
             helmetColor * 0.96f, nullptr, 1.0f);

    QVector3D cheekRTop = cheekTop + QVector3D(cheekW, 0, pose.headR * 0.38f);
    QVector3D cheekRBot =
        cheekBot + QVector3D(cheekW * 0.82f, 0, pose.headR * 0.28f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cheekRBot, cheekRTop, 0.028f),
             helmetColor * 0.96f, nullptr, 1.0f);

    QVector3D neckGuardTop(0, pose.headPos.y() + pose.headR * 0.03f,
                           -pose.headR * 0.82f);
    QVector3D neckGuardBot(0, pose.headPos.y() - pose.headR * 0.32f,
                           -pose.headR * 0.88f);
    out.mesh(
        getUnitCylinder(),
        cylinderBetween(ctx.model, neckGuardBot, neckGuardTop, helmetR * 0.88f),
        helmetColor * 0.93f, nullptr, 1.0f);

    QVector3D crestBase = apexPos;
    QVector3D crestMid = crestBase + QVector3D(0, 0.09f, 0);
    QVector3D crestTop = crestMid + QVector3D(0, 0.12f, 0);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, crestBase, crestMid, 0.018f),
             helmetAccent, nullptr, 1.0f);

    out.mesh(getUnitCone(), coneFromTo(ctx.model, crestMid, crestTop, 0.042f),
             QVector3D(0.88f, 0.18f, 0.18f), nullptr, 1.0f);

    out.mesh(getUnitSphere(), sphereAt(ctx.model, crestTop, 0.020f),
             helmetAccent, nullptr, 1.0f);
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

    QVector3D mailColor = v.palette.metal * QVector3D(0.85f, 0.87f, 0.92f);
    QVector3D leatherTrim = v.palette.leatherDark * 0.90f;

    float waistY = pose.pelvisPos.y();

    QVector3D mailTop(0, yTopCover + 0.01f, 0);
    QVector3D mailMid(0, (yTopCover + waistY) * 0.5f, 0);
    QVector3D mailBot(0, waistY + 0.08f, 0);
    float rTop = torsoR * 1.10f;
    float rMid = torsoR * 1.08f;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, mailTop, mailMid, rTop), mailColor,
             nullptr, 1.0f);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, mailMid, mailBot, rMid),
             mailColor * 0.95f, nullptr, 1.0f);

    for (int i = 0; i < 3; ++i) {
      float y = mailTop.y() - (i * 0.12f);
      ring(QVector3D(0, y, 0), rTop * (1.01f + i * 0.005f), 0.012f,
           leatherTrim);
    }

    auto drawPauldron = [&](const QVector3D &shoulder,
                            const QVector3D &outward) {
      for (int i = 0; i < 3; ++i) {
        float segY = shoulder.y() + 0.02f - i * 0.035f;
        float segR = upperArmR * (2.2f - i * 0.15f);
        QVector3D segTop(shoulder.x(), segY + 0.025f, shoulder.z());
        QVector3D segBot(shoulder.x(), segY - 0.010f, shoulder.z());

        segTop += outward * 0.02f;
        segBot += outward * 0.02f;

        out.mesh(getUnitSphere(), sphereAt(ctx.model, segTop, segR),
                 mailColor * (1.0f - i * 0.05f), nullptr, 1.0f);
      }
    };

    drawPauldron(pose.shoulderL, -rightAxis);
    drawPauldron(pose.shoulderR, rightAxis);

    auto drawManica = [&](const QVector3D &shoulder, const QVector3D &elbow) {
      QVector3D dir = (elbow - shoulder);
      float len = dir.length();
      if (len < 1e-5f)
        return;
      dir /= len;

      for (int i = 0; i < 4; ++i) {
        float t0 = 0.08f + i * 0.18f;
        float t1 = t0 + 0.16f;
        QVector3D a = shoulder + dir * (t0 * len);
        QVector3D b = shoulder + dir * (t1 * len);
        float r = upperArmR * (1.25f - i * 0.03f);
        out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r),
                 mailColor * (0.95f - i * 0.03f), nullptr, 1.0f);
      }
    };

    drawManica(pose.shoulderL, pose.elbowL);
    drawManica(pose.shoulderR, pose.elbowR);

    QVector3D beltTop(0, waistY + 0.06f, 0);
    QVector3D beltBot(0, waistY - 0.02f, 0);
    float beltR = torsoR * 1.12f;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, beltTop, beltBot, beltR), leatherTrim,
             nullptr, 1.0f);

    QVector3D brassColor = v.palette.metal * QVector3D(1.2f, 1.0f, 0.65f);
    ring(QVector3D(0, waistY + 0.02f, 0), beltR * 1.02f, 0.010f, brassColor);

    auto drawPteruge = [&](float angle, float yStart, float length) {
      float rad = torsoR * 1.15f;
      float x = rad * std::sin(angle);
      float z = rad * std::cos(angle);
      QVector3D top(x, yStart, z);
      QVector3D bot(x * 0.95f, yStart - length, z * 0.95f);
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, top, bot, 0.018f),
               leatherTrim * 0.85f, nullptr, 1.0f);
    };

    float shoulderPterugeY = yTopCover - 0.02f;
    for (int i = 0; i < 8; ++i) {
      float angle = (i / 8.0f) * 2.0f * 3.14159265f;
      drawPteruge(angle, shoulderPterugeY, 0.14f);
    }

    float waistPterugeY = waistY - 0.04f;
    for (int i = 0; i < 10; ++i) {
      float angle = (i / 10.0f) * 2.0f * 3.14159265f;
      drawPteruge(angle, waistPterugeY, 0.18f);
    }

    QVector3D collarTop(0, yTopCover + 0.018f, 0);
    QVector3D collarBot(0, yTopCover - 0.008f, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, collarTop, collarBot,
                             HP::NECK_RADIUS * 1.8f),
             mailColor * 1.05f, nullptr, 1.0f);
  }

  void drawShoulderDecorations(const DrawContext &ctx, const HumanoidVariant &v,
                               const HumanoidPose &pose, float yTopCover,
                               float yNeck, const QVector3D &rightAxis,
                               ISubmitter &out) const override {
    using HP = HumanProportions;

    QVector3D brassColor = v.palette.metal * QVector3D(1.2f, 1.0f, 0.65f);

    auto drawPhalera = [&](const QVector3D &pos) {
      QMatrix4x4 m = ctx.model;
      m.translate(pos);
      m.scale(0.025f);
      out.mesh(getUnitSphere(), m, brassColor, nullptr, 1.0f);
    };

    drawPhalera(pose.shoulderL + QVector3D(0, 0.05f, 0.02f));

    drawPhalera(pose.shoulderR + QVector3D(0, 0.05f, 0.02f));

    QVector3D claspPos(0, yNeck + 0.02f, 0.08f);
    QMatrix4x4 claspM = ctx.model;
    claspM.translate(claspPos);
    claspM.scale(0.020f);
    out.mesh(getUnitSphere(), claspM, brassColor * 1.1f, nullptr, 1.0f);

    QVector3D capeTop = claspPos + QVector3D(0, -0.02f, -0.05f);
    QVector3D capeBot = claspPos + QVector3D(0, -0.25f, -0.15f);
    QVector3D redFabric = v.palette.cloth * QVector3D(1.2f, 0.3f, 0.3f);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, capeTop, capeBot, 0.025f),
             redFabric * 0.85f, nullptr, 1.0f);
  }

private:
  static void drawQuiver(const DrawContext &ctx, const HumanoidVariant &v,
                         const HumanoidPose &pose, const ArcherExtras &extras,
                         uint32_t seed, ISubmitter &out) {
    using HP = HumanProportions;

    QVector3D spineMid = (pose.shoulderL + pose.shoulderR) * 0.5f;
    QVector3D quiverOffset(-0.08f, 0.10f, -0.25f);
    QVector3D qTop = spineMid + quiverOffset;
    QVector3D qBase = qTop + QVector3D(-0.02f, -0.30f, 0.03f);

    float quiverR = HP::HEAD_RADIUS * 0.45f;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, qBase, qTop, quiverR),
             v.palette.leather, nullptr, 1.0f);

    float j = (hash01(seed) - 0.5f) * 0.04f;
    float k = (hash01(seed ^ 0x9E3779B9u) - 0.5f) * 0.04f;

    QVector3D a1 = qTop + QVector3D(0.00f + j, 0.08f, 0.00f + k);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, qTop, a1, 0.010f),
             v.palette.wood, nullptr, 1.0f);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, a1, a1 + QVector3D(0, 0.05f, 0), 0.025f),
             extras.fletch, nullptr, 1.0f);

    QVector3D a2 = qTop + QVector3D(0.02f - j, 0.07f, 0.02f - k);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, qTop, a2, 0.010f),
             v.palette.wood, nullptr, 1.0f);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, a2, a2 + QVector3D(0, 0.05f, 0), 0.025f),
             extras.fletch, nullptr, 1.0f);
  }

  static void drawBowAndArrow(const DrawContext &ctx, const HumanoidPose &pose,
                              const HumanoidVariant &v,
                              const ArcherExtras &extras, bool isAttacking,
                              float attackPhase, ISubmitter &out) {
    const QVector3D up(0.0f, 1.0f, 0.0f);
    const QVector3D forward(0.0f, 0.0f, 1.0f);

    QVector3D grip = pose.handL;

    float bowPlaneZ = 0.45f;
    QVector3D topEnd(extras.bowX, extras.bowTopY, bowPlaneZ);
    QVector3D botEnd(extras.bowX, extras.bowBotY, bowPlaneZ);

    QVector3D nock(
        extras.bowX,
        clampf(pose.handR.y(), extras.bowBotY + 0.05f, extras.bowTopY - 0.05f),
        clampf(pose.handR.z(), bowPlaneZ - 0.30f, bowPlaneZ + 0.30f));

    const int segs = 22;
    auto qBezier = [](const QVector3D &a, const QVector3D &c,
                      const QVector3D &b, float t) {
      float u = 1.0f - t;
      return u * u * a + 2.0f * u * t * c + t * t * b;
    };

    float bowMidY = (topEnd.y() + botEnd.y()) * 0.5f;

    float ctrlY = bowMidY + 0.45f;

    QVector3D ctrl(extras.bowX, ctrlY, bowPlaneZ + extras.bowDepth * 0.6f);

    QVector3D prev = botEnd;
    for (int i = 1; i <= segs; ++i) {
      float t = float(i) / float(segs);
      QVector3D cur = qBezier(botEnd, ctrl, topEnd, t);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, prev, cur, extras.bowRodR),
               v.palette.wood, nullptr, 1.0f);
      prev = cur;
    }
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, grip - up * 0.05f, grip + up * 0.05f,
                             extras.bowRodR * 1.45f),
             v.palette.wood, nullptr, 1.0f);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, topEnd, nock, extras.stringR),
             extras.stringCol, nullptr, 1.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, nock, botEnd, extras.stringR),
             extras.stringCol, nullptr, 1.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, pose.handR, nock, 0.0045f),
             extras.stringCol * 0.9f, nullptr, 1.0f);

    bool showArrow = !isAttacking || (isAttacking && attackPhase >= 0.0f &&
                                      attackPhase < 0.52f);

    if (showArrow) {
      QVector3D tail = nock - forward * 0.06f;
      QVector3D tip = tail + forward * 0.90f;
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, tail, tip, 0.018f),
               v.palette.wood, nullptr, 1.0f);
      QVector3D headBase = tip - forward * 0.10f;
      out.mesh(getUnitCone(), coneFromTo(ctx.model, headBase, tip, 0.05f),
               extras.metalHead, nullptr, 1.0f);
      QVector3D f1b = tail - forward * 0.02f, f1a = f1b - forward * 0.06f;
      QVector3D f2b = tail + forward * 0.02f, f2a = f2b + forward * 0.06f;
      out.mesh(getUnitCone(), coneFromTo(ctx.model, f1b, f1a, 0.04f),
               extras.fletch, nullptr, 1.0f);
      out.mesh(getUnitCone(), coneFromTo(ctx.model, f2a, f2b, 0.04f),
               extras.fletch, nullptr, 1.0f);
    }
  }
};

void registerArcherRenderer(Render::GL::EntityRendererRegistry &registry) {
  static ArcherRenderer renderer;
  registry.registerRenderer(
      "archer", [](const DrawContext &ctx, ISubmitter &out) {
        static ArcherRenderer staticRenderer;
        Shader *archerShader = nullptr;
        if (ctx.backend) {
          archerShader = ctx.backend->shader(QStringLiteral("archer"));
        }
        Renderer *sceneRenderer = dynamic_cast<Renderer *>(&out);
        if (sceneRenderer && archerShader) {
          sceneRenderer->setCurrentShader(archerShader);
        }
        staticRenderer.render(ctx, out);
        if (sceneRenderer) {
          sceneRenderer->setCurrentShader(nullptr);
        }
      });
}

} 
