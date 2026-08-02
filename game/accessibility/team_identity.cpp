#include "team_identity.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace Game::Accessibility {

namespace {

using Palette = std::array<TeamColor, k_team_palette_size>;

constexpr Palette k_standard{{
    {0.20F, 0.55F, 1.00F},
    {1.00F, 0.30F, 0.30F},
    {0.20F, 0.80F, 0.40F},
    {1.00F, 0.80F, 0.20F},
}};

constexpr Palette k_red_green_safe{{
    {0.086F, 0.212F, 0.498F},
    {0.835F, 0.369F, 0.000F},
    {0.337F, 0.706F, 0.914F},
    {0.941F, 0.894F, 0.259F},
}};

constexpr Palette k_blue_yellow_safe{{
    {0.549F, 0.114F, 0.094F},
    {0.180F, 0.545F, 0.239F},
    {1.000F, 0.541F, 0.502F},
    {0.925F, 0.937F, 0.878F},
}};

std::atomic<PaletteVariant> g_variant{PaletteVariant::Standard};
std::atomic<bool> g_patterns_enabled{false};

auto to_linear(float channel) -> float {
  return channel <= 0.04045F ? channel / 12.92F
                             : std::pow((channel + 0.055F) / 1.055F, 2.4F);
}

auto slot_to_index(int slot, std::size_t modulus) -> std::size_t {
  const int normalized = std::max(0, slot - 1);
  return static_cast<std::size_t>(normalized) % modulus;
}

} // namespace

namespace TeamIdentity {

void set_palette_variant(PaletteVariant variant) {
  g_variant.store(variant, std::memory_order_relaxed);
}

auto palette_variant() -> PaletteVariant {
  return g_variant.load(std::memory_order_relaxed);
}

void set_palette_variant_from_mode(std::string_view color_vision_mode) {
  if (color_vision_mode == "protanopia") {
    set_palette_variant(PaletteVariant::Protanopia);
  } else if (color_vision_mode == "deuteranopia") {
    set_palette_variant(PaletteVariant::Deuteranopia);
  } else if (color_vision_mode == "tritanopia") {
    set_palette_variant(PaletteVariant::Tritanopia);
  } else {
    set_palette_variant(PaletteVariant::Standard);
  }
}

auto palette(PaletteVariant variant) -> const Palette& {
  switch (variant) {
  case PaletteVariant::Protanopia:
  case PaletteVariant::Deuteranopia:
    return k_red_green_safe;
  case PaletteVariant::Tritanopia:
    return k_blue_yellow_safe;
  case PaletteVariant::Standard:
  default:
    return k_standard;
  }
}

auto color_for_slot(int slot, PaletteVariant variant) -> TeamColor {
  return palette(variant)[slot_to_index(slot, k_team_palette_size)];
}

auto color_for_slot(int slot) -> TeamColor {
  return color_for_slot(slot, palette_variant());
}

void set_patterns_enabled(bool enabled) {
  g_patterns_enabled.store(enabled, std::memory_order_relaxed);
}

auto patterns_enabled() -> bool {
  return g_patterns_enabled.load(std::memory_order_relaxed);
}

auto pattern_for_slot(int slot) -> TeamPattern {
  return static_cast<TeamPattern>(
      static_cast<int>(slot_to_index(slot, k_team_pattern_count)));
}

auto relative_luminance(const TeamColor& color) -> float {
  return 0.2126F * to_linear(color[0]) + 0.7152F * to_linear(color[1]) +
         0.0722F * to_linear(color[2]);
}

} // namespace TeamIdentity

} // namespace Game::Accessibility
