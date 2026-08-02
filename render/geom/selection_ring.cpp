#include "selection_ring.h"

#include <QVector3D>
#include <qvectornd.h>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "gl/mesh.h"

namespace Render::Geom {

std::array<std::unique_ptr<Render::GL::Mesh>, Game::Accessibility::k_team_pattern_count>
    SelectionRing::s_meshes;

namespace {

using Game::Accessibility::TeamPattern;

constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;
constexpr int k_segments_per_turn = 48;
constexpr float k_inner_radius = 0.94F;
constexpr float k_outer_radius = 1.0F;

void append_arc(std::vector<Render::GL::Vertex>& verts,
                std::vector<unsigned int>& idx,
                float start_angle,
                float sweep,
                float inner,
                float outer) {
  const int segments =
      std::max(1, static_cast<int>(std::round(sweep / k_two_pi * k_segments_per_turn)));
  const QVector3D normal(0, 1, 0);

  for (int i = 0; i < segments; ++i) {
    const float a0 = start_angle + sweep * (static_cast<float>(i) / segments);
    const float a1 = start_angle + sweep * (static_cast<float>(i + 1) / segments);
    const std::size_t base = verts.size();

    verts.push_back({{inner * std::cos(a0), 0.0F, inner * std::sin(a0)},
                     {normal.x(), normal.y(), normal.z()},
                     {0, 0}});
    verts.push_back({{outer * std::cos(a0), 0.0F, outer * std::sin(a0)},
                     {normal.x(), normal.y(), normal.z()},
                     {1, 0}});
    verts.push_back({{outer * std::cos(a1), 0.0F, outer * std::sin(a1)},
                     {normal.x(), normal.y(), normal.z()},
                     {1, 1}});
    verts.push_back({{inner * std::cos(a1), 0.0F, inner * std::sin(a1)},
                     {normal.x(), normal.y(), normal.z()},
                     {0, 1}});

    idx.push_back(static_cast<unsigned int>(base + 0));
    idx.push_back(static_cast<unsigned int>(base + 2));
    idx.push_back(static_cast<unsigned int>(base + 1));
    idx.push_back(static_cast<unsigned int>(base + 2));
    idx.push_back(static_cast<unsigned int>(base + 0));
    idx.push_back(static_cast<unsigned int>(base + 3));
  }
}

void append_dashes(std::vector<Render::GL::Vertex>& verts,
                   std::vector<unsigned int>& idx,
                   int dash_count,
                   float duty,
                   float inner,
                   float outer) {
  const float slice = k_two_pi / static_cast<float>(dash_count);
  for (int i = 0; i < dash_count; ++i) {
    append_arc(verts, idx, static_cast<float>(i) * slice, slice * duty, inner, outer);
  }
}

void append_ticks(std::vector<Render::GL::Vertex>& verts,
                  std::vector<unsigned int>& idx,
                  int tick_count,
                  float length) {
  const float slice = k_two_pi / static_cast<float>(tick_count);
  const float half_width = slice * 0.10F;
  for (int i = 0; i < tick_count; ++i) {
    append_arc(verts,
               idx,
               static_cast<float>(i) * slice - half_width,
               half_width * 2.0F,
               k_outer_radius,
               k_outer_radius + length);
  }
}

auto create_pattern_mesh(TeamPattern pattern) -> std::unique_ptr<Render::GL::Mesh> {
  std::vector<Render::GL::Vertex> verts;
  std::vector<unsigned int> idx;

  switch (pattern) {
  case TeamPattern::Dashed:
    append_dashes(verts, idx, 8, 0.62F, k_inner_radius, k_outer_radius);
    break;
  case TeamPattern::DoubleRing:
    append_arc(verts, idx, 0.0F, k_two_pi, k_inner_radius, k_outer_radius);
    append_arc(verts, idx, 0.0F, k_two_pi, 0.72F, 0.78F);
    break;
  case TeamPattern::Notched:

    append_dashes(verts, idx, 4, 0.88F, k_inner_radius, k_outer_radius);
    break;
  case TeamPattern::Dotted:
    append_dashes(verts, idx, 16, 0.38F, k_inner_radius, k_outer_radius);
    break;
  case TeamPattern::Chevron:
    append_arc(verts, idx, 0.0F, k_two_pi, k_inner_radius, k_outer_radius);
    append_ticks(verts, idx, 4, 0.16F);
    break;
  case TeamPattern::Solid:
  default:
    append_arc(verts, idx, 0.0F, k_two_pi, k_inner_radius, k_outer_radius);
    break;
  }

  return std::make_unique<Render::GL::Mesh>(verts, idx);
}

} // namespace

auto SelectionRing::get(TeamPattern pattern) -> Render::GL::Mesh* {
  const auto index = static_cast<std::size_t>(pattern);
  if (index >= s_meshes.size()) {
    return get(TeamPattern::Solid);
  }
  if (!s_meshes[index]) {
    s_meshes[index] = create_pattern_mesh(pattern);
  }
  return s_meshes[index].get();
}

auto SelectionRing::get() -> Render::GL::Mesh* {
  return get(TeamPattern::Solid);
}

} // namespace Render::Geom
