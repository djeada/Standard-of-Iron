#include <QVector3D>

#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <set>

#include "game/accessibility/team_identity.h"
#include "render/geom/selection_ring.h"
#include "render/gl/mesh.h"

namespace {

using Game::Accessibility::TeamPattern;

constexpr std::array<TeamPattern, 6> k_all_patterns{
    TeamPattern::Solid,
    TeamPattern::Dashed,
    TeamPattern::DoubleRing,
    TeamPattern::Notched,
    TeamPattern::Dotted,
    TeamPattern::Chevron,
};

auto triangle_normal_y(const Render::GL::Vertex& a,
                       const Render::GL::Vertex& b,
                       const Render::GL::Vertex& c) -> float {
  QVector3D const pa(a.position[0], a.position[1], a.position[2]);
  QVector3D const pb(b.position[0], b.position[1], b.position[2]);
  QVector3D const pc(c.position[0], c.position[1], c.position[2]);
  return QVector3D::crossProduct(pb - pa, pc - pa).y();
}

auto bearing_degrees(const Render::GL::Vertex& vertex) -> float {
  float degrees = std::atan2(vertex.position[2], vertex.position[0]) * 180.0F /
                  static_cast<float>(M_PI);
  return degrees < 0.0F ? degrees + 360.0F : degrees;
}

auto angular_coverage(const Render::GL::Mesh& mesh) -> float {
  constexpr int k_buckets = 360;
  std::array<bool, k_buckets> covered{};

  const auto& vertices = mesh.get_vertices();
  const auto& indices = mesh.get_indices();

  for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
    std::array<float, 3> bearings{bearing_degrees(vertices[indices[i]]),
                                  bearing_degrees(vertices[indices[i + 1]]),
                                  bearing_degrees(vertices[indices[i + 2]])};
    const float lowest = *std::min_element(bearings.begin(), bearings.end());
    const float highest = *std::max_element(bearings.begin(), bearings.end());

    float start = lowest;
    float sweep = highest - lowest;
    if (sweep > 180.0F) {
      start = highest;
      sweep = 360.0F - sweep;
    }

    for (float offset = 0.0F; offset <= sweep; offset += 0.25F) {
      const int bucket = static_cast<int>(start + offset) % k_buckets;
      covered[static_cast<std::size_t>(bucket)] = true;
    }
  }

  int count = 0;
  for (const bool bucket : covered) {
    count += bucket ? 1 : 0;
  }
  return static_cast<float>(count) / k_buckets;
}

TEST(SelectionRingPattern, EveryPatternProducesADrawableMesh) {
  for (const auto pattern : k_all_patterns) {
    auto* mesh = Render::Geom::SelectionRing::get(pattern);
    ASSERT_NE(mesh, nullptr) << static_cast<int>(pattern);
    EXPECT_FALSE(mesh->get_vertices().empty()) << static_cast<int>(pattern);
    EXPECT_GE(mesh->get_indices().size(), 6U) << static_cast<int>(pattern);
    EXPECT_EQ(mesh->get_indices().size() % 3, 0U) << static_cast<int>(pattern);
  }
}

TEST(SelectionRingPattern, EveryPatternFacesUpwardForOverheadRendering) {
  for (const auto pattern : k_all_patterns) {
    auto* mesh = Render::Geom::SelectionRing::get(pattern);
    ASSERT_NE(mesh, nullptr);

    auto const& vertices = mesh->get_vertices();
    auto const& indices = mesh->get_indices();
    ASSERT_GE(indices.size(), 6U);

    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
      EXPECT_GT(triangle_normal_y(vertices[indices[i]],
                                  vertices[indices[i + 1]],
                                  vertices[indices[i + 2]]),
                0.0F)
          << "pattern " << static_cast<int>(pattern) << " triangle " << (i / 3);
    }
  }
}

TEST(SelectionRingPattern, TheDefaultRingIsStillTheSolidOne) {
  EXPECT_EQ(Render::Geom::SelectionRing::get(),
            Render::Geom::SelectionRing::get(TeamPattern::Solid));
}

TEST(SelectionRingPattern, MeshesAreCachedRatherThanRebuiltPerFrame) {
  auto* first = Render::Geom::SelectionRing::get(TeamPattern::Dashed);
  EXPECT_EQ(first, Render::Geom::SelectionRing::get(TeamPattern::Dashed));
}

TEST(SelectionRingPattern, EachPatternIsItsOwnMesh) {
  std::set<Render::GL::Mesh*> meshes;
  for (const auto pattern : k_all_patterns) {
    meshes.insert(Render::Geom::SelectionRing::get(pattern));
  }
  EXPECT_EQ(meshes.size(), k_all_patterns.size());
}

TEST(SelectionRingPattern, BrokenPatternsAreVisiblyBrokenAndSolidOnesAreNot) {

  EXPECT_GT(angular_coverage(*Render::Geom::SelectionRing::get(TeamPattern::Solid)),
            0.98F);
  EXPECT_GT(
      angular_coverage(*Render::Geom::SelectionRing::get(TeamPattern::DoubleRing)),
      0.98F);
  EXPECT_GT(angular_coverage(*Render::Geom::SelectionRing::get(TeamPattern::Chevron)),
            0.98F);

  EXPECT_LT(angular_coverage(*Render::Geom::SelectionRing::get(TeamPattern::Dashed)),
            0.80F);
  EXPECT_LT(angular_coverage(*Render::Geom::SelectionRing::get(TeamPattern::Dotted)),
            0.60F);
  EXPECT_LT(angular_coverage(*Render::Geom::SelectionRing::get(TeamPattern::Notched)),
            0.95F);
}

TEST(SelectionRingPattern, PatternsShareTheSameOuterRadiusSoTheyReadAsOneFamily) {
  for (const auto pattern : k_all_patterns) {
    auto* mesh = Render::Geom::SelectionRing::get(pattern);
    ASSERT_NE(mesh, nullptr);

    float max_radius = 0.0F;
    for (const auto& vertex : mesh->get_vertices()) {
      max_radius =
          std::max(max_radius, std::hypot(vertex.position[0], vertex.position[2]));
    }

    const float allowance = pattern == TeamPattern::Chevron ? 1.17F : 1.001F;
    EXPECT_LE(max_radius, allowance) << static_cast<int>(pattern);
    EXPECT_GE(max_radius, 0.999F) << static_cast<int>(pattern);
  }
}

TEST(SelectionRingPattern, AnOutOfRangePatternFallsBackToTheSolidRing) {
  const auto bogus = static_cast<TeamPattern>(999);
  EXPECT_EQ(Render::Geom::SelectionRing::get(bogus),
            Render::Geom::SelectionRing::get(TeamPattern::Solid));
}

} // namespace
