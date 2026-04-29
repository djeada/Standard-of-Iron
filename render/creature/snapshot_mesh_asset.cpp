#include "snapshot_mesh_asset.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <ostream>
#include <unordered_map>

namespace Render::Creature::Snapshot {

namespace {

constexpr std::size_t k_write_chunk_bytes = 1U << 20;

auto write_pod(std::ostream &out, const void *src, std::size_t bytes) -> bool {
  auto const *cursor = static_cast<const char *>(src);
  while (bytes != 0U) {
    std::size_t const chunk_size = std::min<std::size_t>(
        bytes, std::min<std::size_t>(
                   k_write_chunk_bytes,
                   static_cast<std::size_t>(
                       std::numeric_limits<std::streamsize>::max())));
    out.write(cursor, static_cast<std::streamsize>(chunk_size));
    if (!out.good()) {
      return false;
    }
    cursor += chunk_size;
    bytes -= chunk_size;
  }
  return true;
}

auto pad_to_alignment(std::ostream &out, std::uint64_t current,
                      std::uint64_t alignment) -> bool {
  std::uint64_t const padded =
      Render::Creature::Bpat::align_up(current, alignment);
  std::uint64_t const pad = padded - current;
  static constexpr std::array<char, 16> zeros{};
  for (std::uint64_t remaining = pad; remaining != 0U;) {
    auto const chunk = static_cast<std::streamsize>(
        std::min<std::uint64_t>(remaining, zeros.size()));
    out.write(zeros.data(), chunk);
    if (!out.good()) {
      return false;
    }
    remaining -= static_cast<std::uint64_t>(chunk);
  }
  return true;
}

auto lod_from_u32(std::uint32_t raw,
                  Render::Creature::CreatureLOD &out) -> bool {
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

} // namespace

auto SnapshotMeshBlob::from_bytes(std::vector<std::uint8_t> bytes)
    -> SnapshotMeshBlob {
  SnapshotMeshBlob blob{};
  blob.m_bytes = std::move(bytes);
  blob.m_loaded = blob.validate();
  return blob;
}

auto SnapshotMeshBlob::from_file(const std::string &path) -> SnapshotMeshBlob {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    SnapshotMeshBlob blob{};
    blob.m_last_error = "failed to open " + path;
    return blob;
  }
  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
  return from_bytes(std::move(data));
}

bool SnapshotMeshBlob::validate() {
  m_last_error.clear();
  m_header = nullptr;
  m_clip_table = nullptr;
  m_string_table = nullptr;
  m_indices = nullptr;
  m_vertices = nullptr;

  if (m_bytes.size() < sizeof(SnapshotMeshHeader)) {
    m_last_error = "file shorter than header";
    return false;
  }

  auto const *header =
      reinterpret_cast<const SnapshotMeshHeader *>(m_bytes.data());
  if (std::memcmp(header->magic, kMagic.data(), kMagic.size()) != 0) {
    m_last_error = "magic mismatch";
    return false;
  }
  if (header->version != kVersion) {
    m_last_error = "unsupported version";
    return false;
  }
  if (header->species_id > Render::Creature::Bpat::kMaxSpeciesId) {
    m_last_error = "unknown species_id";
    return false;
  }
  Render::Creature::CreatureLOD parsed_lod{};
  if (!lod_from_u32(header->lod, parsed_lod)) {
    m_last_error = "unsupported lod";
    return false;
  }
  if (header->vertex_count == 0U || header->index_count == 0U ||
      header->clip_count == 0U || header->frame_total == 0U) {
    m_last_error = "counts must be non-zero";
    return false;
  }

  std::uint64_t const file_size = m_bytes.size();
  auto in_bounds = [file_size](std::uint64_t offset, std::uint64_t bytes) {
    return offset <= file_size && offset + bytes <= file_size;
  };

  if (!in_bounds(header->clip_table_offset,
                 std::uint64_t{header->clip_count} *
                     sizeof(SnapshotMeshClipEntry))) {
    m_last_error = "clip table out of bounds";
    return false;
  }
  if (!in_bounds(header->string_table_offset, header->string_table_size)) {
    m_last_error = "string table out of bounds";
    return false;
  }
  if (!in_bounds(header->index_data_offset,
                 std::uint64_t{header->index_count} * sizeof(std::uint32_t))) {
    m_last_error = "index data out of bounds";
    return false;
  }
  if (!in_bounds(header->vertex_data_offset,
                 std::uint64_t{header->frame_total} * header->vertex_count *
                     sizeof(Render::GL::RiggedVertex))) {
    m_last_error = "vertex data out of bounds";
    return false;
  }

  m_clip_table = reinterpret_cast<const SnapshotMeshClipEntry *>(
      m_bytes.data() + header->clip_table_offset);
  m_string_table = reinterpret_cast<const char *>(m_bytes.data() +
                                                  header->string_table_offset);
  m_indices = reinterpret_cast<const std::uint32_t *>(
      m_bytes.data() + header->index_data_offset);
  m_vertices = reinterpret_cast<const Render::GL::RiggedVertex *>(
      m_bytes.data() + header->vertex_data_offset);

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

  m_header = header;
  (void)parsed_lod;
  return true;
}

auto SnapshotMeshBlob::species_id() const noexcept -> std::uint32_t {
  return m_header != nullptr ? m_header->species_id : 0U;
}

auto SnapshotMeshBlob::lod() const noexcept -> Render::Creature::CreatureLOD {
  Render::Creature::CreatureLOD out = Render::Creature::CreatureLOD::Full;
  if (m_header == nullptr) {
    return out;
  }
  (void)lod_from_u32(m_header->lod, out);
  return out;
}

auto SnapshotMeshBlob::vertex_count() const noexcept -> std::uint32_t {
  return m_header != nullptr ? m_header->vertex_count : 0U;
}

auto SnapshotMeshBlob::index_count() const noexcept -> std::uint32_t {
  return m_header != nullptr ? m_header->index_count : 0U;
}

auto SnapshotMeshBlob::clip_count() const noexcept -> std::uint32_t {
  return m_header != nullptr ? m_header->clip_count : 0U;
}

auto SnapshotMeshBlob::frame_total() const noexcept -> std::uint32_t {
  return m_header != nullptr ? m_header->frame_total : 0U;
}

auto SnapshotMeshBlob::clip(std::uint32_t index) const -> ClipView {
  ClipView v{};
  if (m_header == nullptr || index >= m_header->clip_count) {
    return v;
  }
  auto const &e = m_clip_table[index];
  v.name = std::string_view(m_string_table + e.name_offset, e.name_length);
  v.frame_count = e.frame_count;
  v.frame_offset = e.frame_offset;
  return v;
}

auto SnapshotMeshBlob::indices_view() const -> std::span<const std::uint32_t> {
  if (m_header == nullptr || m_indices == nullptr) {
    return {};
  }
  return {m_indices, m_header->index_count};
}

auto SnapshotMeshBlob::frame_vertices_view(std::uint32_t global_frame_index)
    const -> std::span<const Render::GL::RiggedVertex> {
  if (m_header == nullptr || m_vertices == nullptr ||
      global_frame_index >= m_header->frame_total) {
    return {};
  }
  std::size_t const off =
      static_cast<std::size_t>(global_frame_index) * m_header->vertex_count;
  return {m_vertices + off, m_header->vertex_count};
}

auto SnapshotMeshBlob::resolve_global_frame(std::uint32_t clip_index,
                                            std::uint32_t frame_in_clip,
                                            std::uint32_t &out) const -> bool {
  out = 0U;
  if (m_header == nullptr || clip_index >= m_header->clip_count) {
    return false;
  }
  auto const c = m_clip_table[clip_index];
  if (c.frame_count == 0U) {
    return false;
  }
  out = c.frame_offset + (frame_in_clip % c.frame_count);
  return true;
}

SnapshotMeshWriter::SnapshotMeshWriter(
    std::uint32_t species_id, Render::Creature::CreatureLOD lod,
    std::uint32_t vertex_count, std::span<const std::uint32_t> indices) noexcept
    : m_species_id(species_id), m_lod(lod), m_vertex_count(vertex_count),
      m_indices(indices.begin(), indices.end()) {}

void SnapshotMeshWriter::add_clip(ClipDescriptor desc) {
  PendingClip pending{};
  pending.frame_offset = m_frame_total;
  m_frame_total += desc.frame_count;
  pending.desc = std::move(desc);
  m_clips.push_back(std::move(pending));
}

void SnapshotMeshWriter::append_clip_vertices(
    std::span<const Render::GL::RiggedVertex> frame_vertices) {
  assert(!m_clips.empty() && "add_clip() before append_clip_vertices()");
  auto &pending = m_clips.back();
  assert(!pending.vertices_appended && "vertices already appended for clip");
  std::size_t const expected =
      static_cast<std::size_t>(m_vertex_count) * pending.desc.frame_count;
  assert(frame_vertices.size() == expected &&
         "frame_vertices.size() must equal vertex_count * frame_count");
  m_vertices.insert(m_vertices.end(), frame_vertices.begin(),
                    frame_vertices.end());
  pending.vertices_appended = true;
}

auto SnapshotMeshWriter::write(std::ostream &out) const -> bool {
  if (m_vertex_count == 0U || m_indices.empty() || m_clips.empty()) {
    return false;
  }
  for (auto const &c : m_clips) {
    if (!c.vertices_appended || c.desc.frame_count == 0U) {
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

  std::vector<SnapshotMeshClipEntry> clip_entries;
  clip_entries.reserve(m_clips.size());
  for (auto const &c : m_clips) {
    SnapshotMeshClipEntry e{};
    e.name_offset = intern(c.desc.name);
    e.name_length = static_cast<std::uint32_t>(c.desc.name.size());
    e.frame_count = c.desc.frame_count;
    e.frame_offset = c.frame_offset;
    clip_entries.push_back(e);
  }

  std::uint64_t cursor = sizeof(SnapshotMeshHeader);
  std::uint64_t const clip_table_offset = cursor;
  cursor += clip_entries.size() * sizeof(SnapshotMeshClipEntry);
  std::uint64_t const string_table_offset = cursor;
  cursor += string_table.size();
  cursor = Render::Creature::Bpat::align_up(
      cursor, Render::Creature::Bpat::kSectionAlignment);
  std::uint64_t const index_data_offset = cursor;
  cursor += m_indices.size() * sizeof(std::uint32_t);
  std::uint64_t const vertex_data_offset = cursor;

  SnapshotMeshHeader header{};
  std::memcpy(header.magic, kMagic.data(), kMagic.size());
  header.version = kVersion;
  header.species_id = m_species_id;
  header.lod = static_cast<std::uint32_t>(m_lod);
  header.vertex_count = m_vertex_count;
  header.index_count = static_cast<std::uint32_t>(m_indices.size());
  header.clip_count = static_cast<std::uint32_t>(clip_entries.size());
  header.frame_total = m_frame_total;
  header.string_table_size = static_cast<std::uint32_t>(string_table.size());
  header.clip_table_offset = clip_table_offset;
  header.string_table_offset = string_table_offset;
  header.index_data_offset = index_data_offset;
  header.vertex_data_offset = vertex_data_offset;

  std::uint64_t written = 0U;
  if (!write_pod(out, &header, sizeof(header))) {
    return false;
  }
  written += sizeof(header);

  if (!clip_entries.empty()) {
    if (!write_pod(out, clip_entries.data(),
                   clip_entries.size() * sizeof(SnapshotMeshClipEntry))) {
      return false;
    }
    written += clip_entries.size() * sizeof(SnapshotMeshClipEntry);
  }
  if (!string_table.empty()) {
    if (!write_pod(out, string_table.data(), string_table.size())) {
      return false;
    }
    written += string_table.size();
  }
  if (!pad_to_alignment(out, written,
                        Render::Creature::Bpat::kSectionAlignment)) {
    return false;
  }

  if (!m_indices.empty()) {
    if (!write_pod(out, m_indices.data(),
                   m_indices.size() * sizeof(std::uint32_t))) {
      return false;
    }
  }
  if (!m_vertices.empty()) {
    if (!write_pod(out, m_vertices.data(),
                   m_vertices.size() * sizeof(Render::GL::RiggedVertex))) {
      return false;
    }
  }
  return out.good();
}

} // namespace Render::Creature::Snapshot
