#include "palette.h"

#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "humanoid/humanoid_math.h"
#include "math/math_utils.h"

namespace Render::GL {

using Render::Geom::clamp_vec_01;

namespace {

// A team color chosen purely for identity can land anywhere in the gamut, and the
// dark/desaturated end of that range sits at the same lightness as grass, soil or
// rock. Remapping the hue into a bounded lightness/chroma window keeps the team
// readable against every biome without changing which color the player picked.
constexpr float k_cloth_min_lightness = 0.52F;
constexpr float k_cloth_max_lightness = 0.72F;
constexpr float k_cloth_min_chroma = 0.12F;
constexpr float k_cloth_max_chroma = 0.22F;
constexpr float k_achromatic_epsilon = 1e-4F;

auto srgb_to_linear(float channel) -> float {
  return channel <= 0.04045F ? channel / 12.92F
                             : std::pow((channel + 0.055F) / 1.055F, 2.4F);
}

auto linear_to_srgb(float channel) -> float {
  return channel <= 0.0031308F ? channel * 12.92F
                               : 1.055F * std::pow(channel, 1.0F / 2.4F) - 0.055F;
}

struct Oklch {
  float lightness;
  float chroma;
  float hue;
};

auto rgb_to_oklch(const QVector3D& rgb) -> Oklch {
  float const r = srgb_to_linear(rgb.x());
  float const g = srgb_to_linear(rgb.y());
  float const b = srgb_to_linear(rgb.z());

  float const long_cone = 0.4122214708F * r + 0.5363325363F * g + 0.0514459929F * b;
  float const medium_cone = 0.2119034982F * r + 0.6806995451F * g + 0.1073969566F * b;
  float const short_cone = 0.0883024619F * r + 0.2817188376F * g + 0.6299787005F * b;

  float const l_ = std::cbrt(long_cone);
  float const m_ = std::cbrt(medium_cone);
  float const s_ = std::cbrt(short_cone);

  float const lightness = 0.2104542553F * l_ + 0.7936177850F * m_ - 0.0040720468F * s_;
  float const green_red = 1.9779984951F * l_ - 2.4285922050F * m_ + 0.4505937099F * s_;
  float const blue_yellow =
      0.0259040371F * l_ + 0.7827717662F * m_ - 0.8086757660F * s_;

  return Oklch{lightness,
               std::sqrt(green_red * green_red + blue_yellow * blue_yellow),
               std::atan2(blue_yellow, green_red)};
}

auto oklch_to_rgb(const Oklch& color) -> QVector3D {
  float const green_red = color.chroma * std::cos(color.hue);
  float const blue_yellow = color.chroma * std::sin(color.hue);

  float const l_ =
      color.lightness + 0.3963377774F * green_red + 0.2158037573F * blue_yellow;
  float const m_ =
      color.lightness - 0.1055613458F * green_red - 0.0638541728F * blue_yellow;
  float const s_ =
      color.lightness - 0.0894841775F * green_red - 1.2914855480F * blue_yellow;

  float const long_cone = l_ * l_ * l_;
  float const medium_cone = m_ * m_ * m_;
  float const short_cone = s_ * s_ * s_;

  float const r = 4.0767416621F * long_cone - 3.3077115913F * medium_cone +
                  0.2309699292F * short_cone;
  float const g = -1.2684380046F * long_cone + 2.6097574011F * medium_cone -
                  0.3413193965F * short_cone;
  float const b = -0.0041960863F * long_cone - 0.7034186147F * medium_cone +
                  1.7076147010F * short_cone;

  return clamp_vec_01(QVector3D(linear_to_srgb(std::max(r, 0.0F)),
                                linear_to_srgb(std::max(g, 0.0F)),
                                linear_to_srgb(std::max(b, 0.0F))));
}

// Preserves the hue the player picked, constrains only how light and how saturated
// the cloth is allowed to be.
auto make_readable_team_cloth(const QVector3D& team_tint) -> QVector3D {
  Oklch color = rgb_to_oklch(team_tint);
  color.lightness =
      std::clamp(color.lightness, k_cloth_min_lightness, k_cloth_max_lightness);
  if (color.chroma > k_achromatic_epsilon) {
    // A neutral team color has no hue to preserve; forcing chroma onto it would
    // invent one, so leave true grays alone and only bound their lightness.
    color.chroma = std::clamp(color.chroma, k_cloth_min_chroma, k_cloth_max_chroma);
  }
  return oklch_to_rgb(color);
}

} // namespace

auto make_humanoid_palette(const QVector3D& team_tint,
                           uint32_t seed) -> HumanoidPalette {
  HumanoidPalette p;

  float const variation = (hash_01(seed) - 0.5F) * 0.08F;
  p.cloth = clamp_vec_01(make_readable_team_cloth(team_tint) * (1.0F + variation));

  p.skin = [&]() {
    float const t = hash_01(seed ^ 0x53C17F0BU);
    float const tint = hash_01(seed ^ 0x914A6FE3U);
    float const value_jitter = (hash_01(seed ^ 0x2B7CU) - 0.5F) * 0.06F;

    QVector3D const cool_light(0.97F, 0.83F, 0.74F);
    QVector3D const warm_light(0.96F, 0.78F, 0.66F);
    QVector3D const olive_mid(0.78F, 0.62F, 0.49F);
    QVector3D const tan_mid(0.71F, 0.54F, 0.40F);
    QVector3D const brown_dark(0.50F, 0.36F, 0.27F);
    QVector3D const deep_dark(0.34F, 0.24F, 0.18F);

    QVector3D base;
    if (t < 0.18F) {
      float const s = t / 0.18F;
      base = cool_light * (1.0F - s) + warm_light * s;
    } else if (t < 0.42F) {
      float const s = (t - 0.18F) / 0.24F;
      base = warm_light * (1.0F - s) + olive_mid * s;
    } else if (t < 0.66F) {
      float const s = (t - 0.42F) / 0.24F;
      base = olive_mid * (1.0F - s) + tan_mid * s;
    } else if (t < 0.88F) {
      float const s = (t - 0.66F) / 0.22F;
      base = tan_mid * (1.0F - s) + brown_dark * s;
    } else {
      float const s = (t - 0.88F) / 0.12F;
      base = brown_dark * (1.0F - s) + deep_dark * s;
    }

    float const warm_bias = (tint - 0.5F) * 0.04F;
    QVector3D tinted(
        base.x() + warm_bias, base.y() + warm_bias * 0.3F, base.z() - warm_bias * 0.5F);
    tinted *= (1.0F + value_jitter);
    return clamp_vec_01(tinted);
  }();

  // Leather and metal stay neutral on purpose. Tinting them toward the team hue
  // pushed the whole soldier into one value band; keeping them dark-brown and
  // bright-gray gives the figure internal contrast that survives any background.
  float const leather_var = (hash_01(seed ^ 0x1234U) - 0.5F) * 0.06F;
  QVector3D const neutral_leather(0.24F, 0.15F, 0.09F);
  p.leather = clamp_vec_01(neutral_leather * (1.0F + leather_var));
  p.leather_dark = p.leather * 0.72F;

  p.wood = QVector3D(0.16F, 0.10F, 0.05F);

  p.metal = QVector3D(0.68F, 0.70F, 0.72F);

  return p;
}

} // namespace Render::GL
