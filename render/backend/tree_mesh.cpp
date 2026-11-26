#include "render/backend/tree_mesh.h"

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
    constexpr unsigned k_sides = 9;
    constexpr unsigned k_rings = 3;
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

    // Spiral the leaf clusters around the surface so the silver leaves
    // break both the sunlit face and the silhouette from any camera angle.
    for (unsigned i = 0; i < 12; ++i) {
      const float h = (static_cast<float>(i) + 0.5F) / 12.0F * 2.0F - 1.0F;
      const float radial = std::sqrt(1.0F - h * h);
      const float a = 2.39996F * i + noise(seed) * k_tau;
      const QVector3D offset(std::cos(a) * radial * radii.x(),
                             h * radii.y(),
                             std::sin(a) * radial * radii.z());
      const QVector3D at = center + rotate(offset);
      const float direction = yaw + a;
      leaf(at, direction - 0.42F, 0.10F + h * 0.45F, seed + i * 13U);
      leaf(at, direction + 0.42F, 0.32F + h * 0.45F, seed + i * 17U);
    }
  }

  // A closed spray has a full shoulder, a narrow tip and a slight bend.
  // The asymmetric section gives each bough a sunlit ridge and shaded underside.
  void spray(const QVector3D& base,
             const QVector3D& tip,
             const QVector3D& bend,
             float width,
             float thickness,
             unsigned seed) {
    constexpr unsigned sides = 5;
    const QVector3D axis = (tip - base).normalized();
    const QVector3D side =
        QVector3D::crossProduct(axis, {0.0F, 1.0F, 0.0F}).normalized();
    const QVector3D up = QVector3D::crossProduct(side, axis);
    const auto start = static_cast<unsigned>(mesh.vertices.size());
    auto vertex = [&](const QVector3D& p, const QVector3D& n, float weight) {
      mesh.vertices.push_back({p, n, std::atan2(p.z(), p.x()) / k_tau + 0.5F,
                               0.44F + p.y() * 0.40F, weight});
    };
    vertex(base, -axis, 0.15F);
    for (unsigned ring = 0; ring < 2; ++ring) {
      const float t = ring == 0 ? 0.36F : 0.72F;
      const float taper = ring == 0 ? 1.0F : 0.66F;
      const QVector3D center = base + (tip - base) * t + bend * (4.0F * t * (1.0F - t));
      for (unsigned i = 0; i < sides; ++i) {
        const float angle = k_tau * i / sides + 0.31F;
        const float x = std::cos(angle);
        const float y = std::sin(angle);
        const float radius = taper * (0.88F + 0.24F * noise(seed + i * 19U));
        const QVector3D p =
            center + (side * (width * x) + up * (thickness * y)) * radius;
        const QVector3D n = (side * (thickness * x) + up * (width * y) +
                             axis * (ring == 0 ? -0.018F : 0.028F)).normalized();
        vertex(p, n, t);
      }
    }
    const auto end = static_cast<unsigned>(mesh.vertices.size());
    vertex(tip, axis, 1.0F);
    for (unsigned i = 0; i < sides; ++i) {
      const unsigned a = start + 1U + i;
      const unsigned b = start + 1U + (i + 1U) % sides;
      triangle(start, a, b);
      triangle(a, a + sides, b);
      triangle(b, a + sides, b + sides);
      triangle(a + sides, end, b + sides);
    }
  }

  void leaf(const QVector3D& center, float yaw, float lift, unsigned seed) {
    const QVector3D along = QVector3D(std::cos(yaw), lift, std::sin(yaw)).normalized();
    const QVector3D across(-std::sin(yaw), 0.0F, std::cos(yaw));
    const QVector3D up = QVector3D::crossProduct(across, along).normalized();
    const float length = 0.028F + noise(seed) * 0.020F;
    const auto start = static_cast<unsigned>(mesh.vertices.size());

    // A high midrib and two lower shoulders make a folded, closed leaf.
    const std::array<QVector3D, 4> offsets{
        along * length + up * 0.004F,
        across * length * 0.30F - up * 0.004F,
        -along * length + up * 0.004F,
        -across * length * 0.30F - up * 0.004F};
    for (const auto& offset : offsets) {
      mesh.vertices.push_back({center + offset,
                               (offset + up * QVector3D::dotProduct(offset, up) * 4.0F)
                                   .normalized(),
                               std::atan2(center.z(), center.x()) / k_tau + 0.5F,
                               0.85F,
                               1.0F});
    }
    triangle(start, start + 2U, start + 1U);
    triangle(start, start + 3U, start + 2U);
    triangle(start, start + 1U, start + 3U);
    triangle(start + 1U, start + 2U, start + 3U);
  }
};

} // namespace

auto build_pine_tree_mesh() -> TreeMesh {
  TreeBuilder tree;
  tree.stem({0.0F, -0.02F, 0.0F}, {0.018F, 0.38F, -0.012F}, 0.082F, 0.052F, 11U);
  tree.stem({0.018F, 0.38F, -0.012F}, {0.0F, 1.32F, 0.0F}, 0.052F, 0.009F, 13U);

  for (unsigned tier = 0; tier < 7; ++tier) {
    const float level = static_cast<float>(tier) / 7.0F;
    const unsigned arms = 7U - tier / 2U;
    for (unsigned arm = 0; arm < arms; ++arm) {
      const unsigned seed = tier * 97U + arm * 17U;
      const float yaw = k_tau * arm / arms + tier * 2.40F + 0.28F * noise(seed);
      const QVector3D dir(std::cos(yaw), 0.0F, std::sin(yaw));
      const QVector3D side(-dir.z(), 0.0F, dir.x());
      const float span = (0.49F - 0.40F * level) * (0.84F + 0.27F * noise(seed + 1U));
      const float y = 0.37F + 0.139F * tier + noise(seed + 2U) * 0.065F;
      const QVector3D base(0.01F, y, 0.0F);
      const float lift = -0.12F + 0.19F * level + noise(seed + 4U) * 0.06F;
      const QVector3D tip = base + dir * span + QVector3D(0.0F, lift, 0.0F);
      const QVector3D bend(0.0F, 0.057F * (1.0F - level), 0.0F);
      tree.spray(base, tip, bend, span * 0.32F, span * 0.18F, seed);
      for (int hand : {-1, 1}) {
        if (hand > 0 && (tier >= 4U || arm % 2U == 0U)) {
          continue;
        }
        const QVector3D fork = base + dir * (span * 0.25F);
        const QVector3D end = tip - dir * (span * 0.16F) +
                              side * (span * hand * 0.35F);
        tree.spray(fork, end, bend, span * 0.20F, span * 0.115F, seed + 5U);
      }
    }
  }
  tree.spray({0.0F, 1.17F, 0.0F}, {0.025F, 1.42F, 0.0F}, {},
             0.073F, 0.077F, 61U);
  return std::move(tree.mesh);
}

auto build_cypress_tree_mesh() -> TreeMesh {
  TreeBuilder tree;
  tree.stem({0.0F, -0.018F, 0.0F}, {0.016F, 0.58F, 0.0F}, 0.062F, 0.032F, 17U);
  tree.spray({0.016F, 0.20F, 0.0F}, {0.035F, 1.64F, -0.020F}, {},
             0.161F, 0.146F, 31U);
  // Upright lateral sprays nest against the leader, with daylight between
  // their tips instead of a single continuous outline.
  for (unsigned i = 0; i < 16; ++i) {
    const float level = static_cast<float>(i) / 16.0F;
    const float yaw = 2.39996F * i;
    const QVector3D dir(std::cos(yaw), 0.0F, std::sin(yaw));
    const float y = 0.22F + 0.80F * level;
    const float reach = (0.128F + 0.030F * noise(37U + i)) * (1.0F - 0.63F * level);
    const QVector3D base = dir * 0.035F + QVector3D(0.0F, y, 0.0F);
    const QVector3D tip = dir * reach + QVector3D(0.0F, y + 0.50F, 0.0F);
    tree.spray(base, tip, dir * 0.029F, 0.075F, 0.068F, i * 37U);
  }
  return std::move(tree.mesh);
}

auto build_olive_tree_mesh() -> TreeMesh {
  TreeBuilder tree;
  const QVector3D root(-0.012F, -0.025F, 0.0F);
  const QVector3D elbow_low(-0.055F, 0.20F, 0.016F);
  const QVector3D fork(0.025F, 0.35F, -0.022F);
  tree.stem(root, elbow_low, 0.132F, 0.093F, 11U);
  tree.stem(elbow_low, fork, 0.095F, 0.078F, 13U);

  // Unequal scaffolding limbs carry broad, flattened crowns. The open centre
  // lets the fork and the undersides of individual boughs read in an orchard.
  for (unsigned branch = 0; branch < 5; ++branch) {
    const unsigned seed = 211U + branch * 73U;
    const float angle = branch * k_tau / 5.0F + noise(seed) * 0.35F;
    const QVector3D dir(std::cos(angle), 0.0F, std::sin(angle));
    const QVector3D side(-dir.z(), 0.0F, dir.x());
    const float reach = 0.31F + noise(seed + 1U) * 0.13F;
    const float y = 0.67F + noise(seed + 2U) * 0.35F;
    const QVector3D elbow = fork + dir * (reach * 0.45F) + side * 0.043F +
                            QVector3D(0.0F, 0.12F + noise(seed + 4U) * 0.09F, 0.0F);
    const QVector3D tip = dir * reach + QVector3D(0.0F, y, 0.0F);
    tree.stem(fork, elbow, 0.068F, 0.048F, seed);
    tree.stem(elbow, tip, 0.049F, 0.017F, seed + 3U);
    tree.crown(tip, QVector3D(0.235F, 0.112F, 0.16F), angle, seed + 5U);
    for (int twig = -1; twig <= 1; twig += 2) {
      const QVector3D end = tip - dir * 0.13F + side * (0.16F * twig) +
                            QVector3D(0.0F, 0.055F + noise(seed + 7U) * 0.065F, 0.0F);
      tree.stem(elbow, end, 0.030F, 0.009F, seed + 11U);
      tree.crown(end,
                 QVector3D(0.185F, 0.097F, 0.145F),
                 angle + twig * 0.6F,
                 seed + (twig < 0 ? 19U : 29U));
    }
  }
  return std::move(tree.mesh);
}

} // namespace Render::GL::BackendPipelines
