#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../geom/math_utils.h"
#include "../geom/selection_ring.h"
#include "../geom/transforms.h"
#include "../gl/mesh.h"
#include "../gl/primitives.h"
#include "../gl/texture.h"
#include "archer_renderer.h"
#include "registry.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

namespace Render::GL {

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::clampVec01;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;

struct HumanProportions {

  static constexpr float TOTAL_HEIGHT = 2.00F;
  static constexpr float HEAD_HEIGHT = 0.25F;

  static constexpr float GROUND_Y = -1.00F;
  static constexpr float HEAD_TOP_Y = GROUND_Y + TOTAL_HEIGHT;
  static constexpr float CHIN_Y = HEAD_TOP_Y - HEAD_HEIGHT;
  static constexpr float NECK_BASE_Y = CHIN_Y - 0.10F;
  static constexpr float SHOULDER_Y = NECK_BASE_Y - 0.15F;
  static constexpr float CHEST_Y = SHOULDER_Y - 0.35F;
  static constexpr float WAIST_Y = CHEST_Y - 0.30F;
  static constexpr float HIP_Y = WAIST_Y - 0.15F;
  static constexpr float KNEE_Y = HIP_Y - 0.35F;

  static constexpr float SHOULDER_WIDTH = HEAD_HEIGHT * 1.6F;
  static constexpr float HEAD_RADIUS = HEAD_HEIGHT * 0.40F;
  static constexpr float NECK_RADIUS = HEAD_RADIUS * 0.35F;
  static constexpr float TORSO_TOP_R = HEAD_RADIUS * 1.0F;
  static constexpr float TORSO_BOT_R = HEAD_RADIUS * 0.9F;
  static constexpr float UPPER_ARM_R = HEAD_RADIUS * 0.30F;
  static constexpr float FORE_ARM_R = HEAD_RADIUS * 0.25F;
  static constexpr float HAND_RADIUS = HEAD_RADIUS * 0.22F;
  static constexpr float UPPER_LEG_R = HEAD_RADIUS * 0.38F;
  static constexpr float LOWER_LEG_R = HEAD_RADIUS * 0.32F;

  static constexpr float UPPER_ARM_LEN = 0.28F;
  static constexpr float FORE_ARM_LEN = 0.30F;
  static constexpr float UPPER_LEG_LEN = 0.35F;
  static constexpr float LOWER_LEG_LEN = 0.35F;
};

struct ArcherColors {
  QVector3D tunic, skin, leather, leatherDark, wood, metal, metalHead,
      stringCol, fletch;
};

struct ArcherPose {
  using P = HumanProportions;

  QVector3D headPos{0.0F, (P::HEAD_TOP_Y + P::CHIN_Y) * 0.5F, 0.0F};
  float headR = P::HEAD_RADIUS;
  QVector3D neck_base{0.0F, P::NECK_BASE_Y, 0.0F};

  QVector3D shoulderL{-P::SHOULDER_WIDTH * 0.5F, P::SHOULDER_Y, 0.1F};
  QVector3D shoulderR{P::SHOULDER_WIDTH * 0.5F, P::SHOULDER_Y, 0.1F};

  QVector3D elbowL, elbowR;
  QVector3D handL, hand_r;

  float hipSpacing = P::SHOULDER_WIDTH * 0.55F;
  QVector3D hipL{-hipSpacing, P::HIP_Y, 0.03F};
  QVector3D hipR{hipSpacing, P::HIP_Y, -0.03F};

  QVector3D footL{-hipSpacing * 1.6F, P::GROUND_Y, 0.10F};
  QVector3D foot_r{hipSpacing * 1.6F, P::GROUND_Y, -0.10F};

  float bowX = 0.0F;
  float bowTopY = P::SHOULDER_Y + 0.55F;
  float bowBotY = P::HIP_Y - 0.25F;
  float bowRodR = 0.035F;
  float stringR = 0.008F;
  float bowDepth = 0.25F;
};

static inline ArcherPose makePose(uint32_t seed) {
  (void)seed;
  ArcherPose P;

  using HP = HumanProportions;

  P.handL = QVector3D(P.bowX - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

  P.hand_r = QVector3D(0.15F, HP::SHOULDER_Y + 0.15F, 0.20F);

  QVector3D shoulder_to_hand_l = P.handL - P.shoulderL;
  float distL = shoulder_to_hand_l.length();
  QVector3D dirL = shoulder_to_hand_l.normalized();

  QVector3D perpL(-dirL.z(), 0.0F, dirL.x());
  float elbowOffsetL = 0.15F;
  P.elbowL = P.shoulderL + dirL * (distL * 0.45F) + perpL * elbowOffsetL +
             QVector3D(0, -0.08F, 0);

  QVector3D shoulder_to_hand_r = P.hand_r - P.shoulderR;
  float distR = shoulder_to_hand_r.length();
  QVector3D dirR = shoulder_to_hand_r.normalized();

  QVector3D perpR(-dirR.z(), 0.0F, dirR.x());
  float elbowOffsetR = 0.12F;
  P.elbowR = P.shoulderR + dirR * (distR * 0.48F) + perpR * elbowOffsetR +
             QVector3D(0, 0.02F, 0);

  return P;
}

static inline ArcherColors makeColors(const QVector3D &team_tint) {
  ArcherColors C;
  auto tint = [&](float k) {
    return QVector3D(clamp01(team_tint.x() * k), clamp01(team_tint.y() * k),
                     clamp01(team_tint.z() * k));
  };
  C.tunic = team_tint;
  C.skin = QVector3D(0.96F, 0.80F, 0.69F);
  C.leather = QVector3D(0.35F, 0.22F, 0.12F);
  C.leatherDark = C.leather * 0.9F;
  C.wood = QVector3D(0.16F, 0.10F, 0.05F);
  C.metal = QVector3D(0.65F, 0.66F, 0.70F);
  C.metalHead = clampVec01(C.metal * 1.1F);
  C.stringCol = QVector3D(0.30F, 0.30F, 0.32F);
  C.fletch = tint(0.9F);
  return C;
}

static inline void drawTorso(const DrawContext &p, ISubmitter &out,
                             const ArcherColors &C, const ArcherPose &P) {
  using HP = HumanProportions;

  QVector3D torso_top{0.0F, HP::NECK_BASE_Y - 0.05F, 0.0F};
  QVector3D torsoBot{0.0F, HP::WAIST_Y, 0.0F};

  float torso_radius = HP::TORSO_TOP_R;

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, torso_top, torsoBot, torso_radius), C.tunic,
           nullptr, 1.0F);

  QVector3D waist{0.0F, HP::WAIST_Y, 0.0F};
  QVector3D hipCenter = (P.hipL + P.hipR) * 0.5F;

  out.mesh(getUnitCone(),
           coneFromTo(p.model, waist, hipCenter, HP::TORSO_BOT_R),
           C.tunic * 0.9F, nullptr, 1.0F);
}

static inline void drawHeadAndNeck(const DrawContext &p, ISubmitter &out,
                                   const ArcherPose &P, const ArcherColors &C) {
  using HP = HumanProportions;

  QVector3D chin_pos{0.0F, HP::CHIN_Y, 0.0F};
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.neck_base, chin_pos, HP::NECK_RADIUS),
           C.skin * 0.9F, nullptr, 1.0F);

  out.mesh(getUnitSphere(), sphereAt(p.model, P.headPos, P.headR), C.skin,
           nullptr, 1.0F);

  QVector3D iris(0.06F, 0.06F, 0.07F);
  float eyeZ = P.headR * 0.7F;
  float eyeY = P.headPos.y() + P.headR * 0.1F;
  float eye_spacing = P.headR * 0.35F;
  out.mesh(getUnitSphere(),
           p.model *
               sphereAt(QVector3D(-eye_spacing, eyeY, eyeZ), P.headR * 0.15F),
           iris, nullptr, 1.0F);
  out.mesh(getUnitSphere(),
           p.model *
               sphereAt(QVector3D(eye_spacing, eyeY, eyeZ), P.headR * 0.15F),
           iris, nullptr, 1.0F);

  QVector3D domeC = P.headPos + QVector3D(0.0F, P.headR * 0.25F, 0.0F);
  float domeR = P.headR * 1.05F;
  out.mesh(getUnitSphere(), sphereAt(p.model, domeC, domeR), C.metal, nullptr,
           1.0F);

  QVector3D visorBase(0.0F, P.headPos.y() + P.headR * 0.10F, P.headR * 0.80F);
  QVector3D visorTip = visorBase + QVector3D(0.0F, -0.015F, 0.06F);
  out.mesh(getUnitCone(),
           coneFromTo(p.model, visorBase, visorTip, P.headR * 0.38F),
           C.metal * 0.92F, nullptr, 1.0F);

  QVector3D cheekL0(-P.headR * 0.85F, P.headPos.y() + P.headR * 0.05F, 0.02F);
  QVector3D cheekL1(-P.headR * 0.85F, P.headPos.y() - P.headR * 0.20F, 0.04F);
  QVector3D cheekR0(P.headR * 0.85F, P.headPos.y() + P.headR * 0.05F, -0.02F);
  QVector3D cheekR1(P.headR * 0.85F, P.headPos.y() - P.headR * 0.20F, -0.04F);
  out.mesh(getUnitCone(),
           coneFromTo(p.model, cheekL0, cheekL1, P.headR * 0.24F),
           C.metal * 0.95F, nullptr, 1.0F);
  out.mesh(getUnitCone(),
           coneFromTo(p.model, cheekR0, cheekR1, P.headR * 0.24F),
           C.metal * 0.95F, nullptr, 1.0F);
}

static inline void draw_arms(const DrawContext &p, ISubmitter &out,
                             const ArcherPose &P, const ArcherColors &C) {
  using HP = HumanProportions;

  const float upper_arm_r = HP::UPPER_ARM_R;
  const float fore_arm_r = HP::FORE_ARM_R;
  const float joint_r = upper_arm_r * 1.2F;

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.shoulderL, P.elbowL, upper_arm_r),
           C.tunic, nullptr, 1.0F);

  out.mesh(getUnitSphere(), sphereAt(p.model, P.elbowL, joint_r),
           C.tunic * 0.95F, nullptr, 1.0F);

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.elbowL, P.handL, fore_arm_r),
           C.skin * 0.95F, nullptr, 1.0F);

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.shoulderR, P.elbowR, upper_arm_r),
           C.tunic, nullptr, 1.0F);

  out.mesh(getUnitSphere(), sphereAt(p.model, P.elbowR, joint_r),
           C.tunic * 0.95F, nullptr, 1.0F);

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.elbowR, P.hand_r, fore_arm_r),
           C.skin * 0.95F, nullptr, 1.0F);
}

static inline void draw_legs(const DrawContext &p, ISubmitter &out,
                             const ArcherPose &P, const ArcherColors &C) {
  using HP = HumanProportions;

  const float thighR = HP::UPPER_LEG_R;
  const float shinR = HP::LOWER_LEG_R;
  const float kneeJointR = thighR * 1.15F;

  auto makeKnee = [&](const QVector3D &hip, const QVector3D &foot,
                      float outwardSign) {
    const float t = 0.38F;
    QVector3D knee = hip * (1.0F - t) + foot * t;
    knee.setY(HP::KNEE_Y + 0.03F);
    knee.setZ(knee.z() + 0.05F);
    knee.setX(knee.x() + outwardSign * 0.06F);
    return knee;
  };

  QVector3D knee_l = makeKnee(P.hipL, P.footL, -1.0F);
  QVector3D knee_r = makeKnee(P.hipR, P.foot_r, 1.0F);

  out.mesh(getUnitCone(), coneFromTo(p.model, P.hipL, knee_l, thighR),
           C.leather, nullptr, 1.0F);
  out.mesh(getUnitCone(), coneFromTo(p.model, P.hipR, knee_r, thighR),
           C.leather, nullptr, 1.0F);

  out.mesh(getUnitSphere(), sphereAt(p.model, knee_l, kneeJointR),
           C.leather * 0.95F, nullptr, 1.0F);
  out.mesh(getUnitSphere(), sphereAt(p.model, knee_r, kneeJointR),
           C.leather * 0.95F, nullptr, 1.0F);

  out.mesh(getUnitCone(), coneFromTo(p.model, knee_l, P.footL, shinR),
           C.leatherDark, nullptr, 1.0F);
  out.mesh(getUnitCone(), coneFromTo(p.model, knee_r, P.foot_r, shinR),
           C.leatherDark, nullptr, 1.0F);

  QVector3D down(0.0F, -0.02F, 0.0F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.footL, P.footL + down, shinR * 1.1F),
           C.leatherDark, nullptr, 1.0F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.foot_r, P.foot_r + down, shinR * 1.1F),
           C.leatherDark, nullptr, 1.0F);
}

static inline void drawQuiver(const DrawContext &p, ISubmitter &out,
                              const ArcherColors &C, const ArcherPose &P,
                              uint32_t seed) {
  using HP = HumanProportions;

  auto hash01 = [](uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (x & 0x00FFFFFF) / float(0x01000000);
  };

  QVector3D qTop(-0.08F, HP::SHOULDER_Y + 0.10F, -0.25F);
  QVector3D q_base(-0.10F, HP::CHEST_Y, -0.22F);

  float quiver_r = HP::HEAD_RADIUS * 0.45F;
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, q_base, qTop, quiver_r),
           C.leather, nullptr, 1.0F);

  float j = (hash01(seed) - 0.5F) * 0.04F;
  float k = (hash01(seed ^ 0x9E3779B9u) - 0.5F) * 0.04F;

  QVector3D a1 = qTop + QVector3D(0.00F + j, 0.08F, 0.00F + k);
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, qTop, a1, 0.010F),
           C.wood, nullptr, 1.0F);
  out.mesh(getUnitCone(),
           coneFromTo(p.model, a1, a1 + QVector3D(0, 0.05F, 0), 0.025F),
           C.fletch, nullptr, 1.0F);

  QVector3D a2 = qTop + QVector3D(0.02F - j, 0.07F, 0.02F - k);
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, qTop, a2, 0.010F),
           C.wood, nullptr, 1.0F);
  out.mesh(getUnitCone(),
           coneFromTo(p.model, a2, a2 + QVector3D(0, 0.05F, 0), 0.025F),
           C.fletch, nullptr, 1.0F);
}

static inline void drawBowAndArrow(const DrawContext &p, ISubmitter &out,
                                   const ArcherPose &P, const ArcherColors &C) {
  const QVector3D up(0.0F, 1.0F, 0.0F);
  const QVector3D forward(0.0F, 0.0F, 1.0F);

  QVector3D grip = P.handL;
  QVector3D top_end(P.bowX, P.bowTopY, grip.z());
  QVector3D bot_end(P.bowX, P.bowBotY, grip.z());

  QVector3D nock(P.bowX,
                 clampf(P.hand_r.y(), P.bowBotY + 0.05F, P.bowTopY - 0.05F),
                 clampf(P.hand_r.z(), grip.z() - 0.30F, grip.z() + 0.30F));

  const int segs = 22;
  auto q_bezier = [](const QVector3D &a, const QVector3D &c, const QVector3D &b,
                     float t) {
    float u = 1.0F - t;
    return u * u * a + 2.0F * u * t * c + t * t * b;
  };
  QVector3D ctrl = nock + forward * P.bowDepth;
  QVector3D prev = bot_end;
  for (int i = 1; i <= segs; ++i) {
    float t = float(i) / float(segs);
    QVector3D cur = q_bezier(bot_end, ctrl, top_end, t);
    out.mesh(getUnitCylinder(), cylinderBetween(p.model, prev, cur, P.bowRodR),
             C.wood, nullptr, 1.0F);
    prev = cur;
  }
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, grip - up * 0.05F, grip + up * 0.05F,
                           P.bowRodR * 1.45F),
           C.wood, nullptr, 1.0F);

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, top_end, nock, P.stringR), C.stringCol,
           nullptr, 1.0F);
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, nock, bot_end, P.stringR), C.stringCol,
           nullptr, 1.0F);
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, P.hand_r, nock, 0.0045F),
           C.stringCol * 0.9F, nullptr, 1.0F);

  QVector3D tail = nock - forward * 0.06F;
  QVector3D tip = tail + forward * 0.90F;
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, tail, tip, 0.035F),
           C.wood, nullptr, 1.0F);
  QVector3D head_base = tip - forward * 0.10F;
  out.mesh(getUnitCone(), coneFromTo(p.model, head_base, tip, 0.05F),
           C.metalHead, nullptr, 1.0F);
  QVector3D f1b = tail - forward * 0.02F, f1a = f1b - forward * 0.06F;
  QVector3D f2b = tail + forward * 0.02F, f2a = f2b + forward * 0.06F;
  out.mesh(getUnitCone(), coneFromTo(p.model, f1b, f1a, 0.04F), C.fletch,
           nullptr, 1.0F);
  out.mesh(getUnitCone(), coneFromTo(p.model, f2a, f2b, 0.04F), C.fletch,
           nullptr, 1.0F);
}

void registerArcherRenderer(Render::GL::EntityRendererRegistry &registry) {
  registry.registerRenderer(
      "archer", [](const DrawContext &p, ISubmitter &out) {
        QVector3D tunic(0.8F, 0.9F, 1.0F);
        Engine::Core::UnitComponent *unit = nullptr;
        Engine::Core::RenderableComponent *rc = nullptr;
        if (p.entity) {
          unit = p.entity->getComponent<Engine::Core::UnitComponent>();
          rc = p.entity->getComponent<Engine::Core::RenderableComponent>();
        }
        if (unit && unit->owner_id > 0) {
          tunic = Game::Visuals::team_colorForOwner(unit->owner_id);
        } else if (rc) {
          tunic = QVector3D(rc->color[0], rc->color[1], rc->color[2]);
        }

        uint32_t seed = 0u;
        if (unit)
          seed ^= uint32_t(unit->owner_id * 2654435761u);
        if (p.entity)
          seed ^= uint32_t(reinterpret_cast<uintptr_t>(p.entity) & 0xFFFFFFFFu);

        ArcherPose pose = makePose(seed);
        ArcherColors colors = makeColors(tunic);

        drawQuiver(p, out, colors, pose, seed);
        draw_legs(p, out, pose, colors);
        drawTorso(p, out, colors, pose);
        draw_arms(p, out, pose, colors);
        drawHeadAndNeck(p, out, pose, colors);
        drawBowAndArrow(p, out, pose, colors);
      });
}

} // namespace Render::GL
