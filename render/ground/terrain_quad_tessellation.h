#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace Render::Ground {

inline void append_quad_triangles(std::vector<unsigned int>& indices,
                                  unsigned int v00,
                                  unsigned int v10,
                                  unsigned int v01,
                                  unsigned int v11,
                                  bool rising_diagonal) {
  if (rising_diagonal) {
    indices.insert(indices.end(), {v00, v10, v11, v00, v11, v01});
    return;
  }
  indices.insert(indices.end(), {v00, v10, v01, v01, v10, v11});
}

inline void append_subdivided_quad_triangles(std::vector<unsigned int>& indices,
                                             const std::array<unsigned int, 9>& grid,
                                             bool rising_diagonal) {
  const auto at = [&](int x, int z) {
    return grid[static_cast<std::size_t>(z * 3 + x)];
  };
  for (int z = 0; z < 2; ++z) {
    for (int x = 0; x < 2; ++x) {
      append_quad_triangles(indices,
                            at(x, z),
                            at(x + 1, z),
                            at(x, z + 1),
                            at(x + 1, z + 1),
                            rising_diagonal);
    }
  }
}

} // namespace Render::Ground
