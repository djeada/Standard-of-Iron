#include "quiver_renderer.h"
#include "../../entity/registry.h"
#include "../../geom/math_utils.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../gl/render_constants.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../humanoid/rig.h"
#include "../../submitter.h"

#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::GL::HashXorShift::k_golden_ratio;

QuiverRenderer::QuiverRenderer(QuiverRenderConfig config) 
    : m_config(std::move(config)) {}

void QuiverRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext & /*anim*/,
                           ISubmitter &submitter) {
  // Use the shoulder positions to compute quiver location
  // This matches the original implementation which used:
  // spine_mid = (pose.shoulderL + pose.shoulderR) * 0.5F
  QVector3D const spine_mid = (frames.shoulderL.origin + frames.shoulderR.origin) * 0.5F;
  QVector3D const quiver_offset(-0.08F, 0.10F, -0.25F);
  QVector3D const q_top = spine_mid + quiver_offset;
  QVector3D const q_base = q_top + QVector3D(-0.02F, -0.30F, 0.03F);

  // Draw the quiver cylinder
  submitter.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, q_base, q_top, m_config.quiver_radius),
                 palette.leather, nullptr, 1.0F);

  // Generate a seed for arrow jitter based on entity pointer
  uint32_t seed = 0U;
  if (ctx.entity != nullptr) {
    seed = uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
  }

  // Draw arrows sticking out of the quiver
  float const j = (hash_01(seed) - 0.5F) * 0.04F;
  float const k = (hash_01(seed ^ k_golden_ratio) - 0.5F) * 0.04F;

  // First arrow
  QVector3D const a1 = q_top + QVector3D(0.00F + j, 0.08F, 0.00F + k);
  submitter.mesh(getUnitCylinder(), 
                 cylinderBetween(ctx.model, q_top, a1, 0.010F),
                 palette.wood, nullptr, 1.0F);
  submitter.mesh(getUnitCone(),
                 coneFromTo(ctx.model, a1, a1 + QVector3D(0, 0.05F, 0), 0.025F),
                 m_config.fletching_color, nullptr, 1.0F);

  // Second arrow (if configured)
  if (m_config.num_arrows >= 2) {
    QVector3D const a2 = q_top + QVector3D(0.02F - j, 0.07F, 0.02F - k);
    submitter.mesh(getUnitCylinder(), 
                   cylinderBetween(ctx.model, q_top, a2, 0.010F),
                   palette.wood, nullptr, 1.0F);
    submitter.mesh(getUnitCone(),
                   coneFromTo(ctx.model, a2, a2 + QVector3D(0, 0.05F, 0), 0.025F),
                   m_config.fletching_color, nullptr, 1.0F);
  }
}

} // namespace Render::GL
