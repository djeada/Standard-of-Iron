#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <memory>
#include <numbers>
#include <vector>

#include "render/gl/mesh.h"

namespace Render::GL {

struct GarmentSection {
  float y;
  float width;
  float depth;
};

inline auto make_garment_shell(std::initializer_list<GarmentSection> sections)
    -> std::unique_ptr<Mesh> {
  std::vector<GarmentSection> const profile(sections);
  if (profile.size() < 2U) {
    return nullptr;
  }
  constexpr int radial = 32;
  constexpr int subdivisions = 6;
  auto slope = [&](std::size_t i, float GarmentSection::*field) {
    if (i == 0U) {
      return (profile[1].*field - profile[0].*field) / (profile[1].y - profile[0].y);
    }
    if (i + 1U == profile.size()) {
      return (profile[i].*field - profile[i - 1U].*field) /
             (profile[i].y - profile[i - 1U].y);
    }
    float const a = (profile[i].*field - profile[i - 1U].*field) /
                    (profile[i].y - profile[i - 1U].y);
    float const b = (profile[i + 1U].*field - profile[i].*field) /
                    (profile[i + 1U].y - profile[i].y);
    return a * b <= 0.0F ? 0.0F : 2.0F * a * b / (a + b);
  };
  auto sample = [&](std::size_t i, float t, float GarmentSection::*field) {
    float const h = profile[i + 1U].y - profile[i].y;
    float const t2 = t * t;
    float const t3 = t2 * t;
    return (2.0F * t3 - 3.0F * t2 + 1.0F) * (profile[i].*field) +
           (t3 - 2.0F * t2 + t) * h * slope(i, field) +
           (-2.0F * t3 + 3.0F * t2) * (profile[i + 1U].*field) +
           (t3 - t2) * h * slope(i + 1U, field);
  };

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  auto const rings =
      static_cast<unsigned int>((profile.size() - 1U) * subdivisions + 1U);
  vertices.reserve(rings * radial);
  indices.reserve((rings - 1U) * radial * 6U);
  for (unsigned int ring = 0; ring < rings; ++ring) {
    std::size_t const segment =
        std::min<std::size_t>(ring / subdivisions, profile.size() - 2U);
    float const t = static_cast<float>(ring - segment * subdivisions) / subdivisions;
    float const y =
        profile[segment].y + t * (profile[segment + 1U].y - profile[segment].y);
    float const w = sample(segment, t, &GarmentSection::width);
    float const d = sample(segment, t, &GarmentSection::depth);
    for (int j = 0; j < radial; ++j) {
      float const u = static_cast<float>(j) / radial;
      float const angle = u * 2.0F * std::numbers::pi_v<float>;
      vertices.push_back({{w * std::cos(angle), y, d * std::sin(angle)},
                          {},
                          {u, static_cast<float>(ring) / (rings - 1U)}});
    }
  }
  for (unsigned int ring = 0; ring + 1U < rings; ++ring) {
    for (unsigned int j = 0; j < radial; ++j) {
      unsigned int const a = ring * radial + j;
      unsigned int const b = ring * radial + (j + 1U) % radial;
      indices.insert(indices.end(), {a, a + radial, b, b, a + radial, b + radial});
    }
  }
  std::vector<QVector3D> normals(vertices.size());
  for (std::size_t i = 0; i < indices.size(); i += 3U) {
    auto point = [&](unsigned int index) {
      auto const& p = vertices[index].position;
      return QVector3D(p[0], p[1], p[2]);
    };
    QVector3D const normal =
        QVector3D::crossProduct(point(indices[i + 1U]) - point(indices[i]),
                                point(indices[i + 2U]) - point(indices[i]));
    for (std::size_t k = 0; k < 3U; ++k) {
      normals[indices[i + k]] += normal;
    }
  }
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    QVector3D const n = normals[i].normalized();
    vertices[i].normal = {n.x(), n.y(), n.z()};
  }
  return std::make_unique<Mesh>(vertices, indices);
}

} // namespace Render::GL
