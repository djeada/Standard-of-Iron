#include "horse_renderer.h"

#include "../creature/pipeline/unit_visual_spec.h"
namespace Render::GL {

HorseRenderer::HorseRenderer() = default;

auto HorseRenderer::visual_spec() const
    -> const Render::Creature::Pipeline::UnitVisualSpec & {
  if (!m_visual_spec_baked) {
    m_visual_spec_cache = Render::Creature::Pipeline::UnitVisualSpec{};
    m_visual_spec_cache.kind = Render::Creature::Pipeline::CreatureKind::Horse;
    m_visual_spec_cache.debug_name = "horse/with_attachments";
    m_visual_spec_cache.owned_legacy_slots =
        Render::Creature::Pipeline::LegacySlotMask::HorseAttachments;
    m_visual_spec_baked = true;
  }
  return m_visual_spec_cache;
}

} // namespace Render::GL
