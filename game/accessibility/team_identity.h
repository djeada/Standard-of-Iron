#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace Game::Accessibility {

enum class TeamPattern : int {
  Solid = 0,
  Dashed,
  DoubleRing,
  Notched,
  Dotted,
  Chevron,
};

inline constexpr int k_team_pattern_count = 6;

enum class PaletteVariant : int {
  Standard = 0,
  Protanopia,
  Deuteranopia,
  Tritanopia,
};

using TeamColor = std::array<float, 3>;

inline constexpr std::size_t k_team_palette_size = 4;

namespace TeamIdentity {

void set_palette_variant(PaletteVariant variant);
[[nodiscard]] auto palette_variant() -> PaletteVariant;

void set_palette_variant_from_mode(std::string_view color_vision_mode);

[[nodiscard]] auto
palette(PaletteVariant variant) -> const std::array<TeamColor, k_team_palette_size>&;

[[nodiscard]] auto color_for_slot(int slot) -> TeamColor;
[[nodiscard]] auto color_for_slot(int slot, PaletteVariant variant) -> TeamColor;
[[nodiscard]] auto pattern_for_slot(int slot) -> TeamPattern;

void set_patterns_enabled(bool enabled);
[[nodiscard]] auto patterns_enabled() -> bool;

[[nodiscard]] auto relative_luminance(const TeamColor& color) -> float;

} // namespace TeamIdentity

} // namespace Game::Accessibility
