#pragma once

#include "bpat_format.h"
#include "bpat_reader.h"

#include <QMatrix4x4>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace Render::Creature::Bpat {

class BpatRegistry {
public:
  static constexpr std::uint32_t kInvalidClip = 0xFFFFu;

  [[nodiscard]] static auto instance() noexcept -> BpatRegistry &;

  auto load_species(std::uint32_t species_id, const std::string &path) -> bool;

  auto load_all(const std::string &asset_root) -> std::size_t;

  [[nodiscard]] auto
  blob(std::uint32_t species_id) const noexcept -> const BpatBlob *;

  [[nodiscard]] auto
  has_species(std::uint32_t species_id) const noexcept -> bool;

  [[nodiscard]] auto find_clip(std::uint32_t species_id,
                               std::string_view name) const -> std::uint32_t;

  auto sample_palette(std::uint32_t species_id, std::uint32_t clip_index,
                      std::uint32_t frame_in_clip,
                      std::span<QMatrix4x4> out) const -> std::uint32_t;

  auto sample_socket(std::uint32_t species_id, std::uint32_t clip_index,
                     std::uint32_t frame_in_clip, std::uint32_t socket_index,
                     QMatrix4x4 &out) const -> bool;

  [[nodiscard]] auto last_error() const noexcept -> std::string_view {
    return m_last_error;
  }

private:
  BpatRegistry() = default;

  [[nodiscard]] auto species_slot(std::uint32_t id) noexcept -> BpatBlob *;
  [[nodiscard]] auto
  species_slot(std::uint32_t id) const noexcept -> const BpatBlob *;

  std::array<BpatBlob, kSpeciesCount> m_blobs{};
  std::string m_last_error{};
};

} // namespace Render::Creature::Bpat
