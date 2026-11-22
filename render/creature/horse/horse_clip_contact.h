#pragma once

#include <cstdint>
#include <optional>

namespace Render::Creature::Pipeline {

// The baked foot-contact height of a horse clip at `phase`, from the horse BPAT
// blob; empty when the blob or the clip is missing.
[[nodiscard]] auto horse_clip_contact_y(std::uint16_t clip_id,
                                        float phase) noexcept -> std::optional<float>;

} // namespace Render::Creature::Pipeline
