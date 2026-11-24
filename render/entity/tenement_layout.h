#pragma once

#include <array>
#include <cstdint>

// Town housing that fills its lot ("style": "tenement" on a home): the lot
// geometry every nation's tenement shares, and which variant a lot draws.
// Nothing here draws.
namespace Render::GL {

// Lot extents in building-local units. A tenement's side walls stand on the
// edge of the home's navigation footprint, so two tenements on neighbouring
// lots share a party wall; the street front keeps a strip for the pavement.
namespace TenementLot {
inline constexpr float k_half_width = 1.192F;
inline constexpr float k_front = 1.08F;
inline constexpr float k_rear = -1.20F;
inline constexpr float k_wall = 0.08F;
inline constexpr float k_pavement_reach = 1.50F;
inline constexpr float k_pavement_top = 0.035F;
inline constexpr float k_ground_storey = 1.00F;
inline constexpr float k_upper_storey = 0.72F;
} // namespace TenementLot

enum class TenementGround : std::uint8_t {
  Shop,
  Door,
  Passage
};

enum class TenementRoof : std::uint8_t {
  Gable,
  Terrace
};

struct TenementTraits {
  int storeys{2};
  TenementGround ground{TenementGround::Shop};
  TenementRoof roof{TenementRoof::Gable};
  // Storey (1 = first above the street) that carries a balcony; 0 for none.
  int balcony_storey{0};
  float roof_rise{0.44F};
};

inline constexpr int k_tenement_variant_count = 8;

inline constexpr std::array<TenementTraits, k_tenement_variant_count> k_tenement_traits{
    {
        {.storeys = 2, .ground = TenementGround::Shop, .roof_rise = 0.42F},
        {.storeys = 2, .ground = TenementGround::Door, .roof_rise = 0.46F},
        {.storeys = 2,
         .ground = TenementGround::Passage,
         .roof = TenementRoof::Terrace},
        {.storeys = 3,
         .ground = TenementGround::Shop,
         .balcony_storey = 1,
         .roof_rise = 0.44F},
        {.storeys = 3, .ground = TenementGround::Door, .roof_rise = 0.40F},
        {.storeys = 3,
         .ground = TenementGround::Shop,
         .roof = TenementRoof::Terrace,
         .balcony_storey = 2},
        {.storeys = 4,
         .ground = TenementGround::Shop,
         .balcony_storey = 1,
         .roof_rise = 0.46F},
        {.storeys = 4,
         .ground = TenementGround::Passage,
         .balcony_storey = 2,
         .roof_rise = 0.42F},
    }};

[[nodiscard]] constexpr auto tenement_traits(int variant) -> const TenementTraits& {
  return k_tenement_traits[static_cast<std::size_t>(
      variant < 0 || variant >= k_tenement_variant_count ? 0 : variant)];
}

// Height of the wall top (the eave, or the terrace deck) above the lot floor.
[[nodiscard]] constexpr auto tenement_wall_top(const TenementTraits& traits) -> float {
  return TenementLot::k_pavement_top + TenementLot::k_ground_storey +
         (static_cast<float>(traits.storeys - 1) * TenementLot::k_upper_storey);
}

// Which variant the lot at a world position draws. Storey counts come from a
// coarse cell first, so a street reads as runs of similar height with the odd
// taller or lower house, instead of a random sawtooth.
[[nodiscard]] auto tenement_visual_variant(float world_x, float world_z) -> int;

} // namespace Render::GL
