#include "plume_renderer.h"

#include "../../../entity/registry.h"
#include "../../../geom/transforms.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>
#include <cmath>

namespace Render::GL {

void PlumeRenderer::render(const DrawContext &ctx,
                           const HorseBodyFrames &frames,
                           const HorseVariant &variant,
                           const HorseAnimationContext &anim,
                           ISubmitter &out) const {

  const HorseAttachmentFrame &head = frames.head;

  QVector3D const plume_color = variant.blanketColor;
  float const sway = std::sin(anim.time * 2.5F) * 0.08F;

  QVector3D const base_pos =
      head.origin + head.up * 0.28F + head.forward * 0.05F;

  for (int i = 0; i < 3; ++i) {
    float const offset = (i - 1) * 0.04F;
    QVector3D const feather_base = base_pos + head.right * offset;
    QVector3D const feather_tip =
        feather_base + head.up * 0.25F + head.forward * sway;

    out.cylinder(feather_base, feather_tip, 0.018F - i * 0.002F, plume_color,
                 0.85F);
  }
}

} // namespace Render::GL
