#include "stone.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "render/entity/registry.h"
#include "render/gl/mesh.h"
#include "render/gl/resources.h"
#include "render/gl/shared_geometry_cache.h"
#include "render/scene_renderer.h"

namespace Render {
namespace Geom {

namespace {

constexpr int k_latitude_segments = 9;
constexpr int k_longitude_segments = 12;

auto stone_surface_point(float theta, float phi) -> QVector3D {
  float const sin_theta = std::sin(theta);
  float const cos_theta = std::cos(theta);
  float const sin_phi = std::sin(phi);
  float const cos_phi = std::cos(phi);

  float const facet = 0.155F * std::sin(phi * 3.0F + theta * 2.0F) +
                      0.105F * std::cos(phi * 5.0F - theta * 3.0F) +
                      0.062F * std::sin(phi * 8.0F + theta * 6.0F) +
                      0.048F * std::cos(phi * 2.0F - theta * 7.0F);
  float const flattening = 0.088F * std::cos(theta * 2.0F);
  float const radius = Stone::k_mean_radius * (1.0F + facet - flattening);

  return {radius * sin_theta * cos_phi,
          radius * cos_theta * 0.92F,
          radius * sin_theta * sin_phi};
}

} // namespace

static auto create_stone_mesh() -> std::unique_ptr<GL::Mesh> {
  using GL::Vertex;
  std::vector<GL::Vertex> verts;
  std::vector<unsigned int> idx;

  verts.reserve(static_cast<std::size_t>(k_latitude_segments) * k_longitude_segments *
                6U);
  idx.reserve(verts.capacity());

  auto push_facet = [&](const QVector3D& a,
                        const QVector3D& b,
                        const QVector3D& c,
                        float u,
                        float v) {
    QVector3D normal = QVector3D::crossProduct(b - a, c - a);
    if (normal.lengthSquared() < 1.0e-12F) {
      return;
    }
    normal.normalize();
    for (const auto& point : {a, b, c}) {
      idx.push_back(static_cast<unsigned int>(verts.size()));
      verts.push_back({{point.x(), point.y(), point.z()},
                       {normal.x(), normal.y(), normal.z()},
                       {u, v}});
    }
  };

  for (int lat = 0; lat < k_latitude_segments; ++lat) {
    float const theta_0 = static_cast<float>(lat) /
                          static_cast<float>(k_latitude_segments) *
                          std::numbers::pi_v<float>;
    float const theta_1 = static_cast<float>(lat + 1) /
                          static_cast<float>(k_latitude_segments) *
                          std::numbers::pi_v<float>;

    for (int lon = 0; lon < k_longitude_segments; ++lon) {
      float const phi_0 = static_cast<float>(lon) /
                          static_cast<float>(k_longitude_segments) * 2.0F *
                          std::numbers::pi_v<float>;
      float const phi_1 = static_cast<float>(lon + 1) /
                          static_cast<float>(k_longitude_segments) * 2.0F *
                          std::numbers::pi_v<float>;

      QVector3D const p00 = stone_surface_point(theta_0, phi_0);
      QVector3D const p01 = stone_surface_point(theta_0, phi_1);
      QVector3D const p10 = stone_surface_point(theta_1, phi_0);
      QVector3D const p11 = stone_surface_point(theta_1, phi_1);

      float const u = static_cast<float>(lon) / k_longitude_segments;
      float const v = static_cast<float>(lat) / k_latitude_segments;

      push_facet(p00, p10, p01, u, v);
      push_facet(p10, p11, p01, u, v);
    }
  }

  return std::make_unique<GL::Mesh>(verts, idx);
}

auto Stone::get() -> GL::Mesh* {
  return GL::SharedGeometryCache::instance().get_or_build(
      GL::geometry_key("geom/stone"), create_stone_mesh);
}

} // namespace Geom
} // namespace Render
