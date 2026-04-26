#include "bpat_reader.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

namespace Render::Creature::Bpat {

auto BpatBlob::from_bytes(std::vector<std::uint8_t> bytes) -> BpatBlob {
  BpatBlob blob{};
  blob.m_bytes = std::move(bytes);
  blob.m_loaded = blob.validate();
  return blob;
}

auto BpatBlob::from_file(const std::string &path) -> BpatBlob {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    BpatBlob blob{};
    blob.m_last_error = "failed to open " + path;
    return blob;
  }
  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
  return from_bytes(std::move(data));
}

bool BpatBlob::validate() {
  m_last_error.clear();
  m_header = nullptr;
  m_clip_table = nullptr;
  m_socket_table = nullptr;
  m_string_table = nullptr;
  m_palette_data = nullptr;
  m_socket_data = nullptr;

  if (m_bytes.size() < sizeof(BpatHeader)) {
    m_last_error = "file shorter than header";
    return false;
  }
  auto const *header = reinterpret_cast<const BpatHeader *>(m_bytes.data());
  if (std::memcmp(header->magic, kMagic.data(), kMagic.size()) != 0) {
    m_last_error = "magic mismatch";
    return false;
  }
  if (header->version != kVersion) {
    m_last_error = "unsupported version";
    return false;
  }
  if (header->species_id > kSpeciesElephant) {
    m_last_error = "unknown species_id";
    return false;
  }
  if (header->bone_count == 0U || header->bone_count > 64U) {
    m_last_error = "bone_count out of range";
    return false;
  }
  if (header->clip_count == 0U) {
    m_last_error = "clip_count == 0";
    return false;
  }

  std::uint64_t const file_size = m_bytes.size();
  auto in_bounds = [file_size](std::uint64_t offset, std::uint64_t bytes) {
    return offset <= file_size && offset + bytes <= file_size;
  };

  if (!in_bounds(header->clip_table_offset,
                 std::uint64_t{header->clip_count} * sizeof(BpatClipEntry))) {
    m_last_error = "clip table out of bounds";
    return false;
  }
  if (header->socket_count > 0U &&
      !in_bounds(header->socket_table_offset,
                 std::uint64_t{header->socket_count} *
                     sizeof(BpatSocketEntry))) {
    m_last_error = "socket table out of bounds";
    return false;
  }
  if (!in_bounds(header->string_table_offset, header->string_table_size)) {
    m_last_error = "string table out of bounds";
    return false;
  }

  std::uint64_t const palette_bytes = std::uint64_t{header->frame_total} *
                                      header->bone_count * kMatrixFloats *
                                      sizeof(float);
  if (!in_bounds(header->palette_data_offset, palette_bytes)) {
    m_last_error = "palette data out of bounds";
    return false;
  }

  std::uint64_t const socket_bytes =
      header->socket_count > 0U
          ? std::uint64_t{header->frame_total} * header->socket_count *
                kSocketMatrixFloats * sizeof(float)
          : 0U;
  std::uint64_t const socket_data_offset =
      header->socket_count > 0U ? header->palette_data_offset + palette_bytes
                                : 0U;
  if (header->socket_count > 0U &&
      !in_bounds(socket_data_offset, socket_bytes)) {
    m_last_error = "socket data out of bounds";
    return false;
  }

  m_clip_table = reinterpret_cast<const BpatClipEntry *>(
      m_bytes.data() + header->clip_table_offset);
  m_socket_table = header->socket_count > 0U
                       ? reinterpret_cast<const BpatSocketEntry *>(
                             m_bytes.data() + header->socket_table_offset)
                       : nullptr;
  m_string_table = reinterpret_cast<const char *>(m_bytes.data() +
                                                  header->string_table_offset);
  m_palette_data = reinterpret_cast<const float *>(m_bytes.data() +
                                                   header->palette_data_offset);
  m_socket_data =
      header->socket_count > 0U
          ? reinterpret_cast<const float *>(m_bytes.data() + socket_data_offset)
          : nullptr;

  std::uint32_t computed_total = 0U;
  for (std::uint32_t i = 0; i < header->clip_count; ++i) {
    auto const &c = m_clip_table[i];
    if (c.frame_count == 0U) {
      m_last_error = "clip frame_count == 0";
      return false;
    }
    if (c.frame_offset != computed_total) {
      m_last_error = "clip frame_offset not contiguous";
      return false;
    }
    computed_total += c.frame_count;
    if (std::uint64_t{c.name_offset} + c.name_length + 1U >
        header->string_table_size) {
      m_last_error = "clip name out of string table";
      return false;
    }
    if (m_string_table[c.name_offset + c.name_length] != '\0') {
      m_last_error = "clip name not NUL-terminated";
      return false;
    }
  }
  if (computed_total != header->frame_total) {
    m_last_error = "Σ frame_count != frame_total";
    return false;
  }

  for (std::uint32_t i = 0; i < header->socket_count; ++i) {
    auto const &s = m_socket_table[i];
    if (s.anchor_bone >= header->bone_count) {
      m_last_error = "socket anchor_bone out of range";
      return false;
    }
    if (std::uint64_t{s.name_offset} + s.name_length + 1U >
        header->string_table_size) {
      m_last_error = "socket name out of string table";
      return false;
    }
    if (m_string_table[s.name_offset + s.name_length] != '\0') {
      m_last_error = "socket name not NUL-terminated";
      return false;
    }
  }

  m_header = header;
  decode_palette_cache();
  return true;
}

auto BpatBlob::species_id() const noexcept -> std::uint32_t {
  return m_header != nullptr ? m_header->species_id : 0U;
}
auto BpatBlob::bone_count() const noexcept -> std::uint32_t {
  return m_header != nullptr ? m_header->bone_count : 0U;
}
auto BpatBlob::frame_total() const noexcept -> std::uint32_t {
  return m_header != nullptr ? m_header->frame_total : 0U;
}
auto BpatBlob::clip_count() const noexcept -> std::uint32_t {
  return m_header != nullptr ? m_header->clip_count : 0U;
}
auto BpatBlob::socket_count() const noexcept -> std::uint32_t {
  return m_header != nullptr ? m_header->socket_count : 0U;
}

auto BpatBlob::clip(std::uint32_t index) const -> ClipView {
  ClipView v{};
  if (m_header == nullptr || index >= m_header->clip_count) {
    return v;
  }
  auto const &e = m_clip_table[index];
  v.name = std::string_view(m_string_table + e.name_offset, e.name_length);
  v.frame_count = e.frame_count;
  v.frame_offset = e.frame_offset;
  v.fps = e.fps;
  v.loops = e.loops != 0U;
  return v;
}

auto BpatBlob::socket(std::uint32_t index) const -> SocketView {
  SocketView v{};
  if (m_header == nullptr || index >= m_header->socket_count) {
    return v;
  }
  auto const &e = m_socket_table[index];
  v.name = std::string_view(m_string_table + e.name_offset, e.name_length);
  v.anchor_bone = e.anchor_bone;
  v.local_offset[0] = e.local_offset[0];
  v.local_offset[1] = e.local_offset[1];
  v.local_offset[2] = e.local_offset[2];
  return v;
}

auto BpatBlob::palette_matrix(std::uint32_t global_frame_index,
                              std::uint32_t bone_index) const
    -> std::span<const float> {
  if (m_header == nullptr || global_frame_index >= m_header->frame_total ||
      bone_index >= m_header->bone_count) {
    return {};
  }
  std::uint64_t const offset =
      ((std::uint64_t{global_frame_index} * m_header->bone_count) +
       bone_index) *
      kMatrixFloats;
  return std::span<const float>(m_palette_data + offset, kMatrixFloats);
}

auto BpatBlob::frame_palette_view(std::uint32_t global_frame_index) const
    -> std::span<const QMatrix4x4> {
  if (m_header == nullptr || global_frame_index >= m_header->frame_total ||
      m_decoded_palette.empty()) {
    return {};
  }
  std::size_t const off =
      static_cast<std::size_t>(global_frame_index) * m_header->bone_count;
  return std::span<const QMatrix4x4>(m_decoded_palette.data() + off,
                                     m_header->bone_count);
}

auto BpatBlob::socket_matrix(std::uint32_t global_frame_index,
                             std::uint32_t socket_index) const
    -> std::span<const float> {
  if (m_header == nullptr || m_socket_data == nullptr ||
      global_frame_index >= m_header->frame_total ||
      socket_index >= m_header->socket_count) {
    return {};
  }
  std::uint64_t const offset =
      ((std::uint64_t{global_frame_index} * m_header->socket_count) +
       socket_index) *
      kSocketMatrixFloats;
  return std::span<const float>(m_socket_data + offset, kSocketMatrixFloats);
}

void BpatBlob::decode_palette_cache() {
  if (m_header == nullptr || m_palette_data == nullptr) {
    return;
  }
  std::size_t const total =
      static_cast<std::size_t>(m_header->frame_total) * m_header->bone_count;
  if (total == 0) {
    return;
  }
  m_decoded_palette.assign(total, QMatrix4x4{});
  for (std::size_t i = 0; i < total; ++i) {
    m_decoded_palette[i] = QMatrix4x4(m_palette_data + i * kMatrixFloats);
  }
}

} // namespace Render::Creature::Bpat
