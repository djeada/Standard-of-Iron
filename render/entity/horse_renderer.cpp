#include "horse_renderer.h"

#include "../equipment/horse/i_horse_equipment_renderer.h"
#include "../humanoid/rig.h"

#include <utility>

namespace Render::GL {

HorseRenderer::HorseRenderer() = default;

HorseRenderer::HorseRenderer(
    std::vector<std::shared_ptr<IHorseEquipmentRenderer>> attachments)
    : m_attachments(std::move(attachments)) {}

void HorseRenderer::set_attachments(
    const std::vector<std::shared_ptr<IHorseEquipmentRenderer>> &attachments) {
  m_attachments = attachments;
}

void HorseRenderer::draw_attachments(
    const DrawContext &ctx, const AnimationInputs &anim,
    const HumanoidAnimationContext &, HorseProfile &profile,
    const MountedAttachmentFrame &, float phase, float bob, float,
    const HorseBodyFrames &frames, ISubmitter &out) const {
  if (m_attachments.empty()) {
    return;
  }

  HorseAnimationContext horse_anim;
  horse_anim.time = anim.time;
  horse_anim.phase = phase;
  horse_anim.bob = bob;
  horse_anim.is_moving = anim.is_moving;
  horse_anim.rider_intensity = 0.0F;

  for (const auto &attachment : m_attachments) {
    if (attachment) {
      attachment->render(ctx, frames, profile.variant, horse_anim, out);
    }
  }
}

} 
