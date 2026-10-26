#pragma once

#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <utility>
#include <vector>

#include "render/gl/mesh.h"

namespace Render::CreatureMesh {

struct LoopRefineOptions {
  int levels{1};
  float volume_keep{1.0F};
  int bone_count{32};

  float sole_band{-1.0F};

  QVector4D height_plane{0.0F, 1.0F, 0.0F, 0.0F};
};

namespace detail {

struct SkinnedPoint {
  QVector3D position;
  std::array<float, 2> uv{};
  std::vector<float> weights;
};

inline auto loop_beta(std::size_t valence) -> float {
  if (valence == 3U) {
    return 3.0F / 16.0F;
  }
  return 3.0F / (8.0F * static_cast<float>(valence));
}

struct Topology {
  std::vector<std::vector<std::uint32_t>> neighbours;
  std::vector<bool> on_boundary;
  std::vector<bool> pinned;
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::vector<std::uint32_t>>
      edge_opposites;
};

inline auto edge_key(std::uint32_t a,
                     std::uint32_t b) -> std::pair<std::uint32_t, std::uint32_t> {
  return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
}

inline auto build_topology(std::size_t count,
                           const std::vector<std::uint32_t>& tris) -> Topology {
  Topology topo;
  topo.neighbours.resize(count);
  topo.on_boundary.assign(count, false);
  topo.pinned.assign(count, false);
  for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
    for (int e = 0; e < 3; ++e) {
      std::uint32_t const a = tris[i + e];
      std::uint32_t const b = tris[i + (e + 1) % 3];
      std::uint32_t const c = tris[i + (e + 2) % 3];
      topo.edge_opposites[edge_key(a, b)].push_back(c);
    }
  }
  for (auto const& [edge, opposite] : topo.edge_opposites) {
    topo.neighbours[edge.first].push_back(edge.second);
    topo.neighbours[edge.second].push_back(edge.first);
    if (opposite.size() != 2U) {
      topo.on_boundary[edge.first] = true;
      topo.on_boundary[edge.second] = true;
    }
  }
  return topo;
}

template <typename Mix>
void loop_vertex_stencil(const Topology& topo, std::uint32_t v, Mix&& mix) {
  auto const& ring = topo.neighbours[v];
  if (ring.empty() || topo.pinned[v]) {
    mix(v, 1.0F);
    return;
  }
  if (topo.on_boundary[v]) {
    std::vector<std::uint32_t> rim;
    for (std::uint32_t n : ring) {
      auto const it = topo.edge_opposites.find(edge_key(v, n));
      if (it != topo.edge_opposites.end() && it->second.size() != 2U) {
        rim.push_back(n);
      }
    }
    if (rim.size() == 2U) {
      mix(v, 0.75F);
      mix(rim[0], 0.125F);
      mix(rim[1], 0.125F);
    } else {
      mix(v, 1.0F);
    }
    return;
  }
  float const beta = loop_beta(ring.size());
  mix(v, 1.0F - beta * static_cast<float>(ring.size()));
  for (std::uint32_t n : ring) {
    mix(n, beta);
  }
}

inline auto blend_points(const std::vector<SkinnedPoint>& points,
                         const std::vector<std::pair<std::uint32_t, float>>& terms,
                         int bone_count) -> SkinnedPoint {
  SkinnedPoint out;
  out.weights.assign(static_cast<std::size_t>(bone_count), 0.0F);
  for (auto const& [index, w] : terms) {
    auto const& p = points[index];
    out.position += p.position * w;
    out.uv[0] += p.uv[0] * w;
    out.uv[1] += p.uv[1] * w;
    for (std::size_t b = 0; b < out.weights.size(); ++b) {
      out.weights[b] += p.weights[b] * w;
    }
  }
  return out;
}

inline void subdivide_once(std::vector<SkinnedPoint>& points,
                           std::vector<std::uint32_t>& tris,
                           std::vector<bool>& pinned,
                           int bone_count) {
  Topology topo = build_topology(points.size(), tris);
  topo.pinned = pinned;
  std::vector<SkinnedPoint> next;
  next.reserve(points.size() + topo.edge_opposites.size());
  for (std::uint32_t v = 0; v < points.size(); ++v) {
    std::vector<std::pair<std::uint32_t, float>> terms;
    loop_vertex_stencil(
        topo, v, [&](std::uint32_t i, float w) { terms.emplace_back(i, w); });
    next.push_back(blend_points(points, terms, bone_count));
  }
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> edge_vertex;
  for (auto const& [edge, opposite] : topo.edge_opposites) {
    std::vector<std::pair<std::uint32_t, float>> terms;
    bool const crease = pinned[edge.first] && pinned[edge.second];
    if (opposite.size() == 2U && !crease) {
      terms = {{edge.first, 0.375F},
               {edge.second, 0.375F},
               {opposite[0], 0.125F},
               {opposite[1], 0.125F}};
    } else {
      terms = {{edge.first, 0.5F}, {edge.second, 0.5F}};
    }
    edge_vertex[edge] = static_cast<std::uint32_t>(next.size());
    next.push_back(blend_points(points, terms, bone_count));
    pinned.push_back(crease);
  }
  std::vector<std::uint32_t> refined;
  refined.reserve(tris.size() * 4U);
  for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
    std::uint32_t const a = tris[i];
    std::uint32_t const b = tris[i + 1];
    std::uint32_t const c = tris[i + 2];
    std::uint32_t const ab = edge_vertex[edge_key(a, b)];
    std::uint32_t const bc = edge_vertex[edge_key(b, c)];
    std::uint32_t const ca = edge_vertex[edge_key(c, a)];
    refined.insert(refined.end(), {a, ab, ca, ab, b, bc, ca, bc, c, ab, bc, ca});
  }
  points = std::move(next);
  tris = std::move(refined);
}

inline void preserve_volume(std::vector<SkinnedPoint>& points,
                            const std::vector<std::uint32_t>& tris,
                            const std::vector<bool>& pinned,
                            float amount) {
  if (amount <= 0.0F) {
    return;
  }
  Topology const topo = build_topology(points.size(), tris);
  std::vector<QVector3D> target(points.size());
  for (std::size_t v = 0; v < points.size(); ++v) {
    target[v] = points[v].position;
  }
  auto limit_of = [&](std::uint32_t v) -> QVector3D {
    auto const& ring = topo.neighbours[v];
    if (topo.on_boundary[v]) {
      std::vector<std::uint32_t> rim;
      for (std::uint32_t n : ring) {
        auto const it = topo.edge_opposites.find(edge_key(v, n));
        if (it != topo.edge_opposites.end() && it->second.size() != 2U) {
          rim.push_back(n);
        }
      }
      if (rim.size() != 2U) {
        return points[v].position;
      }
      return points[v].position * (2.0F / 3.0F) +
             (points[rim[0]].position + points[rim[1]].position) / 6.0F;
    }
    float const n = static_cast<float>(ring.size());
    float const chi = 1.0F / (3.0F / (8.0F * loop_beta(ring.size())) + n);
    QVector3D limit = points[v].position * (1.0F - n * chi);
    for (std::uint32_t r : ring) {
      limit += points[r].position * chi;
    }
    return limit;
  };
  constexpr int k_iterations = 4;
  std::vector<QVector3D> moved(points.size());
  for (int iteration = 0; iteration < k_iterations; ++iteration) {
    for (std::uint32_t v = 0; v < points.size(); ++v) {
      moved[v] = points[v].position;
      if (topo.neighbours[v].empty() || pinned[v]) {
        continue;
      }
      moved[v] += (target[v] - limit_of(v)) * amount;
    }
    for (std::uint32_t v = 0; v < points.size(); ++v) {
      points[v].position = moved[v];
    }
  }
}

} // namespace detail

inline void loop_refine_skinned_mesh(std::vector<Render::GL::Vertex>& vertices,
                                     std::vector<unsigned int>& indices,
                                     const LoopRefineOptions& options) {
  using detail::SkinnedPoint;
  if (vertices.empty() || indices.size() < 3U || options.levels < 0) {
    return;
  }
  auto const bones = static_cast<std::size_t>(options.bone_count);
  std::uint8_t const color_role = vertices.front().color_role;
  std::map<std::array<long, 3>, std::uint32_t> weld;
  std::vector<SkinnedPoint> points;
  std::vector<std::uint32_t> remap(vertices.size());
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    auto const& v = vertices[i];
    std::array<long, 3> const key{std::lround(v.position[0] * 1.0e5F),
                                  std::lround(v.position[1] * 1.0e5F),
                                  std::lround(v.position[2] * 1.0e5F)};
    auto const found = weld.find(key);
    if (found != weld.end()) {
      remap[i] = found->second;
      continue;
    }
    SkinnedPoint p;
    p.position = QVector3D(v.position[0], v.position[1], v.position[2]);
    p.uv = {v.tex_coord[0], v.tex_coord[1]};
    p.weights.assign(bones, 0.0F);
    for (std::size_t k = 0; k < 4U; ++k) {
      if (v.bone_indices[k] < bones) {
        p.weights[v.bone_indices[k]] += v.bone_weights[k];
      }
    }
    remap[i] = static_cast<std::uint32_t>(points.size());
    weld.emplace(key, remap[i]);
    points.push_back(std::move(p));
  }
  std::vector<std::uint32_t> tris;
  tris.reserve(indices.size());
  for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
    std::uint32_t const a = remap[indices[i]];
    std::uint32_t const b = remap[indices[i + 1]];
    std::uint32_t const c = remap[indices[i + 2]];
    if (a == b || b == c || c == a) {
      continue;
    }
    tris.insert(tris.end(), {a, b, c});
  }

  std::vector<bool> pinned(points.size(), false);
  if (options.sole_band >= 0.0F) {
    auto height = [&options](const QVector3D& p) {
      return QVector3D::dotProduct(options.height_plane.toVector3D(), p) +
             options.height_plane.w();
    };
    float lowest = height(points.front().position);
    for (auto const& p : points) {
      lowest = std::min(lowest, height(p.position));
    }
    for (std::size_t i = 0; i < points.size(); ++i) {
      pinned[i] = height(points[i].position) <= lowest + options.sole_band;
    }
  }
  if (options.levels > 0) {
    detail::preserve_volume(points, tris, pinned, options.volume_keep);
  }
  for (int level = 0; level < options.levels; ++level) {
    detail::subdivide_once(points, tris, pinned, options.bone_count);
  }

  std::vector<QVector3D> normals(points.size());
  for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
    QVector3D const& a = points[tris[i]].position;
    QVector3D const& b = points[tris[i + 1]].position;
    QVector3D const& c = points[tris[i + 2]].position;
    QVector3D const face = QVector3D::crossProduct(b - a, c - a);
    normals[tris[i]] += face;
    normals[tris[i + 1]] += face;
    normals[tris[i + 2]] += face;
  }

  vertices.clear();
  vertices.reserve(points.size());
  for (std::size_t i = 0; i < points.size(); ++i) {
    auto const& p = points[i];
    Render::GL::Vertex v{};
    v.position = {p.position.x(), p.position.y(), p.position.z()};
    QVector3D const n = normals[i].lengthSquared() > 1.0e-20F
                            ? normals[i].normalized()
                            : QVector3D(0.0F, 1.0F, 0.0F);
    v.normal = {n.x(), n.y(), n.z()};
    v.tex_coord = {p.uv[0], p.uv[1]};
    v.color_role = color_role;
    std::array<std::pair<float, std::uint8_t>, 4> top{};
    for (std::size_t b = 0; b < p.weights.size(); ++b) {
      float const w = p.weights[b];
      auto const weakest =
          std::min_element(top.begin(), top.end(), [](auto const& l, auto const& r) {
            return l.first < r.first;
          });
      if (w > weakest->first) {
        *weakest = {w, static_cast<std::uint8_t>(b)};
      }
    }
    std::sort(top.begin(), top.end(), [](auto const& l, auto const& r) {
      return l.first > r.first;
    });
    float sum = 0.0F;
    for (auto const& t : top) {
      sum += std::max(t.first, 0.0F);
    }
    for (std::size_t k = 0; k < 4U; ++k) {
      float const w =
          sum > 1.0e-6F ? std::max(top[k].first, 0.0F) / sum : (k == 0 ? 1.0F : 0.0F);
      v.bone_indices[k] = w > 0.0F ? top[k].second : static_cast<std::uint8_t>(0U);
      v.bone_weights[k] = w;
    }
    vertices.push_back(v);
  }
  indices.assign(tris.begin(), tris.end());
}

} // namespace Render::CreatureMesh
