#include "quiver_renderer.h"
#include "../../entity/registry.h"
#include "../../geom/math_utils.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../gl/render_constants.h"
#include "../../humanoid/humanoid_math.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/humanoid_specs.h"
#include "../equipment_submit.h"

#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;
using Render::GL::HashXorShift::k_golden_ratio;

QuiverRenderer::QuiverRenderer(QuiverRenderConfig config)
    : m_config(std::move(config)) {}

void QuiverRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                            const HumanoidPalette &palette,
                            const HumanoidAnimationContext &anim,
                            EquipmentBatch &batch) {
  submit(m_config, ctx, frames, palette, anim, batch);
}

void QuiverRenderer::submit(const QuiverRenderConfig &config,
                            const DrawContext &ctx, const BodyFrames &frames,
                            const HumanoidPalette &palette,
                            const HumanoidAnimationContext &,
                            EquipmentBatch &batch) {

  QVector3D const hip_r =
      frames.waist.origin + frames.waist.right * frames.waist.radius * 0.9F;

  QVector3D const quiver_pos =
      hip_r + frames.waist.right * 0.15F - frames.waist.up * 0.10F;

  QVector3D const q_top =
      quiver_pos + frames.waist.up * 0.15F - frames.waist.forward * 0.10F;
  QVector3D const q_base =
      quiver_pos - frames.waist.up * 0.25F + frames.waist.forward * 0.05F;

  batch.meshes.push_back(
      {get_unit_cylinder(), nullptr,
       cylinder_between(ctx.model, q_base, q_top, config.quiver_radius),
       palette.leather, nullptr, 1.0F, config.material_id});

  uint32_t seed = 0U;
  if (ctx.entity != nullptr) {
    seed = uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
  }

  float const j = (hash_01(seed) - 0.5F) * 0.04F;
  float const k = (hash_01(seed ^ k_golden_ratio) - 0.5F) * 0.04F;

  QVector3D const a1 = q_top + QVector3D(0.00F + j, 0.08F, 0.00F + k);
  batch.meshes.push_back({get_unit_cylinder(), nullptr,
                          cylinder_between(ctx.model, q_top, a1, 0.010F),
                          palette.wood, nullptr, 1.0F, config.material_id});
  batch.meshes.push_back(
      {get_unit_cone(), nullptr,
       cone_from_to(ctx.model, a1, a1 + QVector3D(0, 0.05F, 0), 0.025F),
       config.fletching_color, nullptr, 1.0F, config.material_id});

  if (config.num_arrows >= 2) {
    QVector3D const a2 = q_top + QVector3D(0.02F - j, 0.07F, 0.02F - k);
    batch.meshes.push_back({get_unit_cylinder(), nullptr,
                            cylinder_between(ctx.model, q_top, a2, 0.010F),
                            palette.wood, nullptr, 1.0F, config.material_id});
    batch.meshes.push_back(
        {get_unit_cone(), nullptr,
         cone_from_to(ctx.model, a2, a2 + QVector3D(0, 0.05F, 0), 0.025F),
         config.fletching_color, nullptr, 1.0F, config.material_id});
  }
}

} // namespace Render::GL
