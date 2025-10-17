#pragma once

#include "entity/registry.h"
#include "humanoid_specs.h"
#include "palette.h"
#include <QVector3D>
#include <cstdint>

namespace Engine {
namespace Core {
class Entity;
class World;
} // namespace Core
} // namespace Engine

namespace Render::GL {

struct AnimationInputs {
  float time;
  bool isMoving;
  bool isAttacking;
  bool isMelee;
  bool isInHoldMode;
  float holdExitProgress;
};

struct FormationParams {
  int individualsPerUnit;
  int maxPerRow;
  float spacing;
};

struct HumanoidPose {
  QVector3D headPos;
  float headR;
  QVector3D neckBase;

  QVector3D shoulderL, shoulderR;
  QVector3D elbowL, elbowR;
  QVector3D handL, handR;

  float footYOffset;
  QVector3D footL, footR;
};

struct VariationParams {
  float heightScale;
  float bulkScale;
  float stanceWidth;
  float armSwingAmp;
  float walkSpeedMult;
  float postureSlump;
  float shoulderTilt;

  static VariationParams fromSeed(uint32_t seed) {
    VariationParams v;

    auto nextRand = [](uint32_t &s) -> float {
      s = s * 1664525u + 1013904223u;
      return float(s & 0x7FFFFFu) / float(0x7FFFFFu);
    };

    uint32_t rng = seed;
    v.heightScale = 0.95f + nextRand(rng) * 0.10f;
    v.bulkScale = 0.92f + nextRand(rng) * 0.16f;
    v.stanceWidth = 0.88f + nextRand(rng) * 0.24f;
    v.armSwingAmp = 0.85f + nextRand(rng) * 0.30f;
    v.walkSpeedMult = 0.90f + nextRand(rng) * 0.20f;
    v.postureSlump = nextRand(rng) * 0.08f;
    v.shoulderTilt = (nextRand(rng) - 0.5f) * 0.06f;

    return v;
  }
};

struct HumanoidVariant {
  HumanoidPalette palette;
};

class HumanoidRendererBase {
public:
  virtual ~HumanoidRendererBase() = default;

  virtual QVector3D getProportionScaling() const {
    return QVector3D(1.0f, 1.0f, 1.0f);
  }

  virtual void getVariant(const DrawContext &ctx, uint32_t seed,
                          HumanoidVariant &v) const;

  virtual void customizePose(const DrawContext &ctx,
                             const AnimationInputs &anim, uint32_t seed,
                             HumanoidPose &ioPose) const;

  virtual void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                              const HumanoidPose &pose,
                              const AnimationInputs &anim,
                              ISubmitter &out) const;

  virtual void drawHelmet(const DrawContext &ctx, const HumanoidVariant &v,
                          const HumanoidPose &pose, ISubmitter &out) const;

  virtual void drawArmorOverlay(const DrawContext &ctx,
                                const HumanoidVariant &v,
                                const HumanoidPose &pose, float yTopCover,
                                float torsoR, float shoulderHalfSpan,
                                float upperArmR, const QVector3D &rightAxis,
                                ISubmitter &out) const;

  virtual void drawShoulderDecorations(const DrawContext &ctx,
                                       const HumanoidVariant &v,
                                       const HumanoidPose &pose,
                                       float yTopCover, float yNeck,
                                       const QVector3D &rightAxis,
                                       ISubmitter &out) const;

  void render(const DrawContext &ctx, ISubmitter &out) const;

protected:
  mutable QVector3D m_cachedProportionScale;
  mutable bool m_proportionScaleCached = false;

  static FormationParams resolveFormation(const DrawContext &ctx);

  static void computeLocomotionPose(uint32_t seed, float time, bool isMoving,
                                    const VariationParams &variation,
                                    HumanoidPose &ioPose);

  static AnimationInputs sampleAnimState(const DrawContext &ctx);

  static QVector3D resolveTeamTint(const DrawContext &ctx);

  void drawCommonBody(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose, ISubmitter &out) const;

  static void drawSelectionFX(const DrawContext &ctx, ISubmitter &out);
};

} // namespace Render::GL
