#include "archer_renderer.h"
#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../geom/math_utils.h"
#include "../geom/transforms.h"
#include "../gl/mesh.h"
#include "../gl/primitives.h"
#include "../humanoid_base.h"
#include "../humanoid_math.h"
#include "../humanoid_specs.h"
#include "../palette.h"
#include "registry.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Render::GL {

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;

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
    pose.handL = QVector3D(bowX - 0.05f + armAsymmetry,
                           HP::SHOULDER_Y + 0.05f + armHeightJitter, 0.55f);
    pose.handR = QVector3D(0.15f - armAsymmetry * 0.5f,
                           HP::SHOULDER_Y + 0.15f + armHeightJitter * 0.8f, 0.20f);

    if (anim.isAttacking) {
      float attackCycleTime = 1.2f;
      float attackPhase = fmod(anim.time * (1.0f / attackCycleTime), 1.0f);

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
          pose.handL =
              QVector3D(-0.15f, HP::SHOULDER_Y - 0.05f * (1.0f - t),
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
                      const HumanoidPose &pose, ISubmitter &out) const override {
    using HP = HumanProportions;

    QVector3D teamTint = resolveTeamTint(ctx);
    uint32_t seed = 0u;
    if (ctx.entity) {
      auto *unit = ctx.entity->getComponent<Engine::Core::UnitComponent>();
      if (unit)
        seed ^= uint32_t(unit->ownerId * 2654435761u);
      seed ^=
          uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFu);
    }

    ArcherExtras extras;
    extras.metalHead = Render::Geom::clampVec01(v.palette.metal * 1.15f);
    extras.stringCol = QVector3D(0.30f, 0.30f, 0.32f);
    auto tint = [&](float k) {
      return QVector3D(clamp01(teamTint.x() * k), clamp01(teamTint.y() * k),
                       clamp01(teamTint.z() * k));
    };
    extras.fletch = tint(0.9f);
    extras.bowTopY = HP::SHOULDER_Y + 0.55f;
    extras.bowBotY = HP::WAIST_Y - 0.25f;

    drawQuiver(ctx, v, extras, seed, out);

    AnimationInputs anim = sampleAnimState(ctx);
    float attackPhase = 0.0f;
    if (anim.isAttacking && !anim.isMelee) {
      float attackCycleTime = 1.2f;
      attackPhase = fmod(anim.time * (1.0f / attackCycleTime), 1.0f);
    }
    drawBowAndArrow(ctx, pose, v, extras, anim.isAttacking && !anim.isMelee,
                    attackPhase, out);
  }

private:
  static void drawQuiver(const DrawContext &ctx, const HumanoidVariant &v,
                         const ArcherExtras &extras, uint32_t seed,
                         ISubmitter &out) {
    using HP = HumanProportions;

    QVector3D qTop(-0.08f, HP::SHOULDER_Y + 0.10f, -0.25f);
    QVector3D qBase(-0.10f, HP::CHEST_Y, -0.22f);

    float quiverR = HP::HEAD_RADIUS * 0.45f;
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, qBase, qTop, quiverR),
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
    QVector3D topEnd(extras.bowX, extras.bowTopY, grip.z());
    QVector3D botEnd(extras.bowX, extras.bowBotY, grip.z());

    QVector3D nock(
        extras.bowX,
        clampf(pose.handR.y(), extras.bowBotY + 0.05f, extras.bowTopY - 0.05f),
        clampf(pose.handR.z(), grip.z() - 0.30f, grip.z() + 0.30f));

    const int segs = 22;
    auto qBezier = [](const QVector3D &a, const QVector3D &c,
                      const QVector3D &b, float t) {
      float u = 1.0f - t;
      return u * u * a + 2.0f * u * t * c + t * t * b;
    };
    QVector3D ctrl = nock + forward * extras.bowDepth;
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

    bool showArrow =
        !isAttacking || (isAttacking && attackPhase >= 0.0f && attackPhase < 0.52f);

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
  registry.registerRenderer("archer", [](const DrawContext &ctx,
                                         ISubmitter &out) {
    static ArcherRenderer staticRenderer;
    staticRenderer.render(ctx, out);
  });
}

} // namespace Render::GL
