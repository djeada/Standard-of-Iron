#include "tree_mesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace Render::GL::BackendPipelines {
namespace {

constexpr float k_tau = 2.0F * std::numbers::pi_v<float>;

auto noise(unsigned seed) -> float {
  seed ^= seed >> 16U;
  seed *= 0x7FEB352DU;
  seed ^= seed >> 15U;
  seed *= 0x846CA68BU;
  seed ^= seed >> 16U;
  return static_cast<float>(seed & 0xFFFFU) / 65535.0F;
}

class TreeBuilder {
public:
  TreeMesh mesh;

  void triangle(unsigned a, unsigned b, unsigned c) {
    mesh.indices.insert(mesh.indices.end(),
                        {static_cast<std::uint16_t>(a),
                         static_cast<std::uint16_t>(b),
                         static_cast<std::uint16_t>(c)});
  }

  void stem(const QVector3D& from,
            const QVector3D& to,
            float base_radius,
            float tip_radius,
            unsigned seed) {
    constexpr unsigned k_sides = 8;
    const QVector3D axis = (to - from).normalized();
    const QVector3D tangent =
        QVector3D::crossProduct(axis, QVector3D(0.0F, 0.0F, 1.0F)).normalized();
    const QVector3D bitangent = QVector3D::crossProduct(tangent, axis);
    const auto start = static_cast<unsigned>(mesh.vertices.size());
    const float taper = (base_radius - tip_radius) / (to - from).length();
    for (unsigned ring = 0; ring < 2; ++ring) {
      const QVector3D center = ring == 0 ? from : to;
      const float radius = ring == 0 ? base_radius : tip_radius;
      for (unsigned i = 0; i < k_sides; ++i) {
        const float u = static_cast<float>(i) / k_sides;
        const QVector3D radial =
            tangent * std::cos(u * k_tau) + bitangent * std::sin(u * k_tau);
        const float ridge = 0.92F + 0.16F * noise(seed + i * 37U);
        mesh.vertices.push_back({center + radial * radius * ridge,
                                 (radial + axis * taper).normalized(),
                                 u,
                                 0.06F + std::clamp(center.y(), 0.0F, 1.4F) * 0.16F,
                                 0.0F});
      }
    }
    for (unsigned i = 0; i < k_sides; ++i) {
      const unsigned next = (i + 1U) % k_sides;
      triangle(start + i, start + k_sides + i, start + next);
      triangle(start + next, start + k_sides + i, start + k_sides + next);
    }

    for (unsigned ring = 0; ring < 2; ++ring) {
      const auto cap = static_cast<unsigned>(mesh.vertices.size());
      const auto first = start + ring * k_sides;
      for (unsigned i = 0; i < k_sides; ++i) {
        RingVertex v = mesh.vertices[first + i];
        v.normal = ring == 0 ? -axis : axis;
        mesh.vertices.push_back(v);
      }
      for (unsigned i = 1; i + 1U < k_sides; ++i) {
        if (ring == 0) {
          triangle(cap, cap + i, cap + i + 1U);
        } else {
          triangle(cap, cap + i + 1U, cap + i);
        }
      }
    }
  }

  void
  crown(const QVector3D& center, const QVector3D& radii, float yaw, unsigned seed) {
    constexpr unsigned k_sides = 10;
    constexpr unsigned k_rings = 5;
    const auto start = static_cast<unsigned>(mesh.vertices.size());
    const auto rotate = [yaw](QVector3D p) {
      return QVector3D(p.x() * std::cos(yaw) - p.z() * std::sin(yaw),
                       p.y(),
                       p.x() * std::sin(yaw) + p.z() * std::cos(yaw));
    };
    const auto vertex = [&](QVector3D unit, unsigned key) {
      const float irregularity = 0.87F + noise(key) * 0.26F;
      const QVector3D p = center + rotate(QVector3D(unit.x() * radii.x(),
                                                    unit.y() * radii.y(),
                                                    unit.z() * radii.z()) *
                                          irregularity);
      const QVector3D normal =
          rotate(QVector3D(
                     unit.x() / radii.x(), unit.y() / radii.y(), unit.z() / radii.z()))
              .normalized();
      const float u = std::atan2(p.z(), p.x()) / k_tau + 0.5F;
      const float height = std::clamp(p.y() / 1.45F, 0.0F, 1.0F);
      mesh.vertices.push_back(
          {p,
           normal,
           u,
           0.58F + height * 0.46F,
           std::clamp(std::hypot(p.x(), p.z()) / 0.40F, 0.0F, 1.0F)});
    };
    for (unsigned ring = 0; ring < k_rings; ++ring) {
      const float latitude =
          -std::numbers::pi_v<float> * 0.5F +
          std::numbers::pi_v<float> * static_cast<float>(ring + 1U) / (k_rings + 1U);
      for (unsigned i = 0; i < k_sides; ++i) {
        const float angle = k_tau * static_cast<float>(i) / k_sides;
        vertex(QVector3D(std::cos(angle) * std::cos(latitude),
                         std::sin(latitude),
                         std::sin(angle) * std::cos(latitude)),
               seed + ring * 43U + i * 17U);
      }
    }
    const auto bottom = static_cast<unsigned>(mesh.vertices.size());
    vertex(QVector3D(0.0F, -1.0F, 0.0F), seed + 701U);
    const auto top = static_cast<unsigned>(mesh.vertices.size());
    vertex(QVector3D(0.0F, 1.0F, 0.0F), seed + 709U);
    for (unsigned i = 0; i < k_sides; ++i) {
      const unsigned next = (i + 1U) % k_sides;
      triangle(bottom, start + i, start + next);
      triangle(top,
               start + (k_rings - 1U) * k_sides + next,
               start + (k_rings - 1U) * k_sides + i);
      for (unsigned ring = 0; ring + 1U < k_rings; ++ring) {
        const unsigned a = start + ring * k_sides + i;
        const unsigned b = start + ring * k_sides + next;
        triangle(a, a + k_sides, b);
        triangle(b, a + k_sides, b + k_sides);
      }
    }
  }

  void leaf(const QVector3D& center, float yaw, float lift, unsigned seed) {
    const QVector3D along = QVector3D(std::cos(yaw), lift, std::sin(yaw)).normalized();
    const QVector3D across(-std::sin(yaw), 0.0F, std::cos(yaw));
    const QVector3D up = QVector3D::crossProduct(across, along).normalized();
    const float length = 0.030F + noise(seed) * 0.018F;
    const auto start = static_cast<unsigned>(mesh.vertices.size());

    const std::array<QVector3D, 6> offsets{along * length,
                                           across * length * 0.30F,
                                           -along * length,
                                           -across * length * 0.30F,
                                           up * 0.004F,
                                           -up * 0.002F};
    for (const auto& offset : offsets) {
      mesh.vertices.push_back({center + offset,
                               offset.normalized(),
                               std::atan2(center.z(), center.x()) / k_tau + 0.5F,
                               0.85F,
                               1.0F});
    }
    for (unsigned i = 0; i < 4; ++i) {
      const unsigned next = (i + 1U) % 4U;
      triangle(start + i, start + 4U, start + next);
      triangle(start + i, start + next, start + 5U);
    }
  }
};

} // namespace

auto build_olive_tree_mesh() -> TreeMesh {
  TreeBuilder tree;
  const QVector3D root(-0.012F, -0.025F, 0.0F);
  const QVector3D fork(0.032F, 0.31F, 0.026F);
  tree.stem(root, QVector3D(-0.018F, 0.15F, 0.012F), 0.15F, 0.10F, 11U);
  tree.stem(QVector3D(-0.018F, 0.15F, 0.012F), fork, 0.10F, 0.075F, 13U);

  tree.crown(
      QVector3D(0.0F, 0.70F, 0.025F), QVector3D(0.30F, 0.19F, 0.28F), 0.3F, 101U);
  for (unsigned branch = 0; branch < 6; ++branch) {
    const unsigned seed = 211U + branch * 73U;
    const float angle = branch * k_tau / 6.0F + noise(seed) * 0.35F;
    const QVector3D dir(std::cos(angle), 0.0F, std::sin(angle));
    const QVector3D side(-dir.z(), 0.0F, dir.x());
    const float reach = 0.30F + noise(seed + 1U) * 0.12F;
    const float y = 0.62F + noise(seed + 2U) * 0.17F;
    const QVector3D elbow = fork + dir * (reach * 0.52F) + QVector3D(0.0F, 0.13F, 0.0F);
    const QVector3D tip = dir * reach + QVector3D(0.0F, y, 0.0F);
    tree.stem(fork, elbow, 0.045F, 0.028F, seed);
    tree.stem(elbow, tip, 0.028F, 0.010F, seed + 3U);
    tree.crown(tip, QVector3D(0.255F, 0.145F, 0.20F), angle, seed + 5U);
    for (unsigned spray = 0; spray < 12; ++spray) {
      const float around = k_tau * static_cast<float>(spray) / 12.0F;
      const QVector3D radial = dir * std::cos(around) + side * std::sin(around);
      const QVector3D at =
          tip + dir * (std::cos(around) * 0.25F) + side * (std::sin(around) * 0.20F) +
          QVector3D(0.0F, (noise(seed + spray * 7U) - 0.5F) * 0.12F, 0.0F);
      for (unsigned blade = 0; blade < 3; ++blade) {
        tree.leaf(at + radial * (blade * 0.021F),
                  angle + around + (static_cast<float>(blade) - 1.0F) * 0.85F,
                  0.22F + noise(seed + blade) * 0.50F,
                  seed + spray * 11U + blade);
      }
    }
    for (int twig = -1; twig <= 1; twig += 2) {
      const QVector3D end = tip + dir * 0.07F + side * (0.14F * twig) +
                            QVector3D(0.0F, -0.06F + noise(seed + 7U) * 0.06F, 0.0F);
      tree.stem(elbow, end, 0.016F, 0.005F, seed + 11U);
      tree.crown(end,
                 QVector3D(0.15F, 0.105F, 0.12F),
                 angle + twig * 0.6F,
                 seed + (twig < 0 ? 19U : 29U));
    }
  }
  return std::move(tree.mesh);
}

} // namespace Render::GL::BackendPipelines
