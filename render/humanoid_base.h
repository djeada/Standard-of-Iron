#pragma once

#include "entity/registry.h"
#include "humanoid_specs.h"
#include "palette.h"
#include <QVector3D>
#include <cstdint>

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Render::GL {

struct AnimationInputs {
  float time;
  bool isMoving;
  bool is_attacking;
  bool isMelee;
  bool isInHoldMode;
  bool isExitingHold;
  float holdExitProgress;
};

struct FormationParams {
  int individuals_per_unit;
  int max_per_row;
  float spacing;
};

struct HumanoidPose {
  QVector3D headPos;
  float headR{};
  QVector3D neck_base;

  QVector3D shoulderL, shoulderR;
  QVector3D elbowL, elbowR;
  QVector3D handL, hand_r;

  QVector3D pelvisPos;
  QVector3D knee_l, knee_r;

  float footYOffset{};
  QVector3D footL, foot_r;
};

struct VariationParams {
  float height_scale;
  float bulkScale;
  float stanceWidth;
  float armSwingAmp;
  float walkSpeedMult;
  float postureSlump;
  float shoulderTilt;

  static auto fromSeed(uint32_t seed) -> VariationParams {
    VariationParams v{};

    auto nextRand = [](uint32_t &s) -> float {
      s = s * 1664525U + 1013904223U;
      return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    uint32_t rng = seed;
    v.height_scale = 0.95F + nextRand(rng) * 0.10F;
    v.bulkScale = 0.92F + nextRand(rng) * 0.16F;
    v.stanceWidth = 0.88F + nextRand(rng) * 0.24F;
    v.armSwingAmp = 0.85F + nextRand(rng) * 0.30F;
    v.walkSpeedMult = 0.90F + nextRand(rng) * 0.20F;
    v.postureSlump = nextRand(rng) * 0.08F;
    v.shoulderTilt = (nextRand(rng) - 0.5F) * 0.06F;

    return v;
  }
};

struct HumanoidVariant {
  HumanoidPalette palette;
};

class HumanoidRendererBase {
public:
  virtual ~HumanoidRendererBase() = default;

  virtual auto getProportionScaling() const -> QVector3D {
    return {1.0F, 1.0F, 1.0F};
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

  virtual void draw_armorOverlay(const DrawContext &ctx,
                                 const HumanoidVariant &v,
                                 const HumanoidPose &pose, float y_top_cover,
                                 float torso_r, float shoulder_half_span,
                                 float upper_arm_r, const QVector3D &right_axis,
                                 ISubmitter &out) const;

  virtual void drawShoulderDecorations(const DrawContext &ctx,
                                       const HumanoidVariant &v,
                                       const HumanoidPose &pose,
                                       float y_top_cover, float y_neck,
                                       const QVector3D &right_axis,
                                       ISubmitter &out) const;

  void render(const DrawContext &ctx, ISubmitter &out) const;

protected:
  mutable QVector3D m_cachedProportionScale;
  mutable bool m_proportionScaleCached = false;

  static auto resolveFormation(const DrawContext &ctx) -> FormationParams;

  static void computeLocomotionPose(uint32_t seed, float time, bool isMoving,
                                    const VariationParams &variation,
                                    HumanoidPose &ioPose);

  static auto sampleAnimState(const DrawContext &ctx) -> AnimationInputs;

  static auto resolveTeamTint(const DrawContext &ctx) -> QVector3D;

  void drawCommonBody(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose, ISubmitter &out) const;
};

} // namespace Render::GL
