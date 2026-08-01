#include "explored_mask_codec.h"

#include <algorithm>
#include <cstddef>

#include "visibility_service.h"

namespace Game::Map {

namespace {

constexpr std::uint32_t k_max_run = 0xFFFFU;

void append_run(QByteArray& out, std::uint32_t length) {
  out.append(static_cast<char>(length & 0xFFU));
  out.append(static_cast<char>((length >> 8U) & 0xFFU));
}

} // namespace

auto encode_explored_mask(const ExploredMask& mask) -> QString {
  if (!mask.is_valid()) {
    return {};
  }

  QByteArray runs;
  std::uint8_t current = 0U;
  std::uint32_t run_length = 0U;

  for (const std::uint8_t value : mask.explored) {
    const std::uint8_t normalized = value != 0U ? 1U : 0U;
    if (normalized != current || run_length == k_max_run) {
      append_run(runs, run_length);
      if (normalized != current) {
        current = normalized;
      } else {

        append_run(runs, 0U);
      }
      run_length = 0U;
    }
    ++run_length;
  }
  append_run(runs, run_length);

  return QString::fromLatin1(runs.toBase64());
}

auto decode_explored_mask(const QString& encoded,
                          int width,
                          int height) -> ExploredMask {
  ExploredMask mask;
  if (encoded.isEmpty() || width <= 0 || height <= 0) {
    return mask;
  }

  const QByteArray runs = QByteArray::fromBase64(
      encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
  if (runs.isEmpty() || (runs.size() % 2) != 0) {
    return mask;
  }

  const auto expected =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  std::vector<std::uint8_t> cells;
  cells.reserve(expected);

  std::uint8_t value = 0U;
  for (int index = 0; index + 1 < runs.size(); index += 2) {
    const auto low =
        static_cast<std::uint32_t>(static_cast<unsigned char>(runs[index]));
    const auto high =
        static_cast<std::uint32_t>(static_cast<unsigned char>(runs[index + 1]));
    const std::uint32_t length = low | (high << 8U);
    if (cells.size() + length > expected) {
      return {};
    }
    cells.insert(cells.end(), length, value);
    value = value != 0U ? 0U : 1U;
  }

  if (cells.size() != expected) {
    return {};
  }

  mask.width = width;
  mask.height = height;
  mask.explored = std::move(cells);
  return mask;
}

auto explored_mask_from_cells(const std::vector<std::uint8_t>& cells,
                              int width,
                              int height) -> ExploredMask {
  ExploredMask mask;
  const auto expected = static_cast<std::size_t>(std::max(0, width)) *
                        static_cast<std::size_t>(std::max(0, height));
  if (expected == 0 || cells.size() != expected) {
    return mask;
  }

  mask.width = width;
  mask.height = height;
  mask.explored.resize(expected);
  for (std::size_t idx = 0; idx < expected; ++idx) {
    mask.explored[idx] =
        static_cast<VisibilityState>(cells[idx]) != VisibilityState::Unseen ? 1U : 0U;
  }
  return mask;
}

} // namespace Game::Map
