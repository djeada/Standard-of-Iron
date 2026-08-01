#include "rigged_mesh_asset.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <ostream>

namespace Render::Creature::Rigged {

namespace {

constexpr std::size_t k_write_chunk_bytes = 1U << 20;
constexpr std::uint64_t k_data_alignment = 16U;

auto write_pod(std::ostream& out, const void* src, std::size_t bytes) -> bool {
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

auto pad_to_alignment(std::ostream& out, std::uint64_t current) -> bool {
  std::uint64_t const padded =
      Render::Creature::Bpat::align_up(current, k_data_alignment);
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

auto lod_from_u32(std::uint32_t raw, Render::Creature::CreatureLOD& out) -> bool {
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

auto lod_suffix(Render::Creature::CreatureLOD lod) -> std::string_view {
  switch (lod) {
  case Render::Creature::CreatureLOD::Minimal:
    return "minimal";
  case Render::Creature::CreatureLOD::Full:
  default:
    return "full";
  }
}

} // namespace

auto asset_file_name(std::string_view species_name,
                     Render::Creature::CreatureLOD lod) -> std::string {
  std::string name(species_name);
  // Species names such as "humanoid.sword_ready" become file-name friendly.
  std::replace(name.begin(), name.end(), '.', '_');
  name += '_';
  name += lod_suffix(lod);
  name += ".bprm";
  return name;
}

auto RiggedMeshBlob::from_bytes(std::vector<std::uint8_t> bytes) -> RiggedMeshBlob {
  RiggedMeshBlob blob{};
  blob.m_bytes = std::move(bytes);
  blob.m_loaded = blob.validate();
  return blob;
}

auto RiggedMeshBlob::from_file(const std::string& path) -> RiggedMeshBlob {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    RiggedMeshBlob blob{};
    blob.m_last_error = "failed to open " + path;
    return blob;
  }
  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
  return from_bytes(std::move(data));
}

bool RiggedMeshBlob::validate() {
  m_last_error.clear();
  m_header = nullptr;
  m_indices = nullptr;
  m_vertices = nullptr;

  if (m_bytes.size() < sizeof(RiggedMeshHeader)) {
    m_last_error = "file shorter than header";
    return false;
  }

  auto const* header = reinterpret_cast<const RiggedMeshHeader*>(m_bytes.data());
  if (std::memcmp(header->magic, k_magic.data(), k_magic.size()) != 0) {
    m_last_error = "magic mismatch";
    return false;
  }
  if (header->version != k_version) {
    m_last_error = "unsupported version";
    return false;
  }
  Render::Creature::CreatureLOD parsed_lod{};
  if (!lod_from_u32(header->lod, parsed_lod)) {
    m_last_error = "unsupported lod";
    return false;
  }
  if (header->vertex_count == 0U || header->index_count == 0U) {
    m_last_error = "mesh is empty";
    return false;
  }
  if (header->index_count % 3U != 0U) {
    m_last_error = "index count is not a whole number of triangles";
    return false;
  }

  auto const index_bytes =
      static_cast<std::uint64_t>(header->index_count) * sizeof(std::uint32_t);
  auto const vertex_bytes = static_cast<std::uint64_t>(header->vertex_count) *
                            sizeof(Render::GL::RiggedVertex);
  if (header->index_data_offset + index_bytes > m_bytes.size() ||
      header->vertex_data_offset + vertex_bytes > m_bytes.size()) {
    m_last_error = "data extends past end of file";
    return false;
  }

  auto const* indices = reinterpret_cast<const std::uint32_t*>(
      m_bytes.data() + header->index_data_offset);
  for (std::uint32_t i = 0; i < header->index_count; ++i) {
    if (indices[i] >= header->vertex_count) {
      m_last_error = "index out of range";
      return false;
    }
  }

  m_header = header;
  m_indices = indices;
  m_vertices = reinterpret_cast<const Render::GL::RiggedVertex*>(
      m_bytes.data() + header->vertex_data_offset);
  return true;
}

auto RiggedMeshBlob::lod() const noexcept -> Render::Creature::CreatureLOD {
  Render::Creature::CreatureLOD out = Render::Creature::CreatureLOD::Full;
  if (m_header != nullptr) {
    (void)lod_from_u32(m_header->lod, out);
  }
  return out;
}

auto RiggedMeshBlob::vertices_view() const
    -> std::span<const Render::GL::RiggedVertex> {
  if (m_header == nullptr || m_vertices == nullptr) {
    return {};
  }
  return {m_vertices, m_header->vertex_count};
}

auto RiggedMeshBlob::indices_view() const -> std::span<const std::uint32_t> {
  if (m_header == nullptr || m_indices == nullptr) {
    return {};
  }
  return {m_indices, m_header->index_count};
}

RiggedMeshWriter::RiggedMeshWriter(Render::Creature::CreatureLOD lod,
                                   std::span<const Render::GL::RiggedVertex> vertices,
                                   std::span<const std::uint32_t> indices)
    : m_lod(lod),
      m_vertices(vertices.begin(), vertices.end()),
      m_indices(indices.begin(), indices.end()) {}

auto RiggedMeshWriter::write(std::ostream& out) const -> bool {
  if (m_vertices.empty() || m_indices.empty() || m_indices.size() % 3U != 0U) {
    return false;
  }
  for (std::uint32_t const index : m_indices) {
    if (index >= m_vertices.size()) {
      return false;
    }
  }

  RiggedMeshHeader header{};
  std::memcpy(header.magic, k_magic.data(), k_magic.size());
  header.version = k_version;
  header.lod = static_cast<std::uint32_t>(m_lod);
  header.vertex_count = static_cast<std::uint32_t>(m_vertices.size());
  header.index_count = static_cast<std::uint32_t>(m_indices.size());

  std::uint64_t cursor =
      Render::Creature::Bpat::align_up(sizeof(RiggedMeshHeader), k_data_alignment);
  header.index_data_offset = cursor;
  cursor += static_cast<std::uint64_t>(m_indices.size()) * sizeof(std::uint32_t);
  cursor = Render::Creature::Bpat::align_up(cursor, k_data_alignment);
  header.vertex_data_offset = cursor;

  if (!write_pod(out, &header, sizeof(header))) {
    return false;
  }
  if (!pad_to_alignment(out, sizeof(header))) {
    return false;
  }
  if (!write_pod(
          out, m_indices.data(), m_indices.size() * sizeof(std::uint32_t))) {
    return false;
  }
  if (!pad_to_alignment(out,
                        header.index_data_offset +
                            static_cast<std::uint64_t>(m_indices.size()) *
                                sizeof(std::uint32_t))) {
    return false;
  }
  return write_pod(
      out, m_vertices.data(), m_vertices.size() * sizeof(Render::GL::RiggedVertex));
}

} // namespace Render::Creature::Rigged
