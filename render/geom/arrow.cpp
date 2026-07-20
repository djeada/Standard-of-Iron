#include "arrow.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <qmatrix4x4.h>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "../../game/systems/arrow_system.h"
#include "../entity/registry.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"

namespace Render {
namespace Geom {

static auto create_arrow_shaft_mesh() -> std::unique_ptr<GL::Mesh> {
  using GL::Vertex;
  std::vector<GL::Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr int k_arrow_radial_segments = 12;
  const float shaft_radius = 0.027F;
  const float shaft_len = Arrow::k_shaft_length;

  int const base_index = 0;
  for (int ring = 0; ring < 2; ++ring) {
    float z = (ring == 0) ? 0.0F : shaft_len;
    for (int i = 0; i < k_arrow_radial_segments; ++i) {
      float const a = (float(i) / k_arrow_radial_segments) * 6.2831853F;
      float x = std::cos(a) * shaft_radius;
      float y = std::sin(a) * shaft_radius;
      QVector3D n(x, y, 0.0F);
      n.normalize();
      verts.push_back(
          {{x, y, z}, {n.x(), n.y(), n.z()}, {float(i) / k_arrow_radial_segments, z}});
    }
  }

  for (int i = 0; i < k_arrow_radial_segments; ++i) {
    int const next = (i + 1) % k_arrow_radial_segments;
    int a = base_index + i;
    int b = base_index + next;
    int c = base_index + k_arrow_radial_segments + next;
    int d = base_index + k_arrow_radial_segments + i;
    idx.push_back(a);
    idx.push_back(b);
    idx.push_back(c);
    idx.push_back(c);
    idx.push_back(d);
    idx.push_back(a);
  }

  return std::make_unique<GL::Mesh>(verts, idx);
}

static auto create_arrow_tip_mesh() -> std::unique_ptr<GL::Mesh> {
  using GL::Vertex;
  std::vector<GL::Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr int k_arrow_radial_segments = 12;
  const float shaft_radius = 0.027F;
  const float shaft_len = Arrow::k_shaft_length;
  const float tip_len = Arrow::k_tip_length;
  const float tip_start_z = shaft_len;
  const float tip_end_z = shaft_len + tip_len;
  const float tip_base_radius = shaft_radius * Arrow::k_tip_base_radius_scale;

  int const ring_start = 0;
  for (int i = 0; i < k_arrow_radial_segments; ++i) {
    float const a = (float(i) / k_arrow_radial_segments) * 6.2831853F;
    float x = std::cos(a) * tip_base_radius;
    float y = std::sin(a) * tip_base_radius;
    QVector3D n(x, y, 0.32F);
    n.normalize();
    verts.push_back({{x, y, tip_start_z},
                     {n.x(), n.y(), n.z()},
                     {float(i) / k_arrow_radial_segments, 0.0F}});
  }

  int apex_index = verts.size();
  verts.push_back({{0.0F, 0.0F, tip_end_z}, {0.0F, 0.0F, 1.0F}, {0.5F, 1.0F}});
  for (int i = 0; i < k_arrow_radial_segments; ++i) {
    int const next = (i + 1) % k_arrow_radial_segments;
    idx.push_back(ring_start + i);
    idx.push_back(apex_index);
    idx.push_back(ring_start + next);
  }

  return std::make_unique<GL::Mesh>(verts, idx);
}

static auto create_arrow_fletching_mesh() -> std::unique_ptr<GL::Mesh> {
  using GL::Vertex;
  constexpr float k_root_z = 0.035F;
  constexpr float k_tail_z = 0.19F;
  constexpr float k_fin_height = 0.105F;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;
  verts.reserve(16);
  idx.reserve(24);

  auto add_fin = [&](QVector3D normal, QVector3D edge) {
    auto vertex =
        [](float x, float y, float z, const QVector3D& normal_value, float u, float v)
        -> Vertex {
      return {
          {x, y, z}, {normal_value.x(), normal_value.y(), normal_value.z()}, {u, v}};
    };
    auto const base = static_cast<unsigned int>(verts.size());
    verts.push_back(vertex(0.0F, 0.0F, k_root_z, normal, 0.0F, 0.0F));
    verts.push_back(vertex(edge.x(), edge.y(), k_tail_z, normal, 1.0F, 0.0F));
    verts.push_back(
        vertex(edge.x() * 0.72F, edge.y() * 0.72F, k_root_z, normal, 1.0F, 1.0F));
    QVector3D const back_normal = -normal;
    verts.push_back(vertex(0.0F, 0.0F, k_root_z, back_normal, 0.0F, 0.0F));
    verts.push_back(
        vertex(edge.x() * 0.72F, edge.y() * 0.72F, k_root_z, back_normal, 1.0F, 1.0F));
    verts.push_back(vertex(edge.x(), edge.y(), k_tail_z, back_normal, 1.0F, 0.0F));
    idx.insert(idx.end(),
               {base, base + 1U, base + 2U, base + 3U, base + 4U, base + 5U});
  };
  add_fin({0.0F, 1.0F, 0.0F}, {k_fin_height, 0.0F, 0.0F});
  add_fin({1.0F, 0.0F, 0.0F}, {0.0F, k_fin_height, 0.0F});
  return std::make_unique<GL::Mesh>(verts, idx);
}

auto Arrow::get_shaft() -> GL::Mesh* {
  static std::unique_ptr<GL::Mesh> const mesh = create_arrow_shaft_mesh();
  return mesh.get();
}

auto Arrow::get_tip() -> GL::Mesh* {
  static std::unique_ptr<GL::Mesh> const mesh = create_arrow_tip_mesh();
  return mesh.get();
}

auto Arrow::get_fletching() -> GL::Mesh* {
  static std::unique_ptr<GL::Mesh> const mesh = create_arrow_fletching_mesh();
  return mesh.get();
}

} // namespace Geom

namespace GL {

namespace {

constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;

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
  case Game::Systems::ArrowVisualStyle::Volley:
    return 2;
  case Game::Systems::ArrowVisualStyle::Focused:
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
              arrow_z_scale * final_scale);
  return model;
}

void draw_arrow_mesh(Renderer* renderer,
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
    renderer->mesh(
        arrow_fletching_mesh, fletch_model, fletch_color, nullptr, alpha * 0.92F);
  }
}

} // namespace

void render_arrows(Renderer* renderer,
                   ResourceManager* resources,
                   const Game::Systems::ArrowSystem& arrow_system) {
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

  const auto& arrows = arrow_system.arrows();
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
      float const tail_t = std::max(0.0F, clamped_t - arrow.trail_length);
      if (tail_t < clamped_t) {
      }

      int const trail_segments = trail_segments_for_style(arrow.style);
      for (int segment = 1; segment <= trail_segments; ++segment) {
        float const segment_t =
            clamped_t - (arrow.trail_length * (0.45F + (0.38F * segment)));
        if (segment_t <= 0.0F) {
          continue;
        }

        QVector3D const ghost_pos = sample_arrow_position(arrow, segment_t);
        float const ghost_alpha =
            arrow.trail_alpha * main_alpha * (0.9F - (0.22F * segment));
        QMatrix4x4 const ghost_model =
            build_arrow_model(arrow, segment_t, ghost_pos, 0.92F - (0.08F * segment));
        draw_arrow_mesh(renderer,
                        arrow_shaft_mesh,
                        arrow_tip_mesh,
                        arrow_fletching_mesh,
                        ghost_model,
                        scaled_color(shaft_color, 0.78F),
                        scaled_color(tip_color, 0.82F),
                        scaled_color(fletch_color, 0.75F),
                        ghost_alpha,
                        segment == 1,
                        false);
      }
    }

    QMatrix4x4 const model = build_arrow_model(arrow, clamped_t, pos, 1.0F);
    QMatrix4x4 glow_model = model;
    glow_model.scale(2.45F, 2.45F, 1.08F);
    renderer->mesh(arrow_shaft_mesh,
                   glow_model,
                   scaled_color(fletch_color + QVector3D(0.34F, 0.34F, 0.28F), 0.94F),
                   nullptr,
                   main_alpha * 0.30F);
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
                    arrow.style != Game::Systems::ArrowVisualStyle::Marker);
  }
}

} // namespace GL
} // namespace Render
