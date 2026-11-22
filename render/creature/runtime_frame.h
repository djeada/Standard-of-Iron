#pragma once

#include <cstdint>

namespace Render::Creature {

// The creature runtime clock. The frame serial advances once per rendered
// frame (and on a runtime reset); the generation advances only on a reset.
// Per-frame caches in the creature pipeline key on both.
[[nodiscard]] auto creature_frame_serial() noexcept -> std::uint32_t;
void advance_creature_frame_serial() noexcept;

[[nodiscard]] auto creature_runtime_generation() noexcept -> std::uint32_t;
void advance_creature_runtime_generation() noexcept;

} // namespace Render::Creature
