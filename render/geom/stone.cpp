#include "stone.h"
#include "../entity/registry.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace Render {
namespace Geom {

static auto create_stone_mesh() -> std::unique_ptr<GL::Mesh> {
  using GL::Vertex;
  std::vector<GL::Vertex> verts;
  std::vector<unsigned int> idx;

  constexpr int k_latitude_segments = 8;
  constexpr int k_longitude_segments = 10;
  constexpr float k_base_radius = 0.15F;

  for (int lat = 0; lat <= k_latitude_segments; ++lat) {
    float const theta = static_cast<float>(lat) / k_latitude_segments *
                        std::numbers::pi_v<float>;
    float const sin_theta = std::sin(theta);
    float const cos_theta = std::cos(theta);

    for (int lon = 0; lon <= k_longitude_segments; ++lon) {
      float const phi = static_cast<float>(lon) / k_longitude_segments * 2.0F *
                        std::numbers::pi_v<float>;
      float const sin_phi = std::sin(phi);
      float const cos_phi = std::cos(phi);

      float const noise = 1.0F + 0.15F * std::sin(phi * 3.0F + theta * 2.0F) +
                          0.1F * std::cos(phi * 5.0F - theta * 3.0F);
      float const radius = k_base_radius * noise;

      float const x = radius * sin_theta * cos_phi;
      float const y = radius * cos_theta;
      float const z = radius * sin_theta * sin_phi;

      QVector3D const normal =
          QVector3D(sin_theta * cos_phi, cos_theta, sin_theta * sin_phi)
              .normalized();

      float const u = static_cast<float>(lon) / k_longitude_segments;
      float const v = static_cast<float>(lat) / k_latitude_segments;

      verts.push_back(
          {{x, y, z}, {normal.x(), normal.y(), normal.z()}, {u, v}});
    }
  }

  for (int lat = 0; lat < k_latitude_segments; ++lat) {
    for (int lon = 0; lon < k_longitude_segments; ++lon) {
      int const first = lat * (k_longitude_segments + 1) + lon;
      int const second = first + k_longitude_segments + 1;

      idx.push_back(first);
      idx.push_back(second);
      idx.push_back(first + 1);

      idx.push_back(second);
      idx.push_back(second + 1);
      idx.push_back(first + 1);
    }
  }

  return std::make_unique<GL::Mesh>(verts, idx);
}

auto Stone::get() -> GL::Mesh * {
  static std::unique_ptr<GL::Mesh> const mesh = create_stone_mesh();
  return mesh.get();
}

} // namespace Geom
} // namespace Render
