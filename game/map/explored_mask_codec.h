#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <vector>

namespace Game::Map {

struct ExploredMask {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> explored;

  [[nodiscard]] auto is_valid() const -> bool {
    return width > 0 && height > 0 &&
           explored.size() ==
               static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  }
};

[[nodiscard]] auto encode_explored_mask(const ExploredMask& mask) -> QString;

[[nodiscard]] auto
decode_explored_mask(const QString& encoded, int width, int height) -> ExploredMask;

[[nodiscard]] auto explored_mask_from_cells(const std::vector<std::uint8_t>& cells,
                                            int width,
                                            int height) -> ExploredMask;

} // namespace Game::Map
