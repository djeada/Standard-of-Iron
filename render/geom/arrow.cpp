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

namespace {

constexpr float k_two_pi = 6.2831853F;

auto sample_profile(const std::vector<std::pair<float, float>>& stops,
                    float t) -> float {
  if (stops.empty()) {
    return 0.0F;
  }
  if (t <= stops.front().first) {
    return stops.front().second;
  }
  for (std::size_t i = 1; i < stops.size(); ++i) {
    if (t <= stops[i].first) {
      const auto& [prev_t, prev_v] = stops[i - 1];
      const auto& [next_t, next_v] = stops[i];
      float const span = std::max(1.0e-6F, next_t - prev_t);
      return prev_v + (next_v - prev_v) * ((t - prev_t) / span);
    }
  }
  return stops.back().second;
}

void add_swept_ring(std::vector<GL::Vertex>& verts,
                    float z,
                    float half_width,
                    float half_thickness,
                    float slope,
                    int segments,
                    float v_coord) {
  for (int i = 0; i < segments; ++i) {
    float const angle =
        (static_cast<float>(i) / static_cast<float>(segments)) * k_two_pi;
    float const cos_a = std::cos(angle);
    float const sin_a = std::sin(angle);
    QVector3D radial(cos_a * half_thickness, sin_a * half_width, 0.0F);
    if (radial.lengthSquared() < 1.0e-12F) {
      radial = QVector3D(cos_a, sin_a, 0.0F);
    }
    radial.normalize();
    QVector3D normal(radial.x(), radial.y(), slope);
    normal.normalize();
    verts.push_back({{cos_a * half_width, sin_a * half_thickness, z},
                     {normal.x(), normal.y(), normal.z()},
                     {static_cast<float>(i) / static_cast<float>(segments), v_coord}});
  }
}

void stitch_rings(std::vector<unsigned int>& idx,
                  unsigned int lower,
                  unsigned int upper,
                  int segments) {
  for (int i = 0; i < segments; ++i) {
    auto const next = static_cast<unsigned int>((i + 1) % segments);
    auto const step = static_cast<unsigned int>(i);
    idx.insert(idx.end(),
               {lower + step,
                lower + next,
                upper + next,
                upper + next,
                upper + step,
                lower + step});
  }
}

void add_ring_cap(std::vector<GL::Vertex>& verts,
                  std::vector<unsigned int>& idx,
                  unsigned int ring,
                  float z,
                  float normal_z,
                  int segments) {
  auto const center = static_cast<unsigned int>(verts.size());
  verts.push_back({{0.0F, 0.0F, z}, {0.0F, 0.0F, normal_z}, {0.5F, 0.5F}});
  for (int i = 0; i < segments; ++i) {
    auto const next = static_cast<unsigned int>((i + 1) % segments);
    auto const step = static_cast<unsigned int>(i);
    if (normal_z < 0.0F) {
      idx.insert(idx.end(), {center, ring + next, ring + step});
    } else {
      idx.insert(idx.end(), {center, ring + step, ring + next});
    }
  }
}

} // namespace

static auto create_arrow_shaft_mesh() -> std::unique_ptr<GL::Mesh> {
  std::vector<GL::Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr int k_segments = 10;
  constexpr int k_rings = 9;
  const float shaft_len = Arrow::k_shaft_length;
  const std::vector<std::pair<float, float>> radius_stops{
      {0.000F, Arrow::k_shaft_radius * 1.34F},
      {0.028F, Arrow::k_shaft_radius * 1.30F},
      {0.055F, Arrow::k_shaft_radius * 0.88F},
      {0.090F, Arrow::k_shaft_radius * 0.94F},
      {0.500F, Arrow::k_shaft_radius},
      {0.930F, Arrow::k_shaft_radius * 0.90F},
      {1.000F, Arrow::k_shaft_radius * 0.88F},
  };

  verts.reserve(static_cast<std::size_t>(k_rings) * k_segments + 2U);
  idx.reserve(static_cast<std::size_t>(k_rings) * k_segments * 6U);

  for (int ring = 0; ring < k_rings; ++ring) {
    float const t = static_cast<float>(ring) / static_cast<float>(k_rings - 1);
    float const z = t * shaft_len;
    float const radius = sample_profile(radius_stops, t);
    float const step = 1.0F / static_cast<float>(k_rings - 1);
    float const next_t = std::min(1.0F, t + step);
    float const slope = (radius - sample_profile(radius_stops, next_t)) /
                        std::max(1.0e-4F, (next_t - t) * shaft_len);
    add_swept_ring(verts, z, radius, radius, slope, k_segments, t);
  }
  for (int ring = 0; ring + 1 < k_rings; ++ring) {
    stitch_rings(idx,
                 static_cast<unsigned int>(ring * k_segments),
                 static_cast<unsigned int>((ring + 1) * k_segments),
                 k_segments);
  }

  add_ring_cap(verts, idx, 0U, 0.0F, -1.0F, k_segments);
  add_ring_cap(verts,
               idx,
               static_cast<unsigned int>((k_rings - 1) * k_segments),
               shaft_len,
               1.0F,
               k_segments);

  return std::make_unique<GL::Mesh>(verts, idx);
}

static auto create_arrow_tip_mesh() -> std::unique_ptr<GL::Mesh> {
  std::vector<GL::Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr int k_segments = 10;
  const float tip_start_z = Arrow::k_shaft_length;
  const float tip_len = Arrow::k_tip_length;

  const std::vector<std::pair<float, float>> width_stops{
      {0.00F, Arrow::k_ferrule_radius},
      {0.15F, Arrow::k_ferrule_radius},
      {0.20F, Arrow::k_shaft_radius * 0.98F},
      {0.26F, Arrow::k_shaft_radius * 0.94F},
      {0.34F, Arrow::k_head_half_width},
      {0.62F, Arrow::k_head_half_width * 0.52F},
      {1.00F, 0.0F},
  };
  const std::vector<std::pair<float, float>> thickness_stops{
      {0.00F, Arrow::k_ferrule_radius},
      {0.15F, Arrow::k_ferrule_radius},
      {0.20F, Arrow::k_shaft_radius * 0.98F},
      {0.26F, Arrow::k_shaft_radius * 0.94F},
      {0.34F, Arrow::k_head_half_thickness},
      {0.62F, Arrow::k_head_half_thickness * 0.70F},
      {1.00F, 0.0F},
  };
  const std::vector<float> ring_stops{
      0.0F, 0.15F, 0.20F, 0.26F, 0.34F, 0.50F, 0.68F, 0.84F, 0.955F};

  verts.reserve(ring_stops.size() * k_segments + 2U);
  idx.reserve(ring_stops.size() * k_segments * 6U);

  for (std::size_t ring = 0; ring < ring_stops.size(); ++ring) {
    float const s = ring_stops[ring];
    float const half_width = sample_profile(width_stops, s);
    float const half_thickness = sample_profile(thickness_stops, s);
    float const ahead = std::min(1.0F, s + 0.05F);
    float const slope = (half_width - sample_profile(width_stops, ahead)) /
                        std::max(1.0e-4F, (ahead - s) * tip_len);
    add_swept_ring(verts,
                   tip_start_z + s * tip_len,
                   half_width,
                   half_thickness,
                   slope,
                   k_segments,
                   s);
  }
  for (std::size_t ring = 0; ring + 1 < ring_stops.size(); ++ring) {
    stitch_rings(idx,
                 static_cast<unsigned int>(ring * k_segments),
                 static_cast<unsigned int>((ring + 1) * k_segments),
                 k_segments);
  }

  add_ring_cap(verts, idx, 0U, tip_start_z, -1.0F, k_segments);

  auto const last_ring =
      static_cast<unsigned int>((ring_stops.size() - 1) * k_segments);
  auto const apex = static_cast<unsigned int>(verts.size());
  verts.push_back(
      {{0.0F, 0.0F, tip_start_z + tip_len}, {0.0F, 0.0F, 1.0F}, {0.5F, 1.0F}});
  for (int i = 0; i < k_segments; ++i) {
    auto const next = static_cast<unsigned int>((i + 1) % k_segments);
    idx.insert(idx.end(),
               {last_ring + static_cast<unsigned int>(i), last_ring + next, apex});
  }

  return std::make_unique<GL::Mesh>(verts, idx);
}

static auto create_arrow_fletching_mesh() -> std::unique_ptr<GL::Mesh> {
  using GL::Vertex;
  std::vector<Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr int k_span_steps = 13;
  constexpr float k_half_thickness = 0.0024F;
  const float start_z = Arrow::k_fletch_start_z;
  const float end_z = Arrow::k_fletch_end_z;
  const float span = end_z - start_z;
  const std::vector<std::pair<float, float>> height_stops{
      {0.00F, Arrow::k_shaft_radius * 1.00F},
      {0.05F, Arrow::k_fletch_peak_radius * 0.30F},
      {0.13F, Arrow::k_fletch_peak_radius * 0.58F},
      {0.24F, Arrow::k_fletch_peak_radius * 0.79F},
      {0.40F, Arrow::k_fletch_peak_radius * 0.91F},
      {0.62F, Arrow::k_fletch_peak_radius * 0.98F},
      {0.82F, Arrow::k_fletch_peak_radius},
      {0.93F, Arrow::k_fletch_peak_radius * 0.90F},
      {0.97F, Arrow::k_fletch_peak_radius * 0.52F},
      {1.00F, Arrow::k_shaft_radius * 1.90F},
  };

  verts.reserve(static_cast<std::size_t>(Arrow::k_fletch_vanes) * k_span_steps * 3U);
  idx.reserve(static_cast<std::size_t>(Arrow::k_fletch_vanes) * k_span_steps * 12U);

  auto add_vane = [&](float base_angle_deg) {
    auto const base = static_cast<unsigned int>(verts.size());
    for (int step = 0; step < k_span_steps; ++step) {
      float const t = static_cast<float>(step) / static_cast<float>(k_span_steps - 1);
      float const angle_deg = base_angle_deg + (Arrow::k_fletch_helical_deg * t);
      float const angle = angle_deg * std::numbers::pi_v<float> / 180.0F;
      QVector3D const outward(std::cos(angle), std::sin(angle), 0.0F);
      QVector3D const side(-std::sin(angle), std::cos(angle), 0.0F);
      float const z = start_z + (span * t);
      float const inner = Arrow::k_shaft_radius * 0.72F;
      float const outer = sample_profile(height_stops, t);
      QVector3D const root = outward * inner;
      QVector3D const edge = outward * outer;
      QVector3D const face_normal =
          (side + (outward * 0.18F) + QVector3D(0.0F, 0.0F, 0.10F)).normalized();

      verts.push_back({{root.x() + side.x() * k_half_thickness,
                        root.y() + side.y() * k_half_thickness,
                        z},
                       {face_normal.x(), face_normal.y(), face_normal.z()},
                       {t, 0.0F}});
      verts.push_back({{root.x() - side.x() * k_half_thickness,
                        root.y() - side.y() * k_half_thickness,
                        z},
                       {-face_normal.x(), -face_normal.y(), -face_normal.z()},
                       {t, 0.0F}});
      verts.push_back(
          {{edge.x(), edge.y(), z}, {outward.x(), outward.y(), 0.0F}, {t, 1.0F}});
    }
    for (int step = 0; step + 1 < k_span_steps; ++step) {
      auto const a = base + static_cast<unsigned int>(step * 3);
      auto const b = base + static_cast<unsigned int>((step + 1) * 3);
      idx.insert(idx.end(), {a, b, b + 2U, b + 2U, a + 2U, a});
      idx.insert(idx.end(), {a + 1U, a + 2U, b + 2U, b + 2U, b + 1U, a + 1U});
    }
  };

  for (int vane = 0; vane < Arrow::k_fletch_vanes; ++vane) {
    add_vane(360.0F * static_cast<float>(vane) /
             static_cast<float>(Arrow::k_fletch_vanes));
  }

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

void draw_arrow_glow(Renderer* renderer,
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
    renderer->mesh(arrow_fletching_mesh, fletch_model, fletch_color, nullptr, alpha);
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
