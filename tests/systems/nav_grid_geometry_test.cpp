#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "game/systems/nav_grid_types.h"

namespace {

using Game::Systems::body_cell_range;
using Game::Systems::cell_count;
using Game::Systems::cell_gap;
using Game::Systems::for_each_ring_cell;

auto collect_ring(int ring) -> std::vector<std::pair<int, int>> {
  std::vector<std::pair<int, int>> cells;
  for_each_ring_cell(ring, [&cells](int dx, int dz) { cells.emplace_back(dx, dz); });
  return cells;
}

auto collect_ring_by_full_square_scan(int ring) -> std::vector<std::pair<int, int>> {
  std::vector<std::pair<int, int>> cells;
  for (int dz = -ring; dz <= ring; ++dz) {
    for (int dx = -ring; dx <= ring; ++dx) {
      if (std::abs(dx) != ring && std::abs(dz) != ring) {
        continue;
      }
      cells.emplace_back(dx, dz);
    }
  }
  return cells;
}

TEST(ForEachRingCell, RingZeroIsTheOriginCell) {
  EXPECT_EQ(collect_ring(0), (std::vector<std::pair<int, int>>{{0, 0}}));
  EXPECT_EQ(collect_ring(-3), (std::vector<std::pair<int, int>>{{0, 0}}));
}

TEST(ForEachRingCell, RingOneVisitsTheEightNeighboursInRowOrder) {
  const std::vector<std::pair<int, int>> expected{
      {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
  EXPECT_EQ(collect_ring(1), expected);
}

TEST(ForEachRingCell, OrderMatchesTheFullSquareScanItReplaced) {
  for (int ring = 1; ring <= 12; ++ring) {
    EXPECT_EQ(collect_ring(ring), collect_ring_by_full_square_scan(ring))
        << "ring " << ring << " changed visit order, which changes tie-breaking";
  }
}

TEST(ForEachRingCell, VisitsExactlyThePerimeter) {
  for (int ring = 1; ring <= 20; ++ring) {
    EXPECT_EQ(collect_ring(ring).size(), static_cast<std::size_t>(8 * ring));
  }
}

TEST(BodyCellRange, CoversTheWholeBodyCircle) {
  const auto box = body_cell_range(10.0F, 20.0F, 0.45F, 0.5F);
  EXPECT_LE(box.min_x, 9);
  EXPECT_GE(box.max_x, 11);
  EXPECT_LE(box.min_z, 19);
  EXPECT_GE(box.max_z, 21);
  EXPECT_GT(cell_count(box), 0);
}

TEST(BodyCellRange, EmptyRangeCountsZeroCells) {
  EXPECT_EQ(cell_count({.min_x = 5, .max_x = 4, .min_z = 0, .max_z = 0}), 0);
  EXPECT_EQ(cell_count({.min_x = 0, .max_x = 0, .min_z = 5, .max_z = 4}), 0);
  EXPECT_EQ(cell_count({.min_x = 0, .max_x = 0, .min_z = 0, .max_z = 0}), 1);
}

TEST(CellGap, IsZeroInsideTheCellFootprint) {
  const auto gap = cell_gap(7.2F, 3.1F, 7, 3, 0.5F);
  EXPECT_FLOAT_EQ(gap.u, 0.0F);
  EXPECT_FLOAT_EQ(gap.v, 0.0F);
  EXPECT_FLOAT_EQ(gap.squared(), 0.0F);
  EXPECT_FLOAT_EQ(gap.length(), 0.0F);
}

TEST(CellGap, MeasuresTheDistanceToTheCellEdge) {
  const auto gap = cell_gap(9.0F, 3.0F, 7, 3, 0.5F);
  EXPECT_FLOAT_EQ(gap.u, 1.5F);
  EXPECT_FLOAT_EQ(gap.v, 0.0F);
  EXPECT_FLOAT_EQ(gap.length(), 1.5F);
  EXPECT_FLOAT_EQ(gap.squared(), 2.25F);
}

TEST(CellGap, SquaredAgreesWithLength) {
  for (float u = 0.0F; u < 4.0F; u += 0.37F) {
    for (float v = 0.0F; v < 4.0F; v += 0.53F) {
      const auto gap = cell_gap(u, v, 1, 2, 0.5F);
      EXPECT_NEAR(gap.length() * gap.length(), gap.squared(), 1e-4F);
    }
  }
}

} // namespace
