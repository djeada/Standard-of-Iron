#pragma once

#include "../creature/pipeline/unit_visual_spec.h"
#include "../entity/registry.h"
#include "attachment_frames.h"
#include "dimensions.h"
#include <QVector3D>

namespace Render::GL {

struct AnimationInputs;
class ISubmitter;

class ElephantRendererBase {
public:
  virtual ~ElephantRendererBase() = default;

  virtual auto
  visual_spec() const -> const Render::Creature::Pipeline::UnitVisualSpec &;

  virtual auto get_proportion_scaling() const -> QVector3D {
    return {1.0F, 1.0F, 1.0F};
  }

  void render(const DrawContext &ctx, const AnimationInputs &anim,
              ElephantProfile &profile,
              const HowdahAttachmentFrame *shared_howdah,
              const ElephantMotionSample *shared_motion, ISubmitter &out,
              HorseLOD lod) const;

  void render(const DrawContext &ctx, const AnimationInputs &anim,
              ElephantProfile &profile,
              const HowdahAttachmentFrame *shared_howdah,
              const ElephantMotionSample *shared_motion, ISubmitter &out) const;

  void render_simplified(const DrawContext &ctx, const AnimationInputs &anim,
                         ElephantProfile &profile,
                         const HowdahAttachmentFrame *shared_howdah,
                         const ElephantMotionSample *shared_motion,
                         ISubmitter &out) const;

  void render_minimal(const DrawContext &ctx, ElephantProfile &profile,
                      const ElephantMotionSample *shared_motion,
                      ISubmitter &out) const;

private:
  void render_full(const DrawContext &ctx, const AnimationInputs &anim,
                   ElephantProfile &profile,
                   const HowdahAttachmentFrame *shared_howdah,
                   const ElephantMotionSample *shared_motion,
                   ISubmitter &out) const;
};

} // namespace Render::GL
