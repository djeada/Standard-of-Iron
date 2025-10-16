#include "palette.h"
#include "geom/math_utils.h"
#include "humanoid_math.h"

namespace Render::GL {

using Render::Geom::clamp01;
using Render::Geom::clampVec01;

HumanoidPalette makeHumanoidPalette(const QVector3D &teamTint, uint32_t seed) {
  HumanoidPalette p;

  float variation = (hash01(seed) - 0.5f) * 0.08f;
  p.cloth = clampVec01(teamTint * (1.0f + variation));

  p.skin = QVector3D(0.96f, 0.80f, 0.69f);

  float leatherVar = (hash01(seed ^ 0x1234u) - 0.5f) * 0.06f;
  float r = teamTint.x();
  float g = teamTint.y();
  float b = teamTint.z();
  float saturation = 0.6f;
  float brightness = 0.5f;
  QVector3D desaturated(r * saturation + (1.0f - saturation) * brightness,
                        g * saturation + (1.0f - saturation) * brightness,
                        b * saturation + (1.0f - saturation) * brightness);
  p.leather = clampVec01(desaturated * (0.7f + leatherVar));
  p.leatherDark = p.leather * 0.85f;

  p.wood = QVector3D(0.16f, 0.10f, 0.05f);

  QVector3D neutralGray(0.70f, 0.70f, 0.70f);
  p.metal = clampVec01(teamTint * 0.25f + neutralGray * 0.75f);

  return p;
}

} // namespace Render::GL
