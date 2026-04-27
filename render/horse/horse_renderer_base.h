#pragma once

#include "../creature/pipeline/unit_visual_spec.h"
#include "../entity/registry.h"
#include "attachment_frames.h"
#include "dimensions.h"
#include <QVector3D>

namespace Render::Creature::Pipeline {
struct CreaturePreparationResult;
}

namespace Render::GL {
struct AnimationInputs;
struct HumanoidAnimationContext;
class ISubmitter;
class HorseRendererBase;
} // namespace Render::GL

namespace Render::Horse {
using HorsePreparation = Render::Creature::Pipeline::CreaturePreparationResult;
void prepare_horse_full(const Render::GL::HorseRendererBase &owner,
                        const Render::GL::DrawContext &ctx,
                        const Render::GL::AnimationInputs &anim,
                        const Render::GL::HumanoidAnimationContext &rider_ctx,
                        Render::GL::HorseProfile &profile,
                        const Render::GL::MountedAttachmentFrame *shared_mount,
                        const Render::GL::HorseMotionSample *shared_motion,
                        HorsePreparation &out);
void prepare_horse_minimal(const Render::GL::HorseRendererBase &owner,
                           const Render::GL::DrawContext &ctx,
                           Render::GL::HorseProfile &profile,
                           const Render::GL::HorseMotionSample *shared_motion,
                           HorsePreparation &out);
} // namespace Render::Horse

namespace Render::GL {

class HorseRendererBase {
  friend void ::Render::Horse::prepare_horse_full(
      const ::Render::GL::HorseRendererBase &owner,
      const ::Render::GL::DrawContext &ctx,
      const ::Render::GL::AnimationInputs &anim,
      const ::Render::GL::HumanoidAnimationContext &rider_ctx,
      ::Render::GL::HorseProfile &profile,
      const ::Render::GL::MountedAttachmentFrame *shared_mount,
      const ::Render::GL::HorseMotionSample *shared_motion,
      ::Render::Horse::HorsePreparation &out);

public:
  virtual ~HorseRendererBase() = default;

  virtual auto
  visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec &;

  virtual auto get_proportion_scaling() const -> QVector3D {
    return {1.0F, 1.0F, 1.0F};
  }

  void render(const DrawContext &ctx, const AnimationInputs &anim,
              const HumanoidAnimationContext &rider_ctx, HorseProfile &profile,
              const MountedAttachmentFrame *shared_mount,
              const HorseMotionSample *shared_motion, ISubmitter &out,
              HorseLOD lod) const;

  void render(const DrawContext &ctx, const AnimationInputs &anim,
              const HumanoidAnimationContext &rider_ctx, HorseProfile &profile,
              const MountedAttachmentFrame *shared_mount,
              const HorseMotionSample *shared_motion, ISubmitter &out) const;

  mutable Render::Creature::Pipeline::UnitVisualSpec m_visual_spec_cache{};
  mutable bool m_visual_spec_baked{false};
};

} // namespace Render::GL
