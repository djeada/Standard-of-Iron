#include "rock_outcrop_mesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

using Render::GL::BackendPipelines::PropMeshData;
using Render::GL::BackendPipelines::RockMass;
using Render::GL::BackendPipelines::RockShard;
using V3 = QVector3D;

constexpr float k_tau = 6.28318530F;

auto hash01(int a, int b, int c) -> float {
  uint32_t hash = static_cast<uint32_t>(a + 1) * 0x9E3779B9U ^
                  static_cast<uint32_t>(b + 1) * 0x85EBCA6BU ^
                  static_cast<uint32_t>(c + 1) * 0xC2B2AE35U;
  hash ^= hash >> 15;
  hash *= 0x27D4EB2FU;
  hash ^= hash >> 13;
  return static_cast<float>(hash & 0xFFFFU) / 65535.0F;
}

auto signed01(int a, int b, int c) -> float {
  return (hash01(a, b, c) - 0.5F) * 2.0F;
}

struct Facets {
  PropMeshData mesh;

  void tri(const V3& a, const V3& b, const V3& c) {
    V3 normal = QVector3D::crossProduct(c - a, b - a);
    if (normal.lengthSquared() > 1.0e-8F) {
      normal.normalize();
    } else {
      normal = V3(0.0F, 1.0F, 0.0F);
    }
    auto const base = static_cast<uint16_t>(mesh.vertices.size());
    mesh.vertices.push_back({a, normal});
    mesh.vertices.push_back({b, normal});
    mesh.vertices.push_back({c, normal});
    mesh.indices.push_back(base);
    mesh.indices.push_back(static_cast<uint16_t>(base + 2));
    mesh.indices.push_back(static_cast<uint16_t>(base + 1));
  }

  void quad(const V3& a, const V3& b, const V3& c, const V3& d) {
    if ((c - a).lengthSquared() <= (d - b).lengthSquared()) {
      tri(a, b, c);
      tri(a, c, d);
    } else {
      tri(a, b, d);
      tri(b, c, d);
    }
  }
};

constexpr int k_rock_sides = 10;
constexpr int k_rock_rings = 5;
constexpr float k_rock_sink = 0.09F;
constexpr std::array<float, k_rock_rings> k_rock_profile{
    0.94F, 1.06F, 1.00F, 0.82F, 0.55F};
constexpr std::array<float, k_rock_rings> k_rock_station{
    0.00F, 0.22F, 0.48F, 0.74F, 0.92F};

void append_rock(Facets& out, const RockMass& rock) {
  std::array<std::array<V3, k_rock_sides>, k_rock_rings> ring{};
  for (int ri = 0; ri < k_rock_rings; ++ri) {
    float const t = k_rock_station[static_cast<std::size_t>(ri)];
    float const profile = k_rock_profile[static_cast<std::size_t>(ri)];
    for (int i = 0; i < k_rock_sides; ++i) {
      float const step = k_tau / static_cast<float>(k_rock_sides);
      float const angle = static_cast<float>(i) * step +
                          signed01(rock.seed, ri, i * 3 + 1) * step * 0.42F;
      float const swell = 1.0F + 0.16F * signed01(rock.seed, ri, i * 3 + 2);
      float const y = rock.base.y() + t * rock.height +
                      0.035F * rock.height * signed01(rock.seed, ri, i * 3 + 3);
      ring[static_cast<std::size_t>(ri)][static_cast<std::size_t>(i)] =
          V3(rock.base.x() + rock.lean_x * t +
                 rock.radius_x * profile * swell * std::cos(angle),
             y,
             rock.base.z() + rock.lean_z * t +
                 rock.radius_z * profile * swell * std::sin(angle));
    }
  }

  for (int ri = 0; ri + 1 < k_rock_rings; ++ri) {
    for (int i = 0; i < k_rock_sides; ++i) {
      int const next = (i + 1) % k_rock_sides;
      out.quad(ring[static_cast<std::size_t>(ri)][static_cast<std::size_t>(i)],
               ring[static_cast<std::size_t>(ri)][static_cast<std::size_t>(next)],
               ring[static_cast<std::size_t>(ri + 1)][static_cast<std::size_t>(next)],
               ring[static_cast<std::size_t>(ri + 1)][static_cast<std::size_t>(i)]);
    }
  }

  V3 const apex(rock.base.x() + rock.lean_x,
                rock.base.y() + rock.height,
                rock.base.z() + rock.lean_z);
  V3 const floor(rock.base.x(), rock.base.y() - k_rock_sink, rock.base.z());
  for (int i = 0; i < k_rock_sides; ++i) {
    int const next = (i + 1) % k_rock_sides;
    out.tri(ring[k_rock_rings - 1][static_cast<std::size_t>(i)],
            ring[k_rock_rings - 1][static_cast<std::size_t>(next)],
            apex);
    out.tri(ring[0][static_cast<std::size_t>(next)],
            ring[0][static_cast<std::size_t>(i)],
            floor);
  }
}

constexpr int k_shard_stations = 5;
constexpr std::array<float, k_shard_stations> k_shard_station{
    -0.20F, 0.06F, 0.36F, 0.66F, 0.88F};
constexpr std::array<float, k_shard_stations> k_shard_taper{
    1.02F, 1.00F, 0.80F, 0.52F, 0.26F};

void append_shard(Facets& out, const RockShard& shard) {
  V3 axis = shard.tip - shard.root;
  float const length = axis.length();
  if (length < 1.0e-4F) {
    return;
  }
  axis /= length;

  V3 const guide =
      std::abs(axis.y()) < 0.92F ? V3(0.0F, 1.0F, 0.0F) : V3(1.0F, 0.0F, 0.0F);
  V3 side = QVector3D::crossProduct(axis, guide).normalized();
  V3 const up = QVector3D::crossProduct(side, axis).normalized();

  V3 const bend =
      (side * signed01(shard.seed, 0, 11) + up * signed01(shard.seed, 0, 13)) * length *
      0.055F;
  float const roll = hash01(shard.seed, 0, 17) * k_tau;

  std::vector<std::vector<V3>> ring(k_shard_stations);
  for (int si = 0; si < k_shard_stations; ++si) {
    float const t = k_shard_station[static_cast<std::size_t>(si)];
    float const taper = k_shard_taper[static_cast<std::size_t>(si)];
    V3 const centre = shard.root + axis * (length * t) +
                      bend * std::sin(std::max(t, 0.0F) * 3.14159265F);
    auto& points = ring[static_cast<std::size_t>(si)];
    points.reserve(static_cast<std::size_t>(shard.sides));
    for (int i = 0; i < shard.sides; ++i) {
      float const angle =
          roll + k_tau * static_cast<float>(i) / static_cast<float>(shard.sides);
      float const facet = 0.74F + 0.44F * hash01(shard.seed, i, 19);
      float const r = shard.radius * taper * facet;
      points.push_back(centre + (side * std::cos(angle) + up * std::sin(angle)) * r);
    }
  }

  for (int si = 0; si + 1 < k_shard_stations; ++si) {
    for (int i = 0; i < shard.sides; ++i) {
      int const next = (i + 1) % shard.sides;
      out.quad(ring[static_cast<std::size_t>(si)][static_cast<std::size_t>(i)],
               ring[static_cast<std::size_t>(si)][static_cast<std::size_t>(next)],
               ring[static_cast<std::size_t>(si + 1)][static_cast<std::size_t>(next)],
               ring[static_cast<std::size_t>(si + 1)][static_cast<std::size_t>(i)]);
    }
  }

  V3 const apex = shard.tip + (side * signed01(shard.seed, 0, 23) +
                               up * signed01(shard.seed, 0, 29)) *
                                  shard.radius * 0.22F;
  V3 const floor = shard.root + axis * (length * k_shard_station[0]);
  for (int i = 0; i < shard.sides; ++i) {
    int const next = (i + 1) % shard.sides;
    out.tri(ring[k_shard_stations - 1][static_cast<std::size_t>(i)],
            ring[k_shard_stations - 1][static_cast<std::size_t>(next)],
            apex);
    out.tri(ring[0][static_cast<std::size_t>(next)],
            ring[0][static_cast<std::size_t>(i)],
            floor);
  }
}

} // namespace

namespace Render::GL::BackendPipelines {

void append_rock_mass(PropMeshData& out, const RockMass& rock) {
  Facets facets{std::move(out)};
  append_rock(facets, rock);
  out = std::move(facets.mesh);
}

void append_rock_shard(PropMeshData& out, const RockShard& shard) {
  Facets facets{std::move(out)};
  append_shard(facets, shard);
  out = std::move(facets.mesh);
}

} // namespace Render::GL::BackendPipelines
