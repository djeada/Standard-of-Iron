#include "palette.h"
#include "geom/math_utils.h"
#include "humanoid/humanoid_math.h"
#include <cstdint>
#include <qvectornd.h>

namespace Render::GL {

using Render::Geom::clamp_vec_01;

auto make_humanoid_palette(const QVector3D &team_tint,
                           uint32_t seed) -> HumanoidPalette {
  HumanoidPalette p;

  float const variation = (hash_01(seed) - 0.5F) * 0.08F;
  p.cloth = clamp_vec_01(team_tint * (1.0F + variation));

  p.skin = [&]() {
    float const t = hash_01(seed ^ 0x53C17F0BU);
    float const tint = hash_01(seed ^ 0x914A6FE3U);
    float const value_jitter =
        (hash_01(seed ^ 0x2B7Cu) - 0.5F) * 0.06F;

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
    QVector3D tinted(base.x() + warm_bias,
                     base.y() + warm_bias * 0.3F,
                     base.z() - warm_bias * 0.5F);
    tinted *= (1.0F + value_jitter);
    return clamp_vec_01(tinted);
  }();

  float const leather_var = (hash_01(seed ^ 0x1234U) - 0.5F) * 0.06F;
  float const r = team_tint.x();
  float const g = team_tint.y();
  float const b = team_tint.z();
  float const saturation = 0.6F;
  float const brightness = 0.5F;
  QVector3D const desaturated(r * saturation + (1.0F - saturation) * brightness,
                              g * saturation + (1.0F - saturation) * brightness,
                              b * saturation +
                                  (1.0F - saturation) * brightness);
  p.leather = clamp_vec_01(desaturated * (0.7F + leather_var));
  p.leather_dark = p.leather * 0.85F;

  p.wood = QVector3D(0.16F, 0.10F, 0.05F);

  QVector3D const neutral_gray(0.70F, 0.70F, 0.70F);
  p.metal = clamp_vec_01(team_tint * 0.25F + neutral_gray * 0.75F);

  return p;
}

} // namespace Render::GL
