#pragma once

#include "snapshot_mesh_asset.h"

#include <array>
#include <string>
#include <string_view>

namespace Render::Creature::Snapshot {

class SnapshotMeshRegistry {
public:
  [[nodiscard]] static auto instance() noexcept -> SnapshotMeshRegistry &;

  auto load_species(std::uint32_t species_id, Render::Creature::CreatureLOD lod,
                    const std::string &path) -> bool;
  auto load_all(const std::string &asset_root) -> std::size_t;

  [[nodiscard]] auto blob(std::uint32_t species_id,
                          Render::Creature::CreatureLOD lod) const noexcept
      -> const SnapshotMeshBlob *;

  void clear();

  [[nodiscard]] auto last_error() const noexcept -> std::string_view {
    return m_last_error;
  }

private:
  SnapshotMeshRegistry() = default;

  [[nodiscard]] auto
  slot(std::uint32_t species_id,
       Render::Creature::CreatureLOD lod) noexcept -> SnapshotMeshBlob *;
  [[nodiscard]] auto slot(std::uint32_t species_id,
                          Render::Creature::CreatureLOD lod) const noexcept
      -> const SnapshotMeshBlob *;

  std::array<SnapshotMeshBlob, Render::Creature::Bpat::kSpeciesCount * 2U>
      m_blobs{};
  std::string m_last_error{};
};

} // namespace Render::Creature::Snapshot
