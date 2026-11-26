#include "render/backend/tree_mesh.h"

#include <algorithm>
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

  // A single tapered sweep keeps the wood grain and normals continuous at bends.
  void olive_limb(const QVector3D& base,
                  const QVector3D& bend,
                  const QVector3D& tip,
                  float radius,
                  float end_radius,
                  unsigned seed) {
    constexpr unsigned k_sides = 8;
    constexpr unsigned k_rings = 5;
    const auto start = static_cast<unsigned>(mesh.vertices.size());
    for (unsigned ring = 0; ring < k_rings; ++ring) {
      const float t = static_cast<float>(ring) / (k_rings - 1U);
      const float u = 1.0F - t;
      const QVector3D center = base * (u * u) + bend * (2.0F * t * u) + tip * (t * t);
      const QVector3D axis = ((bend - base) * u + (tip - bend) * t).normalized();
      const QVector3D side =
          QVector3D::crossProduct(axis, {0.0F, 0.0F, 1.0F}).normalized();
      const QVector3D across = QVector3D::crossProduct(side, axis);
      const float width = end_radius + (radius - end_radius) * u * (0.7F + 0.3F * u);
      for (unsigned i = 0; i < k_sides; ++i) {
        const float uv = static_cast<float>(i) / k_sides;
        const float angle = k_tau * uv + 0.42F * t;
        const QVector3D radial = side * std::cos(angle) + across * std::sin(angle);
        const float ridge = 0.85F + 0.27F * noise(seed + i * 19U);
        QVector3D position = center + radial * (width * ridge);
        if (ring == 0U && base.y() < 0.0F) {
          position.setY(base.y());
        }
        mesh.vertices.push_back({position,
                                 (radial + axis * 0.10F).normalized(),
                                 uv, 0.06F + std::max(0.0F, center.y()) * 0.16F, 0.0F});
        if (ring > 0) {
          const unsigned v = start + ring * k_sides + i;
          const unsigned next = start + ring * k_sides + (i + 1U) % k_sides;
          triangle(v - k_sides, v, next - k_sides);
          triangle(next - k_sides, v, next);
        }
      }
    }
    for (unsigned end = 0; end < 2; ++end) {
      const bool top = end == 1U;
      const unsigned first = start + (top ? (k_rings - 1U) * k_sides : 0U);
      const QVector3D normal = (top ? tip - bend : base - bend).normalized();
      const auto cap = static_cast<unsigned>(mesh.vertices.size());
      mesh.vertices.push_back(
          {top ? tip : base, normal, 0.5F, mesh.vertices[first].v, 0.0F});
      for (unsigned i = 0; i < k_sides; ++i) {
        const unsigned a = first + i;
        const unsigned b = first + (i + 1U) % k_sides;
        triangle(cap, top ? b : a, top ? a : b);
      }
    }
  }

  void olive_crown(const QVector3D& center,
                   const QVector3D& radii,
                   float yaw,
                   unsigned seed) {
    // Opposite pairs of blades share an arching twig. Outer shoots droop
    // under their own weight and the younger central shoots turn upward.
    const QVector3D forward(std::cos(yaw), 0.0F, std::sin(yaw));
    const QVector3D across(-forward.z(), 0.0F, forward.x());
    for (unsigned spray = 0; spray < 8; ++spray) {
      const unsigned key = seed + spray * 31U;
      const float angle = 2.39996F * spray + noise(seed) * k_tau;
      const float level = (spray + 0.5F) / 8.0F;
      const QVector3D tip = (forward * (radii.x() * std::cos(angle)) +
                             across * (radii.z() * std::sin(angle))) *
                            (1.15F - 0.47F * level);
      const QVector3D dir = tip.normalized();
      const QVector3D side(-dir.z(), 0.0F, dir.x());
      const auto spine = [&](float t) {
        const float lift = radii.y() * t * (2.6F * level + 0.30F - 1.45F * t);
        return center + tip * t + QVector3D(0.0F, lift, 0.0F);
      };
      const auto rib = static_cast<unsigned>(mesh.vertices.size());
      for (unsigned station = 0; station < 3; ++station) {
        const float t = station * 0.5F;
        for (int hand : {-1, 1}) {
          mesh.vertices.push_back({spine(t) + side * (hand * (0.006F - 0.003F * t)),
                                   {0.0F, 1.0F, 0.0F}, yaw / k_tau, 0.82F, -1.0F});
        }
        if (station > 0) {
          const unsigned v = rib + station * 2U;
          triangle(v - 2U, v - 1U, v);
          triangle(v, v - 1U, v + 1U);
        }
      }
      const float tone = noise(key);
      for (unsigned pair = 0; pair < 2; ++pair) {
        const float t = 0.18F + pair * 0.39F;
        const float spread = 0.56F + 0.58F * noise(key + pair + 1U);
        for (int hand : {-1, 1}) {
          olive_leaf(spine(t),
                     dir + side * (hand * spread),
                     0.34F * level - 0.35F * t,
                     0.116F + 0.036F * noise(key + pair),
                     tone,
                     hand * (0.32F + 0.80F * noise(key + pair + 3U)),
                     false);
        }
      }
      olive_leaf(spine(0.83F), dir, level * 0.48F - 0.47F,
                 0.103F, tone, (tone - 0.5F) * 1.35F, true);
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

  void olive_leaf(const QVector3D& base,
                  QVector3D direction,
                  float lift,
                  float length,
                  float tone,
                  float roll,
                  bool terminal) {
    direction.normalize();
    const QVector3D along = (direction + QVector3D(0.0F, lift, 0.0F)).normalized();
    const QVector3D side(-direction.z(), 0.0F, direction.x());
    const QVector3D across = side * std::cos(roll) +
                             QVector3D::crossProduct(side, along) * std::sin(roll);
    const QVector3D up = QVector3D::crossProduct(across, along).normalized();
    const QVector3D shoulder = base + along * (length * 0.42F) - up * (length * 0.053F);
    const auto start = static_cast<unsigned>(mesh.vertices.size());
    const auto blade_vertex = [&](const QVector3D& p, const QVector3D& normal) {
      mesh.vertices.push_back({p, normal, std::atan2(base.z(), base.x()) / k_tau + 0.5F,
                               0.58F + p.y() * 0.30F, tone});
    };
    const QVector3D left = (up + across * 0.34F).normalized();
    const QVector3D right = (up - across * 0.34F).normalized();
    blade_vertex(base, up);
    blade_vertex(shoulder + across * (length * 0.17F), left);
    blade_vertex(base + along * length, (up + along * 0.22F).normalized());
    blade_vertex(shoulder - across * (length * 0.15F) - along * (length * 0.06F), right);
    if (terminal) {
      // The exposed end of the shoot has a longer, recurved blade.
      const QVector3D neck = base + along * (length * 0.72F) + up * (length * 0.02F);
      blade_vertex(neck + across * (length * 0.11F), left);
      blade_vertex(neck - across * (length * 0.11F), right);
      triangle(start, start + 1U, start + 4U);
      triangle(start, start + 4U, start + 5U);
      triangle(start, start + 5U, start + 3U);
      triangle(start + 4U, start + 2U, start + 5U);
    } else {
      triangle(start, start + 1U, start + 2U);
      triangle(start, start + 2U, start + 3U);
    }
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
  const QVector3D root(0.032F, -0.008F, 0.015F);
  const QVector3D elbow_low(-0.042F, 0.23F, -0.026F);
  const QVector3D fork(-0.12F, 0.36F, 0.055F);
  tree.olive_limb(root, elbow_low, fork, 0.138F, 0.060F, 11U);

  // Unequal scaffolding limbs carry broad, flattened crowns. The open centre
  // lets the fork and the undersides of individual boughs read in an orchard.
  for (unsigned branch = 0; branch < 5; ++branch) {
    const unsigned seed = 211U + branch * 73U;
    const float angle = branch * k_tau / 5.0F + noise(seed) * 0.35F;
    const QVector3D dir(std::cos(angle), 0.0F, std::sin(angle));
    const QVector3D side(-dir.z(), 0.0F, dir.x());
    const float reach = 0.31F + noise(seed + 1U) * 0.13F;
    const float y = 0.67F + noise(seed + 2U) * 0.35F;
    const QVector3D elbow = fork + dir * (reach * 0.92F) + side * 0.085F +
                            QVector3D(0.0F, 0.02F + noise(seed + 4U) * 0.15F, 0.0F);
    const QVector3D tip = dir * reach + QVector3D(0.0F, y, 0.0F);
    const float inset = 0.28F + 0.21F * noise(seed + 8U);
    const QVector3D base = fork * (1.0F - inset) + elbow_low * inset;
    tree.olive_limb(base, elbow, tip, 0.048F + 0.014F * noise(seed), 0.007F, seed);
    tree.olive_crown(tip, QVector3D(0.235F, 0.112F, 0.16F), angle, seed + 5U);
    for (int twig = -1; twig <= 1; twig += 2) {
      const QVector3D end = tip - dir * 0.13F + side * (0.16F * twig) +
                            QVector3D(0.0F, 0.055F + noise(seed + 7U) * 0.065F, 0.0F);
      const QVector3D junction = (base + elbow * 2.0F + tip) * 0.25F;
      tree.olive_limb(junction, end - dir * 0.082F, end, 0.032F, 0.006F, seed + 11U);
      tree.olive_crown(end,
                      QVector3D(0.185F, 0.097F, 0.145F),
                      angle + twig * 0.6F,
                      seed + (twig < 0 ? 19U : 29U));
    }
  }
  return std::move(tree.mesh);
}

} // namespace Render::GL::BackendPipelines
