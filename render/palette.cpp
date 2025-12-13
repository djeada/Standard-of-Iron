#include "palette.h"
#include "geom/math_utils.h"
#include "humanoid/humanoid_math.h"
#include <cstdint>
#include <qvectornd.h>

namespace Render::GL {

using Render::Geom::clampVec01;

auto make_humanoid_palette(const QVector3D &team_tint,
                           uint32_t seed) -> HumanoidPalette {
  HumanoidPalette p;

  float const variation = (hash_01(seed) - 0.5F) * 0.08F;
  p.cloth = clampVec01(team_tint * (1.0F + variation));

  p.skin = QVector3D(0.96F, 0.80F, 0.69F);

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
  p.leather = clampVec01(desaturated * (0.7F + leather_var));
  p.leatherDark = p.leather * 0.85F;

  p.wood = QVector3D(0.16F, 0.10F, 0.05F);

  QVector3D const neutral_gray(0.70F, 0.70F, 0.70F);
  p.metal = clampVec01(team_tint * 0.25F + neutral_gray * 0.75F);

  return p;
}

} // namespace Render::GL
