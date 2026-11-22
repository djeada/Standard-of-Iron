#include "render/geom/arrow.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "render/gl/mesh.h"
#include "render/gl/shared_geometry_cache.h"

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
  return GL::SharedGeometryCache::instance().get_or_build(
      GL::geometry_key("geom/arrow/shaft"), create_arrow_shaft_mesh);
}

auto Arrow::get_tip() -> GL::Mesh* {
  return GL::SharedGeometryCache::instance().get_or_build(
      GL::geometry_key("geom/arrow/tip"), create_arrow_tip_mesh);
}

auto Arrow::get_fletching() -> GL::Mesh* {
  return GL::SharedGeometryCache::instance().get_or_build(
      GL::geometry_key("geom/arrow/fletching"), create_arrow_fletching_mesh);
}

} // namespace Geom
} // namespace Render
