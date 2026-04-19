#pragma once

#include "../creature/pipeline/unit_visual_spec.h"
#include "../entity/registry.h"
#include "attachment_frames.h"
#include "dimensions.h"
#include <QVector3D>

namespace Render::GL {

struct AnimationInputs;
struct HumanoidAnimationContext;
class ISubmitter;

class HorseRendererBase {
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
              const ReinState *shared_reins,
              const HorseMotionSample *shared_motion, ISubmitter &out,
              HorseLOD lod,
              Render::Creature::Pipeline::EquipmentLoadout horse_loadout = {},
              const Render::Creature::Pipeline::EquipmentSubmitContext
                  *sub_ctx_template = nullptr) const;

  void render(const DrawContext &ctx, const AnimationInputs &anim,
              const HumanoidAnimationContext &rider_ctx, HorseProfile &profile,
              const MountedAttachmentFrame *shared_mount,
              const ReinState *shared_reins,
              const HorseMotionSample *shared_motion, ISubmitter &out) const;

  void render_simplified(const DrawContext &ctx, const AnimationInputs &anim,
                         const HumanoidAnimationContext &rider_ctx,
                         HorseProfile &profile,
                         const MountedAttachmentFrame *shared_mount,
                         const HorseMotionSample *shared_motion,
                         ISubmitter &out) const;

  void render_minimal(const DrawContext &ctx, HorseProfile &profile,
                      const HorseMotionSample *shared_motion,
                      ISubmitter &out) const;

protected:
  virtual void draw_attachments(const DrawContext &, const AnimationInputs &,
                                const HumanoidAnimationContext &,
                                HorseProfile &, const MountedAttachmentFrame &,
                                float, float, float, const HorseBodyFrames &,
                                ISubmitter &) const {}

private:
  void render_full(const DrawContext &ctx, const AnimationInputs &anim,
                   const HumanoidAnimationContext &rider_ctx,
                   HorseProfile &profile,
                   const MountedAttachmentFrame *shared_mount,
                   const ReinState *shared_reins,
                   const HorseMotionSample *shared_motion, ISubmitter &out,
                   Render::Creature::Pipeline::EquipmentLoadout horse_loadout,
                   const Render::Creature::Pipeline::EquipmentSubmitContext
                       *sub_ctx_template) const;
};

} // namespace Render::GL
