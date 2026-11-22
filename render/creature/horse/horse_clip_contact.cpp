#include "render/creature/horse/horse_clip_contact.h"

#include "animation/bpat/bpat_playback.h"
#include "animation/bpat/bpat_registry.h"

namespace Render::Creature::Pipeline {

auto horse_clip_contact_y(std::uint16_t clip_id,
                          float phase) noexcept -> std::optional<float> {
  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(
      Render::Creature::Bpat::k_species_horse);
  if (blob == nullptr || clip_id >= blob->clip_count()) {
    return std::nullopt;
  }

  auto const playback = resolve_bpat_playback(blob, clip_id, phase);
  if (!playback.valid()) {
    return std::nullopt;
  }

  auto const contacts = blob->frame_contacts();
  if (playback.global_frame >= contacts.size()) {
    return std::nullopt;
  }
  return contacts[playback.global_frame].foot_y;
}

} // namespace Render::Creature::Pipeline
