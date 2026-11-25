#pragma once

#include <cstdint>
#include <optional>

namespace Render::Creature::Pipeline {

[[nodiscard]] auto horse_clip_contact_y(std::uint16_t clip_id,
                                        float phase) noexcept -> std::optional<float>;

} // namespace Render::Creature::Pipeline
