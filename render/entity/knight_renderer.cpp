#include "knight_renderer.h"
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

struct KnightExtras {
  QVector3D metalColor;
  QVector3D shieldColor;
  float swordLength = 0.70f;
  float swordWidth = 0.045f;
  float shieldRadius = 0.18f;
};

class KnightRenderer : public HumanoidRendererBase {
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
      float attackCycleTime = 0.6f;
      float attackPhase = fmod(anim.time * (1.0f / attackCycleTime), 1.0f);

      QVector3D restPos(0.20f, HP::SHOULDER_Y + 0.05f, 0.15f);
      QVector3D raisedPos(0.25f, HP::HEAD_TOP_Y + 0.15f, 0.05f);
      QVector3D strikePos(0.35f, HP::WAIST_Y + 0.10f, 0.50f);

      if (attackPhase < 0.25f) {
        float t = attackPhase / 0.25f;
        t = t * t;
        pose.handR = restPos * (1.0f - t) + raisedPos * t;
        pose.handL = QVector3D(-0.20f, HP::SHOULDER_Y - 0.05f * t, 0.15f);
      } else if (attackPhase < 0.35f) {
        pose.handR = raisedPos;
        pose.handL = QVector3D(-0.20f, HP::SHOULDER_Y - 0.05f, 0.15f);
      } else if (attackPhase < 0.55f) {
        float t = (attackPhase - 0.35f) / 0.2f;
        t = t * t * t;
        pose.handR = raisedPos * (1.0f - t) + strikePos * t;
        pose.handL =
            QVector3D(-0.20f, HP::SHOULDER_Y - 0.05f * (1.0f - t * 0.5f),
                      0.15f + 0.15f * t);
      } else {
        float t = (attackPhase - 0.55f) / 0.45f;
        t = 1.0f - (1.0f - t) * (1.0f - t);
        pose.handR = strikePos * (1.0f - t) + restPos * t;
        pose.handL = QVector3D(-0.20f, HP::SHOULDER_Y - 0.025f * (1.0f - t),
                               0.30f * (1.0f - t) + 0.15f * t);
      }
    } else {
      pose.handR = QVector3D(0.20f + armAsymmetry, HP::SHOULDER_Y + armHeightJitter, 0.20f);
      pose.handL = QVector3D(-0.20f - armAsymmetry, HP::SHOULDER_Y + armHeightJitter, 0.15f);
    }
  }

  void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose, ISubmitter &out) const override {
    uint32_t seed = reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFu;
    KnightExtras extras = computeKnightExtras(seed, v);

    AnimationInputs anim = sampleAnimState(ctx);
    bool isAttacking = anim.isAttacking && anim.isMelee;
    float attackPhase = 0.0f;
    if (isAttacking) {
      float attackCycleTime = 0.6f;
      attackPhase = fmod(anim.time * (1.0f / attackCycleTime), 1.0f);
    }

    drawSword(ctx, pose, v, extras, isAttacking, attackPhase, out);
    drawShield(ctx, pose, v, extras, out);
  }

private:
  static KnightExtras computeKnightExtras(uint32_t seed,
                                          const HumanoidVariant &v) {
    KnightExtras e;
    e.metalColor = QVector3D(0.70f, 0.70f, 0.75f);
    
    float shieldHue = hash01(seed ^ 0x12345u);
    if (shieldHue < 0.5f) {
      e.shieldColor = v.palette.cloth * 1.1f;
    } else {
      e.shieldColor = v.palette.leather * 1.3f;
    }
    
    e.swordLength = 0.65f + (hash01(seed ^ 0xABCDu) - 0.5f) * 0.10f;
    e.shieldRadius = 0.16f + (hash01(seed ^ 0xDEF0u) - 0.5f) * 0.04f;
    
    return e;
  }

  static void drawSword(const DrawContext &ctx, const HumanoidPose &pose,
                        const HumanoidVariant &v, const KnightExtras &extras,
                        bool isAttacking, float attackPhase, ISubmitter &out) {
    const QVector3D forward(0.0f, 0.0f, 1.0f);
    const QVector3D up(0.0f, 1.0f, 0.0f);

    QVector3D gripPos = pose.handR;
    
    QVector3D swordDir = forward;
    if (isAttacking && attackPhase >= 0.35f && attackPhase < 0.55f) {
      float swingT = (attackPhase - 0.35f) / 0.2f;
      float angle = swingT * 3.14159f * 0.6f;
      swordDir = QVector3D(sin(angle) * 0.5f, -cos(angle) * 0.3f, cos(angle));
      swordDir.normalize();
    }

    QVector3D handleEnd = gripPos - swordDir * 0.10f;
    QVector3D bladeStart = gripPos;
    QVector3D bladeEnd = gripPos + swordDir * extras.swordLength;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, handleEnd, bladeStart, 0.025f),
             v.palette.leather, nullptr, 1.0f);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, bladeStart, bladeEnd, extras.swordWidth),
             extras.metalColor, nullptr, 1.0f);

    QVector3D guardCenter = bladeStart;
    QVector3D guardLeft = guardCenter + QVector3D(-0.08f, 0.0f, 0.0f);
    QVector3D guardRight = guardCenter + QVector3D(0.08f, 0.0f, 0.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, guardLeft, guardRight, 0.015f),
             extras.metalColor, nullptr, 1.0f);

    QVector3D pommel = handleEnd - swordDir * 0.02f;
    QMatrix4x4 pommelMat = ctx.model;
    pommelMat.translate(pommel);
    pommelMat.scale(0.035f);
    out.mesh(getUnitSphere(), pommelMat, extras.metalColor, nullptr, 1.0f);
  }

  static void drawShield(const DrawContext &ctx, const HumanoidPose &pose,
                         const HumanoidVariant &v, const KnightExtras &extras,
                         ISubmitter &out) {
    using HP = HumanProportions;

    QVector3D shieldCenter = pose.handL + QVector3D(0.0f, -0.05f, 0.05f);
    
    float shieldThickness = 0.04f;
    QVector3D shieldFront = shieldCenter + QVector3D(0.0f, 0.0f, shieldThickness);
    QVector3D shieldBack = shieldCenter - QVector3D(0.0f, 0.0f, shieldThickness);

    QMatrix4x4 frontMat = ctx.model;
    frontMat.translate(shieldFront);
    frontMat.scale(extras.shieldRadius, extras.shieldRadius, 0.01f);
    out.mesh(getUnitCylinder(), frontMat, extras.shieldColor, nullptr, 1.0f);

    QMatrix4x4 backMat = ctx.model;
    backMat.translate(shieldBack);
    backMat.scale(extras.shieldRadius, extras.shieldRadius, 0.01f);
    out.mesh(getUnitCylinder(), backMat, v.palette.leather * 0.8f, nullptr, 1.0f);

    QVector3D rimTop = shieldCenter + QVector3D(0.0f, extras.shieldRadius, 0.0f);
    QVector3D rimBottom = shieldCenter - QVector3D(0.0f, extras.shieldRadius, 0.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, rimTop, rimBottom, 0.012f),
             extras.metalColor, nullptr, 1.0f);

    QVector3D rimLeft = shieldCenter + QVector3D(-extras.shieldRadius, 0.0f, 0.0f);
    QVector3D rimRight = shieldCenter + QVector3D(extras.shieldRadius, 0.0f, 0.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, rimLeft, rimRight, 0.012f),
             extras.metalColor, nullptr, 1.0f);

    QMatrix4x4 bossMat = ctx.model;
    bossMat.translate(shieldFront + QVector3D(0.0f, 0.0f, 0.02f));
    bossMat.scale(0.04f);
    out.mesh(getUnitSphere(), bossMat, extras.metalColor, nullptr, 1.0f);
  }
};

void registerKnightRenderer(Render::GL::EntityRendererRegistry &registry) {
  static KnightRenderer renderer;
  registry.registerRenderer("knight",
                            [](const DrawContext &ctx, ISubmitter &out) {
                              static KnightRenderer staticRenderer;
                              staticRenderer.render(ctx, out);
                            });
}

} // namespace Render::GL
