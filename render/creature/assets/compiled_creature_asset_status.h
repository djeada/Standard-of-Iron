#pragma once

#include <cstddef>
#include <string>

namespace Render::Creature {

struct CompiledCreatureAssetStatus {
  bool loaded{false};
  std::size_t vertex_count{0U};
  std::size_t triangle_count{0U};
  std::size_t joint_count{0U};
  std::size_t clip_count{0U};
  std::string error;
};

} // namespace Render::Creature
