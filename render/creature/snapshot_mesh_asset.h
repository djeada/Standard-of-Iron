#pragma once

#include "../rigged_mesh.h"
#include "bpat/bpat_format.h"
#include "render_request.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Render::Creature::Snapshot {

inline constexpr std::array<std::uint8_t, 4> kMagic{'B', 'P', 'S', 'M'};
inline constexpr std::uint32_t kVersion = 1U;

struct SnapshotMeshHeader {
  std::uint8_t magic[4];
  std::uint32_t version;
  std::uint32_t species_id;
  std::uint32_t lod;
  std::uint32_t vertex_count;
  std::uint32_t index_count;
  std::uint32_t clip_count;
  std::uint32_t frame_total;
  std::uint32_t string_table_size;
  std::uint32_t reserved;
  std::uint64_t clip_table_offset;
  std::uint64_t string_table_offset;
  std::uint64_t index_data_offset;
  std::uint64_t vertex_data_offset;
};

static_assert(sizeof(SnapshotMeshHeader) == 72,
              "SnapshotMeshHeader must be exactly 72 bytes");

struct SnapshotMeshClipEntry {
  std::uint32_t name_offset;
  std::uint32_t name_length;
  std::uint32_t frame_count;
  std::uint32_t frame_offset;
  std::uint32_t reserved[4];
};

static_assert(sizeof(SnapshotMeshClipEntry) == 32,
              "SnapshotMeshClipEntry must be exactly 32 bytes");

struct ClipDescriptor {
  std::string name;
  std::uint32_t frame_count{0U};
};

struct ClipView {
  std::string_view name{};
  std::uint32_t frame_count{0U};
  std::uint32_t frame_offset{0U};
};

class SnapshotMeshBlob {
public:
  static auto from_bytes(std::vector<std::uint8_t> bytes) -> SnapshotMeshBlob;
  static auto from_file(const std::string &path) -> SnapshotMeshBlob;

  [[nodiscard]] auto loaded() const noexcept -> bool { return m_loaded; }
  [[nodiscard]] auto last_error() const noexcept -> std::string_view {
    return m_last_error;
  }

  [[nodiscard]] auto species_id() const noexcept -> std::uint32_t;
  [[nodiscard]] auto lod() const noexcept -> Render::Creature::CreatureLOD;
  [[nodiscard]] auto vertex_count() const noexcept -> std::uint32_t;
  [[nodiscard]] auto index_count() const noexcept -> std::uint32_t;
  [[nodiscard]] auto clip_count() const noexcept -> std::uint32_t;
  [[nodiscard]] auto frame_total() const noexcept -> std::uint32_t;

  [[nodiscard]] auto clip(std::uint32_t index) const -> ClipView;
  [[nodiscard]] auto indices_view() const -> std::span<const std::uint32_t>;
  [[nodiscard]] auto frame_vertices_view(std::uint32_t global_frame_index) const
      -> std::span<const Render::GL::RiggedVertex>;
  [[nodiscard]] auto resolve_global_frame(std::uint32_t clip_index,
                                          std::uint32_t frame_in_clip,
                                          std::uint32_t &out) const -> bool;

private:
  bool validate();

  std::vector<std::uint8_t> m_bytes{};
  bool m_loaded{false};
  std::string m_last_error{};
  const SnapshotMeshHeader *m_header{nullptr};
  const SnapshotMeshClipEntry *m_clip_table{nullptr};
  const char *m_string_table{nullptr};
  const std::uint32_t *m_indices{nullptr};
  const Render::GL::RiggedVertex *m_vertices{nullptr};
};

class SnapshotMeshWriter {
public:
  SnapshotMeshWriter(std::uint32_t species_id,
                     Render::Creature::CreatureLOD lod,
                     std::uint32_t vertex_count,
                     std::span<const std::uint32_t> indices) noexcept;

  void add_clip(ClipDescriptor desc);
  void append_clip_vertices(
      std::span<const Render::GL::RiggedVertex> frame_vertices);

  [[nodiscard]] auto write(std::ostream &out) const -> bool;

private:
  struct PendingClip {
    ClipDescriptor desc;
    std::uint32_t frame_offset{0U};
    bool vertices_appended{false};
  };

  std::uint32_t m_species_id;
  Render::Creature::CreatureLOD m_lod;
  std::uint32_t m_vertex_count;
  std::uint32_t m_frame_total{0U};
  std::vector<std::uint32_t> m_indices{};
  std::vector<PendingClip> m_clips{};
  std::vector<Render::GL::RiggedVertex> m_vertices{};
};

} // namespace Render::Creature::Snapshot
