#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "../rigged_mesh.h"
#include "animation/bpat/bpat_format.h"
#include "render_request.h"

namespace Render::Creature::Rigged {

inline constexpr std::array<std::uint8_t, 4> k_magic{'B', 'P', 'R', 'M'};
inline constexpr std::uint32_t k_version = 1U;

struct RiggedMeshHeader {
  std::uint8_t magic[4];
  std::uint32_t version;
  std::uint32_t lod;
  std::uint32_t vertex_count;
  std::uint32_t index_count;
  std::uint32_t reserved;
  std::uint64_t index_data_offset;
  std::uint64_t vertex_data_offset;
};

static_assert(sizeof(RiggedMeshHeader) == 40,
              "RiggedMeshHeader must be exactly 40 bytes");

class RiggedMeshBlob {
public:
  static auto from_bytes(std::vector<std::uint8_t> bytes) -> RiggedMeshBlob;
  static auto from_file(const std::string& path) -> RiggedMeshBlob;

  [[nodiscard]] auto loaded() const noexcept -> bool { return m_loaded; }
  [[nodiscard]] auto last_error() const noexcept -> std::string_view {
    return m_last_error;
  }

  [[nodiscard]] auto lod() const noexcept -> Render::Creature::CreatureLOD;
  [[nodiscard]] auto vertices_view() const -> std::span<const Render::GL::RiggedVertex>;
  [[nodiscard]] auto indices_view() const -> std::span<const std::uint32_t>;

private:
  bool validate();

  std::vector<std::uint8_t> m_bytes{};
  bool m_loaded{false};
  std::string m_last_error{};
  const RiggedMeshHeader* m_header{nullptr};
  const std::uint32_t* m_indices{nullptr};
  const Render::GL::RiggedVertex* m_vertices{nullptr};
};

class RiggedMeshWriter {
public:
  RiggedMeshWriter(Render::Creature::CreatureLOD lod,
                   std::span<const Render::GL::RiggedVertex> vertices,
                   std::span<const std::uint32_t> indices);

  [[nodiscard]] auto write(std::ostream& out) const -> bool;

private:
  Render::Creature::CreatureLOD m_lod;
  std::vector<Render::GL::RiggedVertex> m_vertices{};
  std::vector<std::uint32_t> m_indices{};
};

[[nodiscard]] auto asset_file_name(std::string_view species_name,
                                   Render::Creature::CreatureLOD lod) -> std::string;

} // namespace Render::Creature::Rigged
