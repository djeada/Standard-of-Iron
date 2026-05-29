#pragma once

#include <QMatrix4x4>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "bpat_format.h"

namespace Render::Creature::Bpat {

struct ClipView {
  std::string_view name{};
  std::uint32_t frame_count{0U};
  std::uint32_t frame_offset{0U};
  float fps{0.0F};
  bool loops{false};
  // Authored animation markers as normalized clip phase in [0,1]; -1 means unset.
  float marker_anticipation_start{-1.0F};
  float marker_weapon_release{-1.0F};
  float marker_contact{-1.0F};
  float marker_recover_unlocked{-1.0F};
  float marker_exit_safe{-1.0F};
};

struct SocketView {
  std::string_view name{};
  std::uint32_t anchor_bone{0U};
  float local_offset[3]{};
};

class BpatBlob {
public:
  static constexpr std::uint32_t k_invalid_clip_index = 0xFFFFu;

  static auto from_bytes(std::vector<std::uint8_t> bytes) -> BpatBlob;
  static auto from_file(const std::string& path) -> BpatBlob;

  [[nodiscard]] auto loaded() const noexcept -> bool { return m_loaded; }
  [[nodiscard]] auto last_error() const noexcept -> std::string_view {
    return m_last_error;
  }

  [[nodiscard]] auto species_id() const noexcept -> std::uint32_t;
  [[nodiscard]] auto bone_count() const noexcept -> std::uint32_t;
  [[nodiscard]] auto frame_total() const noexcept -> std::uint32_t;
  [[nodiscard]] auto clip_count() const noexcept -> std::uint32_t;
  [[nodiscard]] auto socket_count() const noexcept -> std::uint32_t;

  [[nodiscard]] auto clip(std::uint32_t index) const -> ClipView;
  [[nodiscard]] auto clip_index(std::string_view name) const -> std::uint32_t;
  [[nodiscard]] auto socket(std::uint32_t index) const -> SocketView;

  [[nodiscard]] auto
  palette_matrix(std::uint32_t global_frame_index,
                 std::uint32_t bone_index) const -> std::span<const float>;

  [[nodiscard]] auto frame_palette_view(std::uint32_t global_frame_index) const
      -> std::span<const QMatrix4x4>;

  [[nodiscard]] auto
  socket_matrix(std::uint32_t global_frame_index,
                std::uint32_t socket_index) const -> std::span<const float>;

  [[nodiscard]] auto raw_bytes() const noexcept -> std::span<const std::uint8_t> {
    return {m_bytes.data(), m_bytes.size()};
  }

private:
  struct ClipIndexEntry {
    std::string name{};
    std::uint32_t clip_index{k_invalid_clip_index};
  };

  static constexpr std::uint32_t k_empty_clip_index_bucket = 0xFFFFFFFFu;

  bool validate();
  void decode_palette_cache();

  std::vector<std::uint8_t> m_bytes{};
  bool m_loaded{false};
  std::string m_last_error{};
  const BpatHeader* m_header{nullptr};
  const BpatClipEntry* m_clip_table{nullptr};
  const BpatSocketEntry* m_socket_table{nullptr};
  const char* m_string_table{nullptr};
  const float* m_palette_data{nullptr};
  const float* m_socket_data{nullptr};

  std::vector<QMatrix4x4> m_decoded_palette{};
  std::vector<ClipIndexEntry> m_clip_index_entries{};
  std::vector<std::uint32_t> m_clip_index_buckets{};
};

} // namespace Render::Creature::Bpat
