#pragma once

#include "../equipment/horse/i_horse_equipment_renderer.h"
#include "../horse/horse_renderer_base.h"

#include <memory>
#include <vector>

namespace Render::GL {

class HorseRenderer : public HorseRendererBase {
public:
  using HorseRendererBase::render;

  HorseRenderer();
  explicit HorseRenderer(
      std::vector<std::shared_ptr<IHorseEquipmentRenderer>> attachments);

  void set_attachments(
      const std::vector<std::shared_ptr<IHorseEquipmentRenderer>> &attachments);

  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override;

protected:
  void draw_attachments(const DrawContext &ctx, const AnimationInputs &anim,
                        const HumanoidAnimationContext &rider_ctx,
                        HorseProfile &profile,
                        const MountedAttachmentFrame &mount, float phase,
                        float bob, float rein_slack,
                        const HorseBodyFrames &frames,
                        ISubmitter &out) const override;

private:
  std::vector<std::shared_ptr<IHorseEquipmentRenderer>> m_attachments;
};

} // namespace Render::GL
