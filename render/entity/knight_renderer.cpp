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

static inline float easeInOutCubic(float t) {
  t = clamp01(t);
  return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

static inline float smoothstep(float a, float b, float x) {
  x = clamp01((x - a) / (b - a));
  return x * x * (3.0f - 2.0f * x);
}

static inline float lerp(float a, float b, float t) { return a * (1.0f - t) + b * t; }

static inline QVector3D nlerp(const QVector3D &a, const QVector3D &b, float t) {
  QVector3D v = a * (1.0f - t) + b * t;
  if (v.lengthSquared() > 1e-6f) v.normalize();
  return v;
}

struct KnightExtras {
  QVector3D metalColor;
  QVector3D shieldColor;
  float swordLength = 0.70f;
  float swordWidth = 0.045f;
  float shieldRadius = 0.18f;

  // Internal flavor knobs
  float guardHalfWidth = 0.10f;
  float handleRadius   = 0.018f;
  float pommelRadius   = 0.035f;
  float bladeRicasso   = 0.14f;   // non-tapered segment near guard
  float bladeTaperBias = 0.65f;   // where taper starts (0..1)
  bool  shieldCrossDecal = false; // round-shield cross or ring
  bool  hasScabbard = true;
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
      const float attackCycleTime = 0.6f;
      float attackPhase = std::fmod(anim.time * (1.0f / attackCycleTime), 1.0f);

      // Staged positions with anticipation and follow-through
      QVector3D restPos(0.20f, HP::SHOULDER_Y + 0.05f, 0.15f);
      QVector3D preparePos(0.26f, HP::HEAD_TOP_Y + 0.18f, -0.06f); // higher & slightly back
      QVector3D raisedPos(0.25f, HP::HEAD_TOP_Y + 0.22f, 0.02f);
      QVector3D strikePos(0.30f, HP::HIP_Y - 0.05f, 0.50f);        // lower: top-to-bottom swing
      QVector3D recoverPos(0.22f, HP::SHOULDER_Y + 0.02f, 0.22f);

      if (attackPhase < 0.18f) {
        // Anticipation: lift high
        float t = easeInOutCubic(attackPhase / 0.18f);
        pose.handR = restPos * (1.0f - t) + preparePos * t;
        pose.handL = QVector3D(-0.21f, HP::SHOULDER_Y - 0.02f - 0.03f * t, 0.15f);
      } else if (attackPhase < 0.32f) {
        // Set up above head
        float t = easeInOutCubic((attackPhase - 0.18f) / 0.14f);
        pose.handR = preparePos * (1.0f - t) + raisedPos * t;
        pose.handL = QVector3D(-0.21f, HP::SHOULDER_Y - 0.05f, 0.17f);
      } else if (attackPhase < 0.52f) {
        // Top-to-bottom strike
        float t = (attackPhase - 0.32f) / 0.20f;
        t = t * t * t; // strong acceleration
        pose.handR = raisedPos * (1.0f - t) + strikePos * t;
        pose.handL = QVector3D(-0.21f, HP::SHOULDER_Y - 0.03f * (1.0f - 0.5f * t),
                               0.17f + 0.20f * t);
      } else if (attackPhase < 0.72f) {
        // Follow-through to recover
        float t = easeInOutCubic((attackPhase - 0.52f) / 0.20f);
        pose.handR = strikePos * (1.0f - t) + recoverPos * t;
        pose.handL = QVector3D(-0.20f, HP::SHOULDER_Y - 0.015f * (1.0f - t),
                               lerp(0.37f, 0.20f, t));
      } else {
        // Glide back to rest
        float t = smoothstep(0.72f, 1.0f, attackPhase);
        pose.handR = recoverPos * (1.0f - t) + restPos * t;
        pose.handL = QVector3D(-0.20f - 0.02f * (1.0f - t),
                               HP::SHOULDER_Y + armHeightJitter * (1.0f - t),
                               lerp(0.20f, 0.15f, t));
      }
    } else {
      // Idle stance: sword held more vertically
      pose.handR = QVector3D(0.22f + armAsymmetry, HP::SHOULDER_Y + 0.06f + armHeightJitter, 0.18f);
      pose.handL = QVector3D(-0.22f - 0.5f * armAsymmetry, HP::SHOULDER_Y + 0.5f * armHeightJitter, 0.18f);
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
      attackPhase = std::fmod(anim.time * (1.0f / attackCycleTime), 1.0f);
    }

    drawSword(ctx, pose, v, extras, isAttacking, attackPhase, out);
    drawShield(ctx, pose, v, extras, out);

    // Scabbard on hip when not actively striking
    if (!isAttacking && extras.hasScabbard) {
      drawScabbard(ctx, pose, v, extras, out);
    }
  }

private:
  static KnightExtras computeKnightExtras(uint32_t seed,
                                          const HumanoidVariant &v) {
    KnightExtras e;
    // Subtle cool steel
    e.metalColor = QVector3D(0.72f, 0.73f, 0.78f);

    // Shield base color: cloth/leather bias with seed variation
    float shieldHue = hash01(seed ^ 0x12345u);
    if (shieldHue < 0.45f) {
      e.shieldColor = v.palette.cloth * 1.10f;
    } else if (shieldHue < 0.90f) {
      e.shieldColor = v.palette.leather * 1.25f;
    } else {
      // rare: metal-faced shield
      e.shieldColor = e.metalColor * 0.95f;
    }

    // Weapon & shield dimensions with small individual variance
    e.swordLength   = 0.65f + (hash01(seed ^ 0xABCDu) - 0.5f) * 0.10f;
    e.swordWidth    = 0.043f + (hash01(seed ^ 0x7777u) - 0.5f) * 0.008f;
    e.shieldRadius  = 0.16f + (hash01(seed ^ 0xDEF0u) - 0.5f) * 0.04f;

    e.guardHalfWidth = 0.09f + (hash01(seed ^ 0x3456u) - 0.5f) * 0.025f;
    e.handleRadius   = 0.017f + (hash01(seed ^ 0x88AAu) - 0.5f) * 0.004f;
    e.pommelRadius   = 0.032f + (hash01(seed ^ 0x19C3u) - 0.5f) * 0.006f;

    e.bladeRicasso   = clampf(0.12f + (hash01(seed ^ 0xBEEFu) - 0.5f) * 0.06f, 0.08f, 0.18f);
    e.bladeTaperBias = clamp01(0.6f + (hash01(seed ^ 0xFACEu) - 0.5f) * 0.2f);

    e.shieldCrossDecal = (hash01(seed ^ 0xA11Cu) > 0.55f);
    e.hasScabbard      = (hash01(seed ^ 0x5CABu) > 0.15f);
    return e;
  }

  static void drawSword(const DrawContext &ctx, const HumanoidPose &pose,
                        const HumanoidVariant &v, const KnightExtras &extras,
                        bool isAttacking, float attackPhase, ISubmitter &out) {
    QVector3D gripPos = pose.handR;

    // Desired orientation: more vertical overall. During attack, top-to-bottom arc.
    QVector3D upish(0.05f, 1.0f, 0.15f);   if (upish.lengthSquared()>1e-6f) upish.normalize();
    QVector3D midish(0.08f, 0.20f, 1.0f);  if (midish.lengthSquared()>1e-6f) midish.normalize();
    QVector3D downish(0.10f,-1.0f, 0.25f); if (downish.lengthSquared()>1e-6f) downish.normalize();

    QVector3D swordDir = upish; // default idle: vertical with slight forward

    if (isAttacking) {
      if (attackPhase < 0.18f) {
        // Keep blade vertical while lifting
        float t = easeInOutCubic(attackPhase / 0.18f);
        swordDir = nlerp(upish, upish, t);
      } else if (attackPhase < 0.32f) {
        // Slight pre-rotation forward but still mostly up
        float t = easeInOutCubic((attackPhase - 0.18f) / 0.14f);
        swordDir = nlerp(upish, midish, t * 0.35f);
      } else if (attackPhase < 0.52f) {
        // Main cut: top -> bottom, curved via midish
        float t = (attackPhase - 0.32f) / 0.20f; // 0..1
        t = t * t * t; // accelerate
        if (t < 0.5f) {
          float u = t / 0.5f;                 // 0..1
          swordDir = nlerp(upish, midish, u); // first half of the curve
        } else {
          float u = (t - 0.5f) / 0.5f;        // 0..1
          swordDir = nlerp(midish, downish, u); // second half to downward
        }
      } else if (attackPhase < 0.72f) {
        // Recover: bottom -> mid
        float t = easeInOutCubic((attackPhase - 0.52f) / 0.20f);
        swordDir = nlerp(downish, midish, t);
      } else {
        // Settle back to vertical idle
        float t = smoothstep(0.72f, 1.0f, attackPhase);
        swordDir = nlerp(midish, upish, t);
      }
    }

    QVector3D handleEnd  = gripPos - swordDir * 0.10f;
    QVector3D bladeBase  = gripPos;
    QVector3D bladeTip   = gripPos + swordDir * extras.swordLength;

    // Handle (rounded cylinder)
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, handleEnd, bladeBase, extras.handleRadius),
             v.palette.leather, nullptr, 1.0f);

    // Crossguard
    QVector3D guardCenter = bladeBase;
    float gw = extras.guardHalfWidth;
    QVector3D guardL = guardCenter + QVector3D(-gw, 0.0f, 0.0f);
    QVector3D guardR = guardCenter + QVector3D( gw, 0.0f, 0.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, guardL, guardR, 0.014f),
             extras.metalColor, nullptr, 1.0f);
    // Guard end caps
    QMatrix4x4 gl = ctx.model; gl.translate(guardL); gl.scale(0.018f);
    out.mesh(getUnitSphere(), gl, extras.metalColor, nullptr, 1.0f);
    QMatrix4x4 gr = ctx.model; gr.translate(guardR); gr.scale(0.018f);
    out.mesh(getUnitSphere(), gr, extras.metalColor, nullptr, 1.0f);

    // Blade: ricasso (cyl) + tapered cone to tip
    float L = extras.swordLength;
    float ricassoLen = clampf(extras.bladeRicasso, 0.06f, L * 0.35f);
    QVector3D ricassoEnd = bladeBase + swordDir * ricassoLen;
    float baseW = extras.swordWidth;
    float midW  = baseW * 0.75f;

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, bladeBase, ricassoEnd, baseW),
             extras.metalColor, nullptr, 1.0f);

    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, ricassoEnd, bladeTip, midW),
             extras.metalColor, nullptr, 1.0f);

    // Pommel
    QVector3D pommel = handleEnd - swordDir * 0.02f;
    QMatrix4x4 pommelMat = ctx.model;
    pommelMat.translate(pommel);
    pommelMat.scale(extras.pommelRadius);
    out.mesh(getUnitSphere(), pommelMat, extras.metalColor, nullptr, 1.0f);

    // Motion trail hint during fastest swing (still vertical-ish plane)
    if (isAttacking && attackPhase >= 0.32f && attackPhase < 0.56f) {
      float t = (attackPhase - 0.32f) / 0.24f;
      float alpha = 0.35f * (1.0f - t);
      QVector3D trailStart = bladeBase - swordDir * 0.05f;               // apex (point)
      QVector3D trailEnd   = bladeBase - swordDir * (0.28f + 0.15f * t); // base
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, trailEnd, trailStart, baseW * 0.9f),
               extras.metalColor * 0.9f, nullptr, clamp01(alpha));
    }
  }

  static void drawShieldDecal(const DrawContext &ctx,
                              const QVector3D &center,
                              float radius,
                              const QVector3D & /*baseColor*/,
                              const HumanoidVariant &v,
                              ISubmitter &out) {
    // Two optional styles: cross; color keyed to team cloth
    QVector3D accent = v.palette.cloth * 1.2f;
    float barR = radius * 0.10f;

    // Vertical bar
    QVector3D top = center + QVector3D(0.0f, radius * 0.95f, 0.0f);
    QVector3D bot = center - QVector3D(0.0f, radius * 0.95f, 0.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, top, bot, barR),
             accent, nullptr, 1.0f);

    // Horizontal bar
    QVector3D left  = center + QVector3D(-radius * 0.95f, 0.0f, 0.0f);
    QVector3D right = center + QVector3D( radius * 0.95f, 0.0f, 0.0f);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, left, right, barR),
             accent, nullptr, 1.0f);
  }

  static void drawShieldRing(const DrawContext &ctx,
                             const QVector3D &center,
                             float radius,
                             float thickness,
                             const QVector3D &color,
                             ISubmitter &out) {
    // Approximate ring with segmented cylinders
    const int segments = 12;
    for (int i = 0; i < segments; ++i) {
      float a0 = (float)i / segments * 2.0f * 3.14159265f;
      float a1 = (float)(i + 1) / segments * 2.0f * 3.14159265f;
      QVector3D p0(center.x() + radius * std::cos(a0), center.y() + radius * std::sin(a0), center.z());
      QVector3D p1(center.x() + radius * std::cos(a1), center.y() + radius * std::sin(a1), center.z());
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, p0, p1, thickness), color, nullptr, 1.0f);
    }
  }

  static void drawShield(const DrawContext &ctx, const HumanoidPose &pose,
                         const HumanoidVariant &v, const KnightExtras &extras,
                         ISubmitter &out) {
    // Slight tilt to face incoming blows
    QVector3D shieldCenter = pose.handL + QVector3D(0.0f, -0.05f, 0.05f);
    float tiltDeg = 12.0f;

    float shieldHalfDepth = 0.035f;
    QVector3D localZ(0.0f, 0.0f, 1.0f);

    // Front & back discs with tilt
    QMatrix4x4 frontMat = ctx.model;
    frontMat.translate(shieldCenter + localZ * shieldHalfDepth);
    frontMat.rotate(tiltDeg, 1.0f, 0.0f, 0.0f);
    frontMat.scale(extras.shieldRadius, extras.shieldRadius, 0.010f);
    out.mesh(getUnitCylinder(), frontMat, extras.shieldColor, nullptr, 1.0f);

    QMatrix4x4 backMat = ctx.model;
    backMat.translate(shieldCenter - localZ * shieldHalfDepth);
    backMat.rotate(tiltDeg, 1.0f, 0.0f, 0.0f);
    backMat.scale(extras.shieldRadius * 0.98f, extras.shieldRadius * 0.98f, 0.010f);
    out.mesh(getUnitCylinder(), backMat, v.palette.leather * 0.8f, nullptr, 1.0f);

    // Domed suggestion: shallow cone from center outward (single-radius cone)
    QVector3D domeA = shieldCenter + QVector3D(0.0f, 0.0f, 0.012f);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model,
                        domeA,                                                 // base center (front)
                        shieldCenter + QVector3D(0.0f, 0.0f, -0.002f),        // apex (toward back)
                        extras.shieldRadius * 0.96f),                          // base radius
             extras.shieldColor * 0.98f, nullptr, 1.0f);

    // Thick metal rim (segmented circle)
    drawShieldRing(ctx, shieldCenter, extras.shieldRadius, 0.012f, (extras.metalColor * 0.95f), out);

    // Decorative inner ring
    drawShieldRing(ctx, shieldCenter, extras.shieldRadius * 0.70f, 0.008f, v.palette.leather * 0.9f, out);

    // Boss
    QMatrix4x4 bossMat = ctx.model;
    bossMat.translate(shieldCenter + QVector3D(0.0f, 0.0f, 0.02f));
    bossMat.scale(0.045f);
    out.mesh(getUnitSphere(), bossMat, extras.metalColor, nullptr, 1.0f);

    // Straps/handle (connect hand to shield back)
    QVector3D gripA = shieldCenter - QVector3D(0.03f, 0.00f, 0.03f);
    QVector3D gripB = shieldCenter + QVector3D(0.03f, 0.00f, -0.03f);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, gripA, gripB, 0.010f), v.palette.leather, nullptr, 1.0f);

    // Optional heraldic cross on shield front
    if (extras.shieldCrossDecal && (extras.shieldColor != extras.metalColor)) {
      drawShieldDecal(ctx, shieldCenter + QVector3D(0.0f, 0.0f, 0.011f), extras.shieldRadius * 0.85f,
                      extras.shieldColor, v, out);
    }
  }

  static void drawScabbard(const DrawContext &ctx, const HumanoidPose & /*pose*/,
                           const HumanoidVariant &v, const KnightExtras &extras,
                           ISubmitter &out) {
    using HP = HumanProportions;

    // Hang on left hip, angled back
    QVector3D hip(0.10f, HP::WAIST_Y - 0.04f, -0.02f);
    QVector3D tip = hip + QVector3D(-0.05f, -0.22f, -0.12f);
    float sheathR = extras.swordWidth * 0.85f;

    // Sheath body
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, hip, tip, sheathR), v.palette.leather * 0.9f, nullptr, 1.0f);

    // Sheath tip ferrule (single-radius cone)
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model,
                        tip,                                                  // base
                        tip + QVector3D(-0.02f, -0.02f, -0.02f),             // apex
                        sheathR),                                             // base radius
             extras.metalColor, nullptr, 1.0f);

    // Straps to belt
    QVector3D strapA = hip + QVector3D(0.00f, 0.03f, 0.00f);
    QVector3D belt   = QVector3D(0.12f, HP::WAIST_Y + 0.01f, 0.02f);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, strapA, belt, 0.006f), v.palette.leather, nullptr, 1.0f);
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
