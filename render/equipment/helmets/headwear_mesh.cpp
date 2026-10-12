#include "headwear_mesh.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <numbers>
#include <vector>

#include "animation/rig/humanoid_proportions.h"
#include "helmet_alignment.h"
#include "render/equipment/armor/sheet_mesh.h"
#include "render/gl/mesh.h"
#include "render/gl/shared_geometry_cache.h"

namespace Render::GL::Headwear {

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_tau = 2.0F * k_pi;
constexpr float k_head_socket_radius = 0.16F;
constexpr float k_attachment_scale = k_head_socket_radius * k_helmet_uniform_scale;

// Match make_full_cranium(): OrientedSphere uses twice its half-extents.
// Undo the helmet socket's scale and lift before fitting hair to the scalp.
const QVector3D k_scalp_center(0.0F,
                               HumanProportions::HEAD_RADIUS * 0.06F /
                                       k_attachment_scale -
                                   k_helmet_local_offset.y(),
                               0.0F);
const QVector3D k_scalp_radii = QVector3D(1.60F, 1.96F, 1.76F) *
                                (HumanProportions::HEAD_RADIUS / k_attachment_scale);

class MeshBuilder {
public:
  auto vertex(const QVector3D& position) -> unsigned int {
    auto const index = static_cast<unsigned int>(m_vertices.size());
    Vertex v{};
    v.position = {position.x(), position.y(), position.z()};
    m_vertices.push_back(v);
    return index;
  }

  void triangle(unsigned int a, unsigned int b, unsigned int c) {
    auto position = [this](unsigned int i) {
      auto const& p = m_vertices[i].position;
      return QVector3D(p[0], p[1], p[2]);
    };
    QVector3D const normal =
        QVector3D::crossProduct(position(b) - position(a), position(c) - position(a));
    m_indices.insert(m_indices.end(), {a, b, c});
    for (auto const i : {a, b, c}) {
      auto& n = m_vertices[i].normal;
      n[0] += normal.x();
      n[1] += normal.y();
      n[2] += normal.z();
    }
  }

  void quad(unsigned int a, unsigned int b, unsigned int c, unsigned int d) {
    triangle(a, b, c);
    triangle(a, c, d);
  }

  // Weld the circumferential seam so lighting stays continuous around each fold.
  template <typename Surface>
  void tube(unsigned int steps, unsigned int sides, bool closed, Surface surface) {
    auto const base = static_cast<unsigned int>(m_vertices.size());
    unsigned int const rings = closed ? steps : steps + 1U;
    for (unsigned int i = 0; i < rings; ++i) {
      float const u = static_cast<float>(i) / static_cast<float>(steps);
      for (unsigned int j = 0; j < sides; ++j) {
        float const angle = k_tau * static_cast<float>(j) / static_cast<float>(sides);
        vertex(surface(u, angle));
      }
    }
    for (unsigned int i = 0; i < steps; ++i) {
      unsigned int const next = (i + 1U) % rings;
      for (unsigned int j = 0; j < sides; ++j) {
        unsigned int const k = (j + 1U) % sides;
        quad(base + i * sides + j,
             base + next * sides + j,
             base + next * sides + k,
             base + i * sides + k);
      }
    }
  }

  auto build() -> std::unique_ptr<Mesh> {
    for (auto& v : m_vertices) {
      QVector3D const n = QVector3D(v.normal[0], v.normal[1], v.normal[2]).normalized();
      v.normal = {n.x(), n.y(), n.z()};
    }
    return std::make_unique<Mesh>(m_vertices, m_indices);
  }

private:
  std::vector<Vertex> m_vertices;
  std::vector<unsigned int> m_indices;
};

auto scalp_surface(float angle, float along, bool tied_back) -> QVector3D {
  float const front = std::max(0.0F, std::cos(angle));
  float const back = std::max(0.0F, -std::cos(angle));
  float const temples = std::sin(2.0F * angle);
  float const irregularity =
      (tied_back ? 0.012F : 0.028F) *
      (std::sin(11.0F * angle) + 0.45F * std::sin(19.0F * angle + 0.7F));
  float const hairline = 0.02F + (tied_back ? 0.34F : 0.28F) * front - 0.58F * back +
                         0.10F * temples * temples + irregularity;
  float const elevation = hairline + (0.5F * k_pi - hairline) * along;
  float const sweep = (tied_back ? 0.38F : -0.30F) * std::sin(k_pi * along);
  float const azimuth = angle + sweep * (tied_back ? std::sin(angle) : 1.0F);
  float const c = std::cos(elevation);
  QVector3D const direction(
      c * std::sin(azimuth), std::sin(elevation), c * std::cos(azimuth));
  QVector3D const normal = QVector3D(direction.x() / k_scalp_radii.x(),
                                     direction.y() / k_scalp_radii.y(),
                                     direction.z() / k_scalp_radii.z())
                               .normalized();

  float const roots = std::clamp(along / 0.16F, 0.0F, 1.0F);
  float const root_blend = roots * roots * (3.0F - 2.0F * roots);
  float const locks = 0.5F + 0.5F * std::cos(22.0F * angle + 2.5F * along);
  float const strands = 0.5F + 0.5F * std::cos(43.0F * angle - 3.0F * along);
  float const relief = c * c * (0.030F * locks + 0.008F * strands);
  float const lift = 0.012F + root_blend * ((tied_back ? 0.038F : 0.055F) + relief);
  return k_scalp_center + k_scalp_radii * direction + normal * lift;
}

auto make_hair_cap(bool tied_back) -> std::unique_ptr<Mesh> {
  constexpr unsigned int k_columns = 96U;
  constexpr unsigned int k_rings = 18U;
  MeshBuilder mesh;
  for (unsigned int row = 0; row < k_rings; ++row) {
    float const v = static_cast<float>(row) / static_cast<float>(k_rings);
    for (unsigned int col = 0; col < k_columns; ++col) {
      float const angle =
          k_tau * static_cast<float>(col) / static_cast<float>(k_columns);
      mesh.vertex(scalp_surface(angle, v, tied_back));
    }
  }
  for (unsigned int row = 0; row + 1U < k_rings; ++row) {
    for (unsigned int col = 0; col < k_columns; ++col) {
      unsigned int const next = (col + 1U) % k_columns;
      mesh.quad(row * k_columns + col,
                row * k_columns + next,
                (row + 1U) * k_columns + next,
                (row + 1U) * k_columns + col);
    }
  }
  // A single crown vertex avoids collapsed quads and a pinched shading seam.
  unsigned int const top = mesh.vertex(scalp_surface(0.0F, 1.0F, tied_back));
  unsigned int const last = (k_rings - 1U) * k_columns;
  for (unsigned int col = 0; col < k_columns; ++col) {
    mesh.triangle(last + col, last + (col + 1U) % k_columns, top);
  }
  return mesh.build();
}

auto make_bun_coil() -> std::unique_ptr<Mesh> {
  MeshBuilder mesh;
  // A flattened spiral, with both ends tucked into the underlying hair mass.
  mesh.tube(96U, 10U, false, [](float u, float section) {
    float const angle = 2.35F * k_tau * u;
    float const tuck = std::pow(std::abs(2.0F * u - 1.0F), 8.0F);
    float const radius = 0.045F + 0.38F * u - 0.10F * u * tuck;
    QVector3D const radial(std::cos(angle), std::sin(angle), 0.0F);
    QVector3D const center =
        QVector3D(0.0F, -0.28F, -1.68F + 0.26F * tuck) + radial * radius;
    float const strand = 1.0F + 0.06F * std::cos(3.0F * section + angle);
    return center + radial * (0.105F * strand * std::cos(section)) +
           QVector3D(0.0F, 0.0F, 0.075F * strand * std::sin(section));
  });
  return mesh.build();
}

auto make_bun_tie() -> std::unique_ptr<Mesh> {
  MeshBuilder mesh;
  mesh.tube(48U, 8U, true, [](float u, float section) {
    float const angle = k_tau * u;
    float const edge = 0.018F * std::cos(section);
    // A narrow textile tie sits at the base of the bun.
    return QVector3D((0.44F + edge) * std::cos(angle),
                     -0.28F + (0.40F + edge) * std::sin(angle),
                     -1.28F + 0.055F * std::sin(section));
  });
  return mesh.build();
}

auto make_turban_wraps() -> std::unique_ptr<Mesh> {
  MeshBuilder mesh;
  constexpr unsigned int k_wraps = 5U;
  for (unsigned int layer = 0; layer < k_wraps; ++layer) {
    mesh.tube(64U, 12U, true, [layer](float u, float section) {
      float const band = static_cast<float>(layer);
      float const angle = k_tau * u;
      float const taper = std::max(0.0F, band - 1.0F);
      float const rx = 1.33F - 0.026F * taper * taper;
      float const rz = rx + 0.13F;
      float const tilt = (layer % 2U == 0U ? 0.13F : -0.13F) * std::sin(angle);
      float const height =
          0.15F + 0.245F * band + tilt + 0.035F * std::cos(2.0F * angle + band);
      float const wrinkle = 0.010F * std::sin(7.0F * angle + band) * std::sin(section) *
                            std::sin(section);
      float const fold = 0.065F * std::cos(section) + wrinkle;
      return QVector3D((rx + fold) * std::sin(angle),
                       height + 0.21F * std::sin(section),
                       -0.05F + (rz + fold) * std::cos(angle));
    });
  }
  // Carry the tucked end over the crown so the wrap does not end in a bare dome.
  mesh.tube(40U, 12U, false, [](float u, float section) {
    float const angle = k_pi * u;
    QVector3D const outward(std::cos(angle), std::sin(angle), 0.0F);
    QVector3D const center(1.06F * std::cos(angle),
                           0.81F + 0.70F * std::sin(angle),
                           -0.05F + 0.025F * std::sin(2.0F * angle));
    return center + outward * (0.045F * std::cos(section)) +
           QVector3D(0.0F,
                     -0.018F * std::sin(section) * std::sin(section),
                     0.24F * std::sin(section));
  });
  return mesh.build();
}

auto make_turban_tail() -> std::unique_ptr<Mesh> {
  SheetGrid grid;
  grid.columns = 9;
  grid.rows = 15;
  for (int row = 0; row < grid.rows; ++row) {
    float const v = static_cast<float>(row) / static_cast<float>(grid.rows - 1);
    for (int col = 0; col < grid.columns; ++col) {
      float const u = static_cast<float>(col) / static_cast<float>(grid.columns - 1);
      float const across = 2.0F * u - 1.0F;
      float const width = 0.18F + 0.07F * std::sin(k_pi * v) - 0.035F * v;
      float const pleat = 0.035F * std::cos(3.0F * k_pi * across + 0.6F * v);
      grid.positions.emplace_back(0.28F + 0.14F * v + width * across,
                                  0.57F - 1.60F * v + 0.055F * across * v * v,
                                  -1.48F - 0.10F * std::sin(k_pi * v) + pleat);
      grid.uvs.emplace_back(u, v);
    }
  }
  return make_thick_sheet_mesh(grid, false, 0.018F);
}

} // namespace

auto hair_cap(bool tied_back) -> Mesh* {
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("equipment/headwear/hair_cap", tied_back ? 1U : 0U),
      [tied_back] { return make_hair_cap(tied_back); });
}

auto bun_coil() -> Mesh* {
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("equipment/headwear/bun_coil"), [] { return make_bun_coil(); });
}

auto bun_tie() -> Mesh* {
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("equipment/headwear/bun_tie"), [] { return make_bun_tie(); });
}

auto turban_wraps() -> Mesh* {
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("equipment/headwear/turban_wraps"),
      [] { return make_turban_wraps(); });
}

auto turban_tail() -> Mesh* {
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("equipment/headwear/turban_tail"),
      [] { return make_turban_tail(); });
}

} // namespace Render::GL::Headwear
