#include "spear_renderer.h"
#include "../../entity/renderer_constants.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../humanoid/spear_pose_utils.h"
#include "../equipment_submit.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>

namespace Render::GL {

using Render::Geom::cone_from_to;
using Render::Geom::cylinder_between;

SpearRenderer::SpearRenderer(SpearRenderConfig config)
    : m_base(std::move(config)) {}

void SpearRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           EquipmentBatch &batch) {
  submit(m_base, ctx, frames, palette, anim, batch);
}

void SpearRenderer::submit(const SpearRenderConfig &m_config,
                           const DrawContext &ctx, const BodyFrames &frames,
                           const HumanoidPalette &palette,
                           const HumanoidAnimationContext &anim,
                           EquipmentBatch &batch) {
  QVector3D const grip_pos = frames.hand_r.origin;

  QVector3D const spear_dir = compute_spear_direction(anim.inputs);

  QVector3D const shaft_base = grip_pos - spear_dir * 0.28F;
  QVector3D shaft_mid = grip_pos + spear_dir * (m_config.spear_length * 0.5F);
  QVector3D const shaft_tip = grip_pos + spear_dir * m_config.spear_length;

  shaft_mid.setY(shaft_mid.y() + 0.02F);

  batch.meshes.push_back({get_unit_cylinder(), nullptr,
                          cylinder_between(ctx.model, shaft_base, shaft_mid,
                                           m_config.shaft_radius),
                          m_config.shaft_color, nullptr, 1.0F,
                          m_config.material_id});

  batch.meshes.push_back({get_unit_cylinder(), nullptr,
                          cylinder_between(ctx.model, shaft_mid, shaft_tip,
                                           m_config.shaft_radius * 0.95F),
                          m_config.shaft_color * 0.98F, nullptr, 1.0F,
                          m_config.material_id});

  QVector3D const spearhead_base = shaft_tip;
  QVector3D const spearhead_tip =
      shaft_tip + spear_dir * m_config.spearhead_length;

  batch.meshes.push_back({get_unit_cone(), nullptr,
                          cone_from_to(ctx.model, spearhead_base, spearhead_tip,
                                       m_config.shaft_radius * 1.8F),
                          m_config.spearhead_color, nullptr, 1.0F,
                          m_config.material_id});

  QVector3D const grip_end = grip_pos + spear_dir * 0.10F;
  batch.meshes.push_back({get_unit_cylinder(), nullptr,
                          cylinder_between(ctx.model, grip_pos, grip_end,
                                           m_config.shaft_radius * 1.5F),
                          palette.leather * 0.92F, nullptr, 1.0F,
                          m_config.material_id});
}

} // namespace Render::GL
