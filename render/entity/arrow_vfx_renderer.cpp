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

  static constexpr float TOTAL_HEIGHT = 2.00f;
  static constexpr float HEAD_HEIGHT = 0.25f;

  static constexpr float GROUND_Y = -1.00f;
  static constexpr float HEAD_TOP_Y = GROUND_Y + TOTAL_HEIGHT;
  static constexpr float CHIN_Y = HEAD_TOP_Y - HEAD_HEIGHT;
  static constexpr float NECK_BASE_Y = CHIN_Y - 0.10f;
  static constexpr float SHOULDER_Y = NECK_BASE_Y - 0.15f;
  static constexpr float CHEST_Y = SHOULDER_Y - 0.35f;
  static constexpr float WAIST_Y = CHEST_Y - 0.30f;
  static constexpr float HIP_Y = WAIST_Y - 0.15f;
  static constexpr float KNEE_Y = HIP_Y - 0.35f;

  static constexpr float SHOULDER_WIDTH = HEAD_HEIGHT * 1.6f;
  static constexpr float HEAD_RADIUS = HEAD_HEIGHT * 0.40f;
  static constexpr float NECK_RADIUS = HEAD_RADIUS * 0.35f;
  static constexpr float TORSO_TOP_R = HEAD_RADIUS * 1.0f;
  static constexpr float TORSO_BOT_R = HEAD_RADIUS * 0.9f;
  static constexpr float UPPER_ARM_R = HEAD_RADIUS * 0.30f;
  static constexpr float FORE_ARM_R = HEAD_RADIUS * 0.25f;
  static constexpr float HAND_RADIUS = HEAD_RADIUS * 0.22f;
  static constexpr float UPPER_LEG_R = HEAD_RADIUS * 0.38f;
  static constexpr float LOWER_LEG_R = HEAD_RADIUS * 0.32f;

  static constexpr float UPPER_ARM_LEN = 0.28f;
  static constexpr float FORE_ARM_LEN = 0.30f;
  static constexpr float UPPER_LEG_LEN = 0.35f;
  static constexpr float LOWER_LEG_LEN = 0.35f;
};

struct ArcherColors {
  QVector3D tunic, skin, leather, leatherDark, wood, metal, metalHead,
      stringCol, fletch;
};

struct ArcherPose {
  using P = HumanProportions;

  QVector3D headPos{0.0f, (P::HEAD_TOP_Y + P::CHIN_Y) * 0.5f, 0.0f};
  float headR = P::HEAD_RADIUS;
  QVector3D neckBase{0.0f, P::NECK_BASE_Y, 0.0f};

  QVector3D shoulderL{-P::SHOULDER_WIDTH * 0.5f, P::SHOULDER_Y, 0.1f};
  QVector3D shoulderR{P::SHOULDER_WIDTH * 0.5f, P::SHOULDER_Y, 0.1f};

  QVector3D elbowL, elbowR;
  QVector3D handL, handR;

  float hipSpacing = P::SHOULDER_WIDTH * 0.55f;
  QVector3D hipL{-hipSpacing, P::HIP_Y, 0.03f};
  QVector3D hipR{hipSpacing, P::HIP_Y, -0.03f};

  QVector3D footL{-hipSpacing * 1.6f, P::GROUND_Y, 0.10f};
  QVector3D footR{hipSpacing * 1.6f, P::GROUND_Y, -0.10f};

  float bowX = 0.0f;
  float bowTopY = P::SHOULDER_Y + 0.55f;
  float bowBotY = P::HIP_Y - 0.25f;
  float bowRodR = 0.035f;
  float stringR = 0.008f;
  float bowDepth = 0.25f;
};

static inline ArcherPose makePose(uint32_t seed) {
  (void)seed;
  ArcherPose P;

  using HP = HumanProportions;

  P.handL = QVector3D(P.bowX - 0.05f, HP::SHOULDER_Y + 0.05f, 0.55f);

  P.handR = QVector3D(0.15f, HP::SHOULDER_Y + 0.15f, 0.20f);

  QVector3D shoulderToHandL = P.handL - P.shoulderL;
  float distL = shoulderToHandL.length();
  QVector3D dirL = shoulderToHandL.normalized();

  QVector3D perpL(-dirL.z(), 0.0f, dirL.x());
  float elbowOffsetL = 0.15f;
  P.elbowL = P.shoulderL + dirL * (distL * 0.45f) + perpL * elbowOffsetL +
             QVector3D(0, -0.08f, 0);

  QVector3D shoulderToHandR = P.handR - P.shoulderR;
  float distR = shoulderToHandR.length();
  QVector3D dirR = shoulderToHandR.normalized();

  QVector3D perpR(-dirR.z(), 0.0f, dirR.x());
  float elbowOffsetR = 0.12f;
  P.elbowR = P.shoulderR + dirR * (distR * 0.48f) + perpR * elbowOffsetR +
             QVector3D(0, 0.02f, 0);

  return P;
}

static inline ArcherColors makeColors(const QVector3D &teamTint) {
  ArcherColors C;
  auto tint = [&](float k) {
    return QVector3D(clamp01(teamTint.x() * k), clamp01(teamTint.y() * k),
                     clamp01(teamTint.z() * k));
  };
  C.tunic = teamTint;
  C.skin = QVector3D(0.96f, 0.80f, 0.69f);
  C.leather = QVector3D(0.35f, 0.22f, 0.12f);
  C.leatherDark = C.leather * 0.9f;
  C.wood = QVector3D(0.16f, 0.10f, 0.05f);
  C.metal = QVector3D(0.65f, 0.66f, 0.70f);
  C.metalHead = clampVec01(C.metal * 1.1f);
  C.stringCol = QVector3D(0.30f, 0.30f, 0.32f);
  C.fletch = tint(0.9f);
  return C;
}

static inline void drawTorso(const DrawContext &p, ISubmitter &out,
                             const ArcherColors &C, const ArcherPose &P) {
  using HP = HumanProportions;

  QVector3D torsoTop{0.0f, HP::NECK_BASE_Y - 0.05f, 0.0f};
  QVector3D torsoBot{0.0f, HP::WAIST_Y, 0.0f};

  float torsoRadius = HP::TORSO_TOP_R;

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, torsoTop, torsoBot, torsoRadius), C.tunic,
           nullptr, 1.0f);

  QVector3D waist{0.0f, HP::WAIST_Y, 0.0f};
  QVector3D hipCenter = (P.hipL + P.hipR) * 0.5f;

  out.mesh(getUnitCone(),
           coneFromTo(p.model, waist, hipCenter, HP::TORSO_BOT_R),
           C.tunic * 0.9f, nullptr, 1.0f);
}

static inline void drawHeadAndNeck(const DrawContext &p, ISubmitter &out,
                                   const ArcherPose &P, const ArcherColors &C) {
  using HP = HumanProportions;

  QVector3D chinPos{0.0f, HP::CHIN_Y, 0.0f};
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.neckBase, chinPos, HP::NECK_RADIUS),
           C.skin * 0.9f, nullptr, 1.0f);

  out.mesh(getUnitSphere(), sphereAt(p.model, P.headPos, P.headR), C.skin,
           nullptr, 1.0f);

  QVector3D iris(0.06f, 0.06f, 0.07f);
  float eyeZ = P.headR * 0.7f;
  float eyeY = P.headPos.y() + P.headR * 0.1f;
  float eyeSpacing = P.headR * 0.35f;
  out.mesh(getUnitSphere(),
           p.model *
               sphereAt(QVector3D(-eyeSpacing, eyeY, eyeZ), P.headR * 0.15f),
           iris, nullptr, 1.0f);
  out.mesh(getUnitSphere(),
           p.model *
               sphereAt(QVector3D(eyeSpacing, eyeY, eyeZ), P.headR * 0.15f),
           iris, nullptr, 1.0f);

  QVector3D domeC = P.headPos + QVector3D(0.0f, P.headR * 0.25f, 0.0f);
  float domeR = P.headR * 1.05f;
  out.mesh(getUnitSphere(), sphereAt(p.model, domeC, domeR), C.metal, nullptr,
           1.0f);

  QVector3D visorBase(0.0f, P.headPos.y() + P.headR * 0.10f, P.headR * 0.80f);
  QVector3D visorTip = visorBase + QVector3D(0.0f, -0.015f, 0.06f);
  out.mesh(getUnitCone(),
           coneFromTo(p.model, visorBase, visorTip, P.headR * 0.38f),
           C.metal * 0.92f, nullptr, 1.0f);

  QVector3D cheekL0(-P.headR * 0.85f, P.headPos.y() + P.headR * 0.05f, 0.02f);
  QVector3D cheekL1(-P.headR * 0.85f, P.headPos.y() - P.headR * 0.20f, 0.04f);
  QVector3D cheekR0(P.headR * 0.85f, P.headPos.y() + P.headR * 0.05f, -0.02f);
  QVector3D cheekR1(P.headR * 0.85f, P.headPos.y() - P.headR * 0.20f, -0.04f);
  out.mesh(getUnitCone(),
           coneFromTo(p.model, cheekL0, cheekL1, P.headR * 0.24f),
           C.metal * 0.95f, nullptr, 1.0f);
  out.mesh(getUnitCone(),
           coneFromTo(p.model, cheekR0, cheekR1, P.headR * 0.24f),
           C.metal * 0.95f, nullptr, 1.0f);
}

static inline void drawArms(const DrawContext &p, ISubmitter &out,
                            const ArcherPose &P, const ArcherColors &C) {
  using HP = HumanProportions;

  const float upperArmR = HP::UPPER_ARM_R;
  const float foreArmR = HP::FORE_ARM_R;
  const float jointR = upperArmR * 1.2f;

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.shoulderL, P.elbowL, upperArmR), C.tunic,
           nullptr, 1.0f);

  out.mesh(getUnitSphere(), sphereAt(p.model, P.elbowL, jointR),
           C.tunic * 0.95f, nullptr, 1.0f);

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.elbowL, P.handL, foreArmR),
           C.skin * 0.95f, nullptr, 1.0f);

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.shoulderR, P.elbowR, upperArmR), C.tunic,
           nullptr, 1.0f);

  out.mesh(getUnitSphere(), sphereAt(p.model, P.elbowR, jointR),
           C.tunic * 0.95f, nullptr, 1.0f);

  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.elbowR, P.handR, foreArmR),
           C.skin * 0.95f, nullptr, 1.0f);
}

static inline void drawLegs(const DrawContext &p, ISubmitter &out,
                            const ArcherPose &P, const ArcherColors &C) {
  using HP = HumanProportions;

  const float thighR = HP::UPPER_LEG_R;
  const float shinR = HP::LOWER_LEG_R;
  const float kneeJointR = thighR * 1.15f;

  auto makeKnee = [&](const QVector3D &hip, const QVector3D &foot,
                      float outwardSign) {
    const float t = 0.38f;
    QVector3D knee = hip * (1.0f - t) + foot * t;
    knee.setY(HP::KNEE_Y + 0.03f);
    knee.setZ(knee.z() + 0.05f);
    knee.setX(knee.x() + outwardSign * 0.06f);
    return knee;
  };

  QVector3D kneeL = makeKnee(P.hipL, P.footL, -1.0f);
  QVector3D kneeR = makeKnee(P.hipR, P.footR, 1.0f);

  out.mesh(getUnitCone(), coneFromTo(p.model, P.hipL, kneeL, thighR), C.leather,
           nullptr, 1.0f);
  out.mesh(getUnitCone(), coneFromTo(p.model, P.hipR, kneeR, thighR), C.leather,
           nullptr, 1.0f);

  out.mesh(getUnitSphere(), sphereAt(p.model, kneeL, kneeJointR),
           C.leather * 0.95f, nullptr, 1.0f);
  out.mesh(getUnitSphere(), sphereAt(p.model, kneeR, kneeJointR),
           C.leather * 0.95f, nullptr, 1.0f);

  out.mesh(getUnitCone(), coneFromTo(p.model, kneeL, P.footL, shinR),
           C.leatherDark, nullptr, 1.0f);
  out.mesh(getUnitCone(), coneFromTo(p.model, kneeR, P.footR, shinR),
           C.leatherDark, nullptr, 1.0f);

  QVector3D down(0.0f, -0.02f, 0.0f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.footL, P.footL + down, shinR * 1.1f),
           C.leatherDark, nullptr, 1.0f);
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, P.footR, P.footR + down, shinR * 1.1f),
           C.leatherDark, nullptr, 1.0f);
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

  QVector3D qTop(-0.08f, HP::SHOULDER_Y + 0.10f, -0.25f);
  QVector3D qBase(-0.10f, HP::CHEST_Y, -0.22f);

  float quiverR = HP::HEAD_RADIUS * 0.45f;
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, qBase, qTop, quiverR),
           C.leather, nullptr, 1.0f);

  float j = (hash01(seed) - 0.5f) * 0.04f;
  float k = (hash01(seed ^ 0x9E3779B9u) - 0.5f) * 0.04f;

  QVector3D a1 = qTop + QVector3D(0.00f + j, 0.08f, 0.00f + k);
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, qTop, a1, 0.010f),
           C.wood, nullptr, 1.0f);
  out.mesh(getUnitCone(),
           coneFromTo(p.model, a1, a1 + QVector3D(0, 0.05f, 0), 0.025f),
           C.fletch, nullptr, 1.0f);

  QVector3D a2 = qTop + QVector3D(0.02f - j, 0.07f, 0.02f - k);
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, qTop, a2, 0.010f),
           C.wood, nullptr, 1.0f);
  out.mesh(getUnitCone(),
           coneFromTo(p.model, a2, a2 + QVector3D(0, 0.05f, 0), 0.025f),
           C.fletch, nullptr, 1.0f);
}

static inline void drawBowAndArrow(const DrawContext &p, ISubmitter &out,
                                   const ArcherPose &P, const ArcherColors &C) {
  const QVector3D up(0.0f, 1.0f, 0.0f);
  const QVector3D forward(0.0f, 0.0f, 1.0f);

  QVector3D grip = P.handL;
  QVector3D topEnd(P.bowX, P.bowTopY, grip.z());
  QVector3D botEnd(P.bowX, P.bowBotY, grip.z());

  QVector3D nock(P.bowX,
                 clampf(P.handR.y(), P.bowBotY + 0.05f, P.bowTopY - 0.05f),
                 clampf(P.handR.z(), grip.z() - 0.30f, grip.z() + 0.30f));

  const int segs = 22;
  auto qBezier = [](const QVector3D &a, const QVector3D &c, const QVector3D &b,
                    float t) {
    float u = 1.0f - t;
    return u * u * a + 2.0f * u * t * c + t * t * b;
  };
  QVector3D ctrl = nock + forward * P.bowDepth;
  QVector3D prev = botEnd;
  for (int i = 1; i <= segs; ++i) {
    float t = float(i) / float(segs);
    QVector3D cur = qBezier(botEnd, ctrl, topEnd, t);
    out.mesh(getUnitCylinder(), cylinderBetween(p.model, prev, cur, P.bowRodR),
             C.wood, nullptr, 1.0f);
    prev = cur;
  }
  out.mesh(getUnitCylinder(),
           cylinderBetween(p.model, grip - up * 0.05f, grip + up * 0.05f,
                           P.bowRodR * 1.45f),
           C.wood, nullptr, 1.0f);

  out.mesh(getUnitCylinder(), cylinderBetween(p.model, topEnd, nock, P.stringR),
           C.stringCol, nullptr, 1.0f);
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, nock, botEnd, P.stringR),
           C.stringCol, nullptr, 1.0f);
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, P.handR, nock, 0.0045f),
           C.stringCol * 0.9f, nullptr, 1.0f);

  QVector3D tail = nock - forward * 0.06f;
  QVector3D tip = tail + forward * 0.90f;
  out.mesh(getUnitCylinder(), cylinderBetween(p.model, tail, tip, 0.035f),
           C.wood, nullptr, 1.0f);
  QVector3D headBase = tip - forward * 0.10f;
  out.mesh(getUnitCone(), coneFromTo(p.model, headBase, tip, 0.05f),
           C.metalHead, nullptr, 1.0f);
  QVector3D f1b = tail - forward * 0.02f, f1a = f1b - forward * 0.06f;
  QVector3D f2b = tail + forward * 0.02f, f2a = f2b + forward * 0.06f;
  out.mesh(getUnitCone(), coneFromTo(p.model, f1b, f1a, 0.04f), C.fletch,
           nullptr, 1.0f);
  out.mesh(getUnitCone(), coneFromTo(p.model, f2a, f2b, 0.04f), C.fletch,
           nullptr, 1.0f);
}

static inline void drawSelectionFX(const DrawContext &p, ISubmitter &out) {
  if (p.selected || p.hovered) {
    float ringSize = 0.5f;
    if (p.entity) {
      auto *unit = p.entity->getComponent<Engine::Core::UnitComponent>();
      if (unit && !unit->unitType.empty()) {
        ringSize = Game::Units::TroopConfig::instance().getSelectionRingSize(
            unit->unitType);
      }
    }

    QMatrix4x4 ringM;
    QVector3D pos = p.model.column(3).toVector3D();
    ringM.translate(pos.x(), 0.15f, pos.z());
    ringM.scale(ringSize, 1.0f, ringSize);
    if (p.selected)
      out.selectionRing(ringM, 0.6f, 0.25f, QVector3D(0.2f, 0.8f, 0.2f));
    else
      out.selectionRing(ringM, 0.35f, 0.15f, QVector3D(0.90f, 0.90f, 0.25f));
  }
}

void registerArcherRenderer(Render::GL::EntityRendererRegistry &registry) {
  registry.registerRenderer(
      "archer", [](const DrawContext &p, ISubmitter &out) {
        QVector3D tunic(0.8f, 0.9f, 1.0f);
        Engine::Core::UnitComponent *unit = nullptr;
        Engine::Core::RenderableComponent *rc = nullptr;
        if (p.entity) {
          unit = p.entity->getComponent<Engine::Core::UnitComponent>();
          rc = p.entity->getComponent<Engine::Core::RenderableComponent>();
        }
        if (unit && unit->ownerId > 0) {
          tunic = Game::Visuals::teamColorForOwner(unit->ownerId);
        } else if (rc) {
          tunic = QVector3D(rc->color[0], rc->color[1], rc->color[2]);
        }

        uint32_t seed = 0u;
        if (unit)
          seed ^= uint32_t(unit->ownerId * 2654435761u);
        if (p.entity)
          seed ^= uint32_t(reinterpret_cast<uintptr_t>(p.entity) & 0xFFFFFFFFu);

        ArcherPose pose = makePose(seed);
        ArcherColors colors = makeColors(tunic);

        drawQuiver(p, out, colors, pose, seed);
        drawLegs(p, out, pose, colors);
        drawTorso(p, out, colors, pose);
        drawArms(p, out, pose, colors);
        drawHeadAndNeck(p, out, pose, colors);
        drawBowAndArrow(p, out, pose, colors);
        drawSelectionFX(p, out);
      });
}

} // namespace Render::GL
