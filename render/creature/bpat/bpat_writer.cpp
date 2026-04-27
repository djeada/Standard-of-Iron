#include "bpat_writer.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <ostream>
#include <unordered_map>

namespace Render::Creature::Bpat {

namespace {

void write_pod(std::ostream &out, const void *src, std::size_t bytes) {
  out.write(static_cast<const char *>(src),
            static_cast<std::streamsize>(bytes));
}

void pad_to_alignment(std::ostream &out, std::uint64_t current,
                      std::uint64_t alignment) {
  std::uint64_t const padded = align_up(current, alignment);
  std::uint64_t const pad = padded - current;
  static constexpr std::array<char, 16> zeros{};
  for (std::uint64_t remaining = pad; remaining != 0U;) {
    auto const chunk = static_cast<std::streamsize>(
        std::min<std::uint64_t>(remaining, zeros.size()));
    out.write(zeros.data(), chunk);
    remaining -= static_cast<std::uint64_t>(chunk);
  }
}

void copy_matrix_row_major(const QMatrix4x4 &m, float *dst) {

  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      dst[(row * 4) + col] = m(row, col);
    }
  }
}

void copy_socket_row_major(const QMatrix4x4 &m, float *dst) {
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 4; ++col) {
      dst[(row * 4) + col] = m(row, col);
    }
  }
}

} // namespace

BpatWriter::BpatWriter(std::uint32_t species_id,
                       std::uint32_t bone_count) noexcept
    : m_species_id(species_id), m_bone_count(bone_count) {}

void BpatWriter::add_clip(ClipDescriptor desc) {
  PendingClip pending{};
  pending.frame_offset = m_frame_total;
  m_frame_total += desc.frame_count;
  pending.desc = std::move(desc);
  m_clips.push_back(std::move(pending));
}

void BpatWriter::add_socket(SocketDescriptor desc) {
  m_sockets.push_back(std::move(desc));
}

void BpatWriter::append_clip_palettes(
    std::span<const QMatrix4x4> palette_frames) {
  assert(!m_clips.empty() && "add_clip() before append_clip_palettes()");
  auto &pending = m_clips.back();
  assert(!pending.palette_appended && "palettes already appended for clip");
  std::size_t const expected =
      static_cast<std::size_t>(m_bone_count) * pending.desc.frame_count;
  assert(palette_frames.size() == expected &&
         "palette_frames.size() must equal bone_count * frame_count");
  std::size_t const base_floats = m_palette_floats.size();
  m_palette_floats.resize(base_floats + (expected * kMatrixFloats));
  for (std::size_t i = 0; i < palette_frames.size(); ++i) {
    copy_matrix_row_major(palette_frames[i], m_palette_floats.data() +
                                                 base_floats +
                                                 (i * kMatrixFloats));
  }
  pending.palette_appended = true;
}

void BpatWriter::append_clip_socket_transforms(
    std::span<const QMatrix4x4> socket_frames) {
  assert(!m_clips.empty() &&
         "add_clip() before append_clip_socket_transforms()");
  assert(!m_sockets.empty() && "add_socket() before appending socket data");
  auto &pending = m_clips.back();
  assert(!pending.sockets_appended && "sockets already appended for clip");
  std::size_t const expected = m_sockets.size() * pending.desc.frame_count;
  assert(socket_frames.size() == expected &&
         "socket_frames.size() must equal socket_count * frame_count");
  std::size_t const base_floats = m_socket_floats.size();
  m_socket_floats.resize(base_floats + (expected * kSocketMatrixFloats));
  for (std::size_t i = 0; i < socket_frames.size(); ++i) {
    copy_socket_row_major(socket_frames[i], m_socket_floats.data() +
                                                base_floats +
                                                (i * kSocketMatrixFloats));
  }
  pending.sockets_appended = true;
}

auto BpatWriter::write(std::ostream &out) const -> bool {
  if (m_clips.empty() || m_bone_count == 0U) {
    return false;
  }
  for (auto const &c : m_clips) {
    if (!c.palette_appended) {
      return false;
    }
    if (!m_sockets.empty() && !c.sockets_appended) {
      return false;
    }
    if (c.desc.frame_count == 0U) {
      return false;
    }
  }

  std::vector<char> string_table;
  std::unordered_map<std::string, std::uint32_t> string_offsets;
  auto intern = [&](const std::string &s) -> std::uint32_t {
    auto it = string_offsets.find(s);
    if (it != string_offsets.end()) {
      return it->second;
    }
    auto offset = static_cast<std::uint32_t>(string_table.size());
    string_table.insert(string_table.end(), s.begin(), s.end());
    string_table.push_back('\0');
    string_offsets.emplace(s, offset);
    return offset;
  };

  std::vector<BpatClipEntry> clip_entries;
  clip_entries.reserve(m_clips.size());
  for (auto const &c : m_clips) {
    BpatClipEntry e{};
    e.name_offset = intern(c.desc.name);
    e.name_length = static_cast<std::uint32_t>(c.desc.name.size());
    e.frame_count = c.desc.frame_count;
    e.frame_offset = c.frame_offset;
    e.fps = c.desc.fps;
    e.loops = c.desc.loops ? 1U : 0U;
    clip_entries.push_back(e);
  }

  std::vector<BpatSocketEntry> socket_entries;
  socket_entries.reserve(m_sockets.size());
  for (auto const &s : m_sockets) {
    BpatSocketEntry e{};
    e.name_offset = intern(s.name);
    e.name_length = static_cast<std::uint32_t>(s.name.size());
    e.anchor_bone = s.anchor_bone;
    e.local_offset[0] = s.local_offset.x();
    e.local_offset[1] = s.local_offset.y();
    e.local_offset[2] = s.local_offset.z();
    socket_entries.push_back(e);
  }

  std::uint64_t cursor = sizeof(BpatHeader);
  std::uint64_t const clip_table_offset = cursor;
  cursor += clip_entries.size() * sizeof(BpatClipEntry);
  std::uint64_t const socket_table_offset =
      socket_entries.empty() ? 0U : cursor;
  cursor += socket_entries.size() * sizeof(BpatSocketEntry);
  std::uint64_t const string_table_offset = cursor;
  cursor += string_table.size();
  cursor = align_up(cursor, kSectionAlignment);
  std::uint64_t const palette_data_offset = cursor;

  BpatHeader header{};
  std::memcpy(header.magic, kMagic.data(), kMagic.size());
  header.version = kVersion;
  header.species_id = m_species_id;
  header.bone_count = m_bone_count;
  header.socket_count = static_cast<std::uint32_t>(socket_entries.size());
  header.clip_count = static_cast<std::uint32_t>(clip_entries.size());
  header.frame_total = m_frame_total;
  header.string_table_size = static_cast<std::uint32_t>(string_table.size());
  header.clip_table_offset = clip_table_offset;
  header.socket_table_offset = socket_table_offset;
  header.string_table_offset = string_table_offset;
  header.palette_data_offset = palette_data_offset;

  std::uint64_t written = 0U;
  write_pod(out, &header, sizeof(header));
  written += sizeof(header);

  if (!clip_entries.empty()) {
    write_pod(out, clip_entries.data(),
              clip_entries.size() * sizeof(BpatClipEntry));
    written += clip_entries.size() * sizeof(BpatClipEntry);
  }
  if (!socket_entries.empty()) {
    write_pod(out, socket_entries.data(),
              socket_entries.size() * sizeof(BpatSocketEntry));
    written += socket_entries.size() * sizeof(BpatSocketEntry);
  }
  if (!string_table.empty()) {
    write_pod(out, string_table.data(), string_table.size());
    written += string_table.size();
  }
  pad_to_alignment(out, written, kSectionAlignment);

  if (!m_palette_floats.empty()) {
    write_pod(out, m_palette_floats.data(),
              m_palette_floats.size() * sizeof(float));
  }
  if (!m_socket_floats.empty()) {
    write_pod(out, m_socket_floats.data(),
              m_socket_floats.size() * sizeof(float));
  }
  return out.good();
}

} // namespace Render::Creature::Bpat
