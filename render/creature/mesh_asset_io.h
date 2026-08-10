#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>

#include "animation/bpat/bpat_format.h"
#include "part_graph.h"

namespace Render::Creature::MeshAssetIo {

inline constexpr std::size_t k_write_chunk_bytes = 1U << 20;

inline auto write_pod(std::ostream& out, const void* src, std::size_t bytes) -> bool {
  auto const* cursor = static_cast<const char*>(src);
  while (bytes != 0U) {
    std::size_t const chunk_size = std::min<std::size_t>(
        bytes,
        std::min<std::size_t>(
            k_write_chunk_bytes,
            static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())));
    out.write(cursor, static_cast<std::streamsize>(chunk_size));
    if (!out.good()) {
      return false;
    }
    cursor += chunk_size;
    bytes -= chunk_size;
  }
  return true;
}

inline auto pad_to_alignment(std::ostream& out,
                             std::uint64_t current,
                             std::uint64_t alignment) -> bool {
  std::uint64_t const padded = Render::Creature::Bpat::align_up(current, alignment);
  static constexpr std::array<char, 16> zeros{};
  for (std::uint64_t remaining = padded - current; remaining != 0U;) {
    auto const chunk =
        static_cast<std::streamsize>(std::min<std::uint64_t>(remaining, zeros.size()));
    out.write(zeros.data(), chunk);
    if (!out.good()) {
      return false;
    }
    remaining -= static_cast<std::uint64_t>(chunk);
  }
  return true;
}

inline auto lod_from_u32(std::uint32_t raw,
                         Render::Creature::CreatureLOD& out) -> bool {
  switch (raw) {
  case static_cast<std::uint32_t>(Render::Creature::CreatureLOD::Full):
    out = Render::Creature::CreatureLOD::Full;
    return true;
  case static_cast<std::uint32_t>(Render::Creature::CreatureLOD::Minimal):
    out = Render::Creature::CreatureLOD::Minimal;
    return true;
  default:
    return false;
  }
}

} // namespace Render::Creature::MeshAssetIo
