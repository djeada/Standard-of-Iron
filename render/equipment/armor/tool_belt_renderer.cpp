#include "tool_belt_renderer.h"
#include "../../geom/math_utils.h"
#include "../../geom/transforms.h"
#include "../../gl/primitives.h"
#include "../../humanoid/humanoid_specs.h"
#include "../../submitter.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>
#include <numbers>

namespace Render::GL {

using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

ToolBeltRenderer::ToolBeltRenderer(const ToolBeltConfig &config)
    : m_config(config) {}

void ToolBeltRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                              const HumanoidPalette &,
                              const HumanoidAnimationContext &,
                              ISubmitter &submitter) {
  renderBelt(ctx, frames.waist, submitter);

  if (m_config.include_hammer) {
    renderHammerLoop(ctx, frames.waist, submitter);
  }

  if (m_config.include_chisel) {
    renderChiselHolder(ctx, frames.waist, submitter);
  }

  if (m_config.include_pouches) {
    renderPouches(ctx, frames.waist, submitter);
  }
}

void ToolBeltRenderer::renderBelt(const DrawContext &ctx,
                                  const AttachmentFrame &waist,
                                  ISubmitter &submitter) {
  if (waist.radius <= 0.0F) {
    return;
  }

  QVector3D const belt_color = m_config.leather_color;
  QVector3D const buckle_color = m_config.metal_color;

  const QVector3D &origin = waist.origin;
  const QVector3D &right = waist.right;
  const QVector3D &forward = waist.forward;
  const QVector3D &up = waist.up;

  float const waist_r = waist.radius * 1.05F;
  float const waist_d =
      (waist.depth > 0.0F) ? waist.depth * 0.90F : waist.radius * 0.80F;

  constexpr int segs = 16;
  constexpr float pi = std::numbers::pi_v<float>;
  float const belt_y = -0.02F;
  float const belt_thickness = 0.022F;

  for (int i = 0; i < segs; ++i) {
    float const a1 = (float(i) / segs) * 2.0F * pi;
    float const a2 = (float(i + 1) / segs) * 2.0F * pi;

    QVector3D const p1 = origin + right * (waist_r * std::sin(a1)) +
                         forward * (waist_d * std::cos(a1)) + up * belt_y;
    QVector3D const p2 = origin + right * (waist_r * std::sin(a2)) +
                         forward * (waist_d * std::cos(a2)) + up * belt_y;

    submitter.mesh(get_unit_cylinder(),
                   cylinder_between(ctx.model, p1, p2, belt_thickness),
                   belt_color, nullptr, 1.0F);
  }

  QVector3D const buckle_pos =
      origin + forward * waist.radius * 0.92F - right * 0.05F + up * belt_y;
  submitter.mesh(get_unit_sphere(), sphere_at(ctx.model, buckle_pos, 0.030F),
                 buckle_color, nullptr, 1.0F);

  QVector3D const buckle_pin = buckle_pos + right * 0.035F;
  submitter.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, buckle_pos, buckle_pin, 0.008F),
                 buckle_color * 0.85F, nullptr, 1.0F);
}

void ToolBeltRenderer::renderHammerLoop(const DrawContext &ctx,
                                        const AttachmentFrame &waist,
                                        ISubmitter &submitter) {
  if (waist.radius <= 0.0F) {
    return;
  }

  QVector3D const loop_color = m_config.leather_color * 0.90F;
  QVector3D const hammer_wood = m_config.wood_color;
  QVector3D const hammer_metal = m_config.metal_color * 0.92F;

  constexpr float pi = std::numbers::pi_v<float>;
  float const side_angle = -0.35F * pi;

  QVector3D const loop_pos =
      waist.origin + waist.right * (waist.radius * std::sin(side_angle)) +
      waist.forward * (waist.radius * std::cos(side_angle)) - waist.up * 0.05F;

  for (int i = 0; i < 3; ++i) {
    float const t = float(i) / 2.0F;
    QVector3D const pos = loop_pos - waist.up * (t * 0.10F);
    float const r = 0.014F - t * 0.003F;
    submitter.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, r), loop_color,
                   nullptr, 1.0F);
  }

  QVector3D const hammer_top = loop_pos - waist.up * 0.08F;
  QVector3D const hammer_handle_bot = hammer_top - waist.up * 0.12F;

  submitter.mesh(
      get_unit_cylinder(),
      cylinder_between(ctx.model, hammer_top, hammer_handle_bot, 0.008F),
      hammer_wood, nullptr, 1.0F);

  QVector3D const hammer_head_center = hammer_top + waist.up * 0.015F;
  QVector3D const hammer_head_left = hammer_head_center - waist.right * 0.025F;
  QVector3D const hammer_head_right = hammer_head_center + waist.right * 0.025F;

  submitter.mesh(
      get_unit_cylinder(),
      cylinder_between(ctx.model, hammer_head_left, hammer_head_right, 0.012F),
      hammer_metal, nullptr, 1.0F);
}

void ToolBeltRenderer::renderChiselHolder(const DrawContext &ctx,
                                          const AttachmentFrame &waist,
                                          ISubmitter &submitter) {
  if (waist.radius <= 0.0F) {
    return;
  }

  QVector3D const holder_color = m_config.leather_color * 0.88F;
  QVector3D const chisel_metal = m_config.metal_color * 0.90F;

  constexpr float pi = std::numbers::pi_v<float>;
  float const side_angle = 0.30F * pi;

  QVector3D const holder_pos =
      waist.origin + waist.right * (waist.radius * std::sin(side_angle)) +
      waist.forward * (waist.radius * std::cos(side_angle)) - waist.up * 0.04F;

  submitter.mesh(get_unit_sphere(), sphere_at(ctx.model, holder_pos, 0.018F),
                 holder_color, nullptr, 1.0F);

  QVector3D const chisel_bot = holder_pos - waist.up * 0.02F;
  QVector3D const chisel_top = holder_pos + waist.up * 0.08F;

  submitter.mesh(get_unit_cylinder(),
                 cylinder_between(ctx.model, chisel_bot, chisel_top, 0.006F),
                 chisel_metal, nullptr, 1.0F);

  submitter.mesh(get_unit_sphere(), sphere_at(ctx.model, chisel_top, 0.008F),
                 chisel_metal * 1.15F, nullptr, 1.0F);
}

void ToolBeltRenderer::renderPouches(const DrawContext &ctx,
                                     const AttachmentFrame &waist,
                                     ISubmitter &submitter) {
  if (waist.radius <= 0.0F) {
    return;
  }

  QVector3D const pouch_color = m_config.leather_color * 0.85F;

  constexpr float pi = std::numbers::pi_v<float>;

  for (int side = 0; side < 2; ++side) {
    float const pouch_angle = (side == 0) ? 0.50F * pi : -0.50F * pi;

    QVector3D const pouch_pos =
        waist.origin +
        waist.right * (waist.radius * 0.95F * std::sin(pouch_angle)) +
        waist.forward * (waist.radius * 0.85F * std::cos(pouch_angle)) -
        waist.up * 0.06F;

    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 3; ++j) {
        float const x_off = (float(i) - 1.5F) * 0.015F;
        float const y_off = float(j) * 0.022F;

        QVector3D const pos = pouch_pos +
                              waist.right * x_off * (side == 0 ? 1.0F : -1.0F) -
                              waist.up * y_off;

        float const r = 0.012F - float(j) * 0.002F;
        submitter.mesh(get_unit_sphere(), sphere_at(ctx.model, pos, r),
                       pouch_color, nullptr, 1.0F);
      }
    }
  }
}

} 
