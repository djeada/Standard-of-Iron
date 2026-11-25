#pragma once

#include <cstdint>

namespace Render::Creature {

[[nodiscard]] auto creature_frame_serial() noexcept -> std::uint32_t;
void advance_creature_frame_serial() noexcept;

[[nodiscard]] auto creature_runtime_generation() noexcept -> std::uint32_t;
void advance_creature_runtime_generation() noexcept;

} // namespace Render::Creature
