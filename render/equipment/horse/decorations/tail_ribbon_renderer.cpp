#include "tail_ribbon_renderer.h"

#include "../../../entity/registry.h"
#include "../../../gl/primitives.h"
#include <QMatrix4x4>
#include <cmath>

namespace Render::GL {

void TailRibbonRenderer::render(const DrawContext &ctx,
                                const HorseBodyFrames &frames,
                                const HorseVariant &variant,
                                const HorseAnimationContext &anim,
                                ISubmitter &out) const {

  const HorseAttachmentFrame &tail = frames.tail_base;

  QVector3D const ribbon_color = variant.blanket_color;

  QVector3D const ribbon_start = tail.origin + tail.up * 0.05F;
  QVector3D const ribbon_end =
      ribbon_start - tail.forward * 0.15F + tail.up * 0.08F;

  float const wave = std::sin(anim.time * 3.0F + anim.phase * 6.28F) * 0.05F;
  QVector3D const ribbon_mid =
      (ribbon_start + ribbon_end) * 0.5F + tail.right * wave;

  out.cylinder(ribbon_start, ribbon_mid, 0.015F, ribbon_color, 0.90F);
  out.cylinder(ribbon_mid, ribbon_end, 0.015F, ribbon_color, 0.90F);

  QMatrix4x4 bow = ctx.model;
  bow.translate(ribbon_start);
  bow.scale(0.08F, 0.08F, 0.06F);
  out.mesh(getUnitSphere(), bow, ribbon_color, nullptr, 1.0F, 4);
}

} // namespace Render::GL
