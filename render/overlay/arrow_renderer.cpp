#include "render/overlay/arrow_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "game/systems/arrow_instance.h"
#include "render/frame/draw_resources.h"
#include "render/frame/frame_submitter.h"
#include "render/geom/arrow.h"
#include "render/geom/mesh.h"

namespace Render {

namespace GL {

namespace {

constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;
constexpr float k_streak_xy_scale = 0.62F;

auto remap_alpha(float value, float begin, float end) -> float {
  if (end <= begin) {
    return value >= end ? 1.0F : 0.0F;
  }
  return std::clamp((value - begin) / (end - begin), 0.0F, 1.0F);
}

auto sample_arrow_position(const Game::Systems::ArrowInstance& arrow,
                           float t) -> QVector3D {
  float const clamped_t = std::clamp(t, 0.0F, 1.0F);
  QVector3D const delta = arrow.end - arrow.start;
  QVector3D pos = arrow.start + delta * clamped_t;
  float const height = arrow.arc_height * 4.0F * clamped_t * (1.0F - clamped_t);
  pos.setY(pos.y() + height);
  return pos;
}

auto sample_arrow_tangent(const Game::Systems::ArrowInstance& arrow,
                          float t) -> QVector3D {
  float const clamped_t = std::clamp(t, 0.0F, 1.0F);
  QVector3D tangent = arrow.end - arrow.start;
  tangent.setY(tangent.y() + (4.0F * arrow.arc_height * (1.0F - 2.0F * clamped_t)));
  return tangent;
}

auto scaled_color(const QVector3D& color, float scale) -> QVector3D {
  return {std::clamp(color.x() * scale, 0.0F, 1.0F),
          std::clamp(color.y() * scale, 0.0F, 1.0F),
          std::clamp(color.z() * scale, 0.0F, 1.0F)};
}

auto trail_segments_for_style(Game::Systems::ArrowVisualStyle style) -> int {
  switch (style) {
  case Game::Systems::ArrowVisualStyle::Aimed:
    return 5;
  case Game::Systems::ArrowVisualStyle::Volley:
    return 2;
  case Game::Systems::ArrowVisualStyle::Focused:
  case Game::Systems::ArrowVisualStyle::Javelin:
    return 1;
  case Game::Systems::ArrowVisualStyle::Marker:
  default:
    return 0;
  }
}

auto main_alpha_for_arrow(const Game::Systems::ArrowInstance& arrow, float t) -> float {
  float const fade_in = remap_alpha(t, 0.0F, 0.1F);
  float const fade_out = remap_alpha(1.0F - t, 0.0F, 0.12F);
  float alpha = std::min(fade_in, fade_out);
  if (arrow.style == Game::Systems::ArrowVisualStyle::Marker) {
    alpha = std::max(alpha, 0.9F);
  }
  return alpha;
}

auto build_arrow_model(const Game::Systems::ArrowInstance& arrow,
                       float t,
                       const QVector3D& pos,
                       float scale_multiplier) -> QMatrix4x4 {
  QVector3D const tangent = sample_arrow_tangent(arrow, t);
  QVector3D const forward = tangent.normalized();
  float const horizontal_mag = std::max(
      0.0001F, std::sqrt(forward.x() * forward.x() + forward.z() * forward.z()));
  float const yaw_deg = std::atan2(forward.x(), forward.z()) * k_rad_to_deg;
  float const pitch_deg = -std::atan2(forward.y(), horizontal_mag) * k_rad_to_deg;
  float const roll_deg = arrow.roll_deg + (t * arrow.spin_rate_deg);

  QMatrix4x4 model;
  model.translate(pos.x(), pos.y(), pos.z());
  model.rotate(yaw_deg, QVector3D(0, 1, 0));
  model.rotate(pitch_deg, QVector3D(1, 0, 0));
  model.rotate(roll_deg, QVector3D(0, 0, 1));

  constexpr float arrow_z_scale = Geom::Arrow::k_arrow_z_scale;
  constexpr float arrow_xy_scale = Geom::Arrow::k_arrow_xy_scale;
  constexpr float arrow_z_translate_factor = Geom::Arrow::k_arrow_z_translate_factor;
  float const final_scale = arrow.scale * scale_multiplier;
  model.translate(0.0F, 0.0F, -arrow_z_scale * arrow_z_translate_factor);
  model.scale(arrow_xy_scale * final_scale,
              arrow_xy_scale * final_scale,
              arrow_z_scale * final_scale * arrow.length_scale);
  return model;
}

void draw_arrow_glow(FrameSubmitter* renderer,
                     GL::Mesh* shaft,
                     GL::Mesh* tip,
                     const QMatrix4x4& model,
                     const QVector3D& glow,
                     float alpha_scale) {
  using Geom::Arrow;

  QMatrix4x4 sheath = model;
  sheath.scale(Arrow::k_shaft_glow_xy_scale, Arrow::k_shaft_glow_xy_scale, 1.0F);
  renderer->mesh(shaft, sheath, glow, nullptr, Arrow::k_shaft_glow_alpha * alpha_scale);

  QMatrix4x4 head = model;
  head.translate(0.0F, 0.0F, Arrow::k_head_center_z);
  head.scale(Arrow::k_head_glow_xy_scale,
             Arrow::k_head_glow_xy_scale,
             Arrow::k_head_glow_z_scale);
  head.translate(0.0F, 0.0F, -Arrow::k_head_center_z);
  renderer->mesh(tip, head, glow, nullptr, Arrow::k_head_glow_alpha * alpha_scale);
}

void draw_arrow_mesh(FrameSubmitter* renderer,
                     GL::Mesh* arrow_shaft_mesh,
                     GL::Mesh* arrow_tip_mesh,
                     GL::Mesh* arrow_fletching_mesh,
                     const QMatrix4x4& model,
                     const QVector3D& shaft_color,
                     const QVector3D& tip_color,
                     const QVector3D& fletch_color,
                     float alpha,
                     bool render_tip,
                     bool render_fletch) {
  renderer->mesh(arrow_shaft_mesh, model, shaft_color, nullptr, alpha);
  if (render_tip) {
    renderer->mesh(arrow_tip_mesh, model, tip_color, nullptr, alpha);
  }
  if (render_fletch) {
    QMatrix4x4 fletch_model = model;
    fletch_model.translate(0.0F,
                           0.0F,
                           -Geom::Arrow::k_arrow_z_scale *
                               Geom::Arrow::k_fletch_z_offset_factor);
    fletch_model.scale(Geom::Arrow::k_fletch_xy_scale,
                       Geom::Arrow::k_fletch_xy_scale,
                       Geom::Arrow::k_fletch_z_scale);
    renderer->mesh(arrow_fletching_mesh, fletch_model, fletch_color, nullptr, alpha);
  }
}

} // namespace

void render_arrows(FrameSubmitter* renderer,
                   const DrawResources* resources,
                   const std::vector<Game::Systems::ArrowInstance>& arrows) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }
  auto* arrow_shaft_mesh = Render::Geom::Arrow::get_shaft();
  auto* arrow_tip_mesh = Render::Geom::Arrow::get_tip();
  auto* arrow_fletching_mesh = Render::Geom::Arrow::get_fletching();
  if ((arrow_shaft_mesh == nullptr) || (arrow_tip_mesh == nullptr) ||
      (arrow_fletching_mesh == nullptr)) {
    return;
  }

  for (const auto& arrow : arrows) {
    if (!arrow.active || arrow.t < 0.0F) {
      continue;
    }

    float const clamped_t = std::clamp(arrow.t, 0.0F, 1.0F);
    QVector3D const pos = sample_arrow_position(arrow, clamped_t);
    float const main_alpha = main_alpha_for_arrow(arrow, clamped_t);
    QVector3D const shaft_color =
        scaled_color(Geom::Arrow::shaft_color(arrow.color), arrow.brightness);
    QVector3D const fletch_color =
        scaled_color(Geom::Arrow::fletch_color(arrow.color), arrow.brightness);
    QVector3D const tip_color =
        Geom::Arrow::tip_color(0.94F + (arrow.brightness * 0.12F));

    if (arrow.trail_alpha > 0.001F && arrow.trail_length > 0.0F) {
      int const trail_segments = trail_segments_for_style(arrow.style);
      for (int segment = 1; segment <= trail_segments; ++segment) {
        float const segment_t =
            clamped_t - (arrow.trail_length * (0.45F + (0.38F * segment)));
        if (segment_t <= 0.0F) {
          continue;
        }

        QVector3D const ghost_pos = sample_arrow_position(arrow, segment_t);
        float const ghost_alpha =
            arrow.trail_alpha * main_alpha * (0.5F - (0.16F * segment));
        QMatrix4x4 ghost_model =
            build_arrow_model(arrow, segment_t, ghost_pos, 1.0F - (0.06F * segment));
        ghost_model.scale(k_streak_xy_scale, k_streak_xy_scale, 1.0F);
        renderer->mesh(arrow_shaft_mesh,
                       ghost_model,
                       scaled_color(shaft_color, 0.86F),
                       nullptr,
                       ghost_alpha);
      }
    }

    QMatrix4x4 const model = build_arrow_model(arrow, clamped_t, pos, 1.0F);
    draw_arrow_mesh(renderer,
                    arrow_shaft_mesh,
                    arrow_tip_mesh,
                    arrow_fletching_mesh,
                    model,
                    shaft_color,
                    tip_color,
                    fletch_color,
                    main_alpha,
                    true,
                    arrow.style != Game::Systems::ArrowVisualStyle::Marker &&
                        arrow.style != Game::Systems::ArrowVisualStyle::Javelin);

    if (arrow.style != Game::Systems::ArrowVisualStyle::Marker) {
      draw_arrow_glow(renderer,
                      arrow_shaft_mesh,
                      arrow_tip_mesh,
                      model,
                      Geom::Arrow::glow_color(arrow.color),
                      main_alpha * arrow.brightness);
    }
  }
}

} // namespace GL

} // namespace Render
