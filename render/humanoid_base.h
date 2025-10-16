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

struct HumanoidVariant {
  HumanoidPalette palette;
};

class HumanoidRendererBase {
public:
  virtual ~HumanoidRendererBase() = default;

  virtual void getVariant(const DrawContext &ctx, uint32_t seed,
                          HumanoidVariant &v) const;

  virtual void customizePose(const DrawContext &ctx,
                             const AnimationInputs &anim, uint32_t seed,
                             HumanoidPose &ioPose) const;

  virtual void addAttachments(const DrawContext &ctx,
                              const HumanoidVariant &v,
                              const HumanoidPose &pose,
                              ISubmitter &out) const;

  void render(const DrawContext &ctx, ISubmitter &out) const;

protected:
  static FormationParams resolveFormation(const DrawContext &ctx);

  static void computeLocomotionPose(uint32_t seed, float time, bool isMoving,
                                    HumanoidPose &ioPose);

  static AnimationInputs sampleAnimState(const DrawContext &ctx);

  static QVector3D resolveTeamTint(const DrawContext &ctx);

  static void drawCommonBody(const DrawContext &ctx, const HumanoidVariant &v,
                             const HumanoidPose &pose, ISubmitter &out);

  static void drawSelectionFX(const DrawContext &ctx, ISubmitter &out);
};

} // namespace Render::GL
