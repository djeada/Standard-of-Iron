#include "render/entity/town_palette.h"

#include <span>

#include "render/entity/building_decay.h"
#include "render/entity/home_renderer_common.h"
#include "render/math/math_utils.h"

namespace Render::GL {
namespace {

using Render::Geom::clamp_vec_01;

constexpr std::array<QVector3D, 7> k_roman_washes{
    QVector3D(0.90F, 0.80F, 0.60F),
    QVector3D(0.88F, 0.70F, 0.47F),
    QVector3D(0.86F, 0.63F, 0.50F),
    QVector3D(0.92F, 0.88F, 0.78F),
    QVector3D(0.82F, 0.72F, 0.55F),
    QVector3D(0.91F, 0.84F, 0.69F),
    QVector3D(0.80F, 0.60F, 0.44F),
};

constexpr std::array<QVector3D, 5> k_roman_accents{
    QVector3D(0.58F, 0.21F, 0.16F),
    QVector3D(0.63F, 0.40F, 0.20F),
    QVector3D(0.42F, 0.26F, 0.19F),
    QVector3D(0.55F, 0.28F, 0.20F),
    QVector3D(0.34F, 0.30F, 0.27F),
};

constexpr std::array<QVector3D, 5> k_roman_cloths{
    QVector3D(0.66F, 0.24F, 0.19F),
    QVector3D(0.83F, 0.62F, 0.24F),
    QVector3D(0.25F, 0.36F, 0.52F),
    QVector3D(0.80F, 0.72F, 0.56F),
    QVector3D(0.38F, 0.47F, 0.30F),
};

constexpr std::array<QVector3D, 6> k_punic_washes{
    QVector3D(0.94F, 0.91F, 0.83F),
    QVector3D(0.92F, 0.86F, 0.72F),
    QVector3D(0.90F, 0.88F, 0.84F),
    QVector3D(0.88F, 0.80F, 0.66F),
    QVector3D(0.93F, 0.87F, 0.78F),
    QVector3D(0.86F, 0.76F, 0.62F),
};

constexpr std::array<QVector3D, 5> k_punic_accents{
    QVector3D(0.20F, 0.36F, 0.56F),
    QVector3D(0.55F, 0.17F, 0.17F),
    QVector3D(0.22F, 0.44F, 0.42F),
    QVector3D(0.24F, 0.30F, 0.50F),
    QVector3D(0.78F, 0.56F, 0.20F),
};

constexpr std::array<QVector3D, 5> k_punic_cloths{
    QVector3D(0.42F, 0.16F, 0.40F),
    QVector3D(0.60F, 0.20F, 0.22F),
    QVector3D(0.82F, 0.66F, 0.30F),
    QVector3D(0.22F, 0.34F, 0.52F),
    QVector3D(0.84F, 0.78F, 0.64F),
};

auto pick(std::span<const QVector3D> washes,
          std::span<const QVector3D> accents,
          std::span<const QVector3D> cloths,
          float wash_jitter,
          const QVector3D& team,
          std::uint32_t seed,
          BuildingState state) -> TownPalette {
  const int decay_seed = static_cast<int>(seed & 0xFFFFU);
  const QVector3D wash = washes[home_tint_pick(seed, washes.size())] *
                         (1.0F + (home_tint_jitter(seed) * wash_jitter));
  const QVector3D accent = accents[home_tint_pick(seed ^ 0x9E3779B9U, accents.size())] *
                           (1.0F + (home_tint_jitter(seed ^ 0x51ED27U) * 0.08F));
  const QVector3D cloth = cloths[home_tint_pick(seed ^ 0x7F4A7C15U, cloths.size())];
  return {clamp_vec_01(team),
          decayed_color(clamp_vec_01(wash), state, decay_seed),
          decayed_color(clamp_vec_01(accent), state, decay_seed + 1),
          decayed_color(cloth, state, decay_seed + 2)};
}

} // namespace

auto roman_town_palette(const QVector3D& team,
                        std::uint32_t seed,
                        BuildingState state) -> TownPalette {
  return pick(
      k_roman_washes, k_roman_accents, k_roman_cloths, 0.07F, team, seed, state);
}

auto carthage_town_palette(const QVector3D& team,
                           std::uint32_t seed,
                           BuildingState state) -> TownPalette {
  return pick(
      k_punic_washes, k_punic_accents, k_punic_cloths, 0.05F, team, seed, state);
}

} // namespace Render::GL
