#pragma once

#include <cstdint>

namespace Render::GL {

enum SawToolPaletteSlot : std::uint8_t {
  k_saw_wood_slot = 0U,
  k_saw_metal_slot = 1U,
  k_saw_metal_dark_slot = 2U,
  k_saw_leather_slot = 3U,
};

enum ChiselToolPaletteSlot : std::uint8_t {
  k_chisel_wood_slot = 0U,
  k_chisel_metal_slot = 1U,
};

} // namespace Render::GL
