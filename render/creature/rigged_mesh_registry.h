#pragma once

#include <array>
#include <string>
#include <string_view>

#include "rigged_mesh_asset.h"

namespace Render::Creature::Rigged {

// Holds the build-time body meshes so the renderer can look one up instead of
// building it from the part graph on the fly.
class RiggedMeshRegistry {
public:
  [[nodiscard]] static auto instance() noexcept -> RiggedMeshRegistry&;

  auto load_species(std::uint32_t species_id,
                    Render::Creature::CreatureLOD lod,
                    const std::string& path) -> bool;

  // Loads every species/LOD pair present under the asset root. Missing files
  // are not an error: a species without a baked body simply falls back to the
  // part graph.
  auto load_all(const std::string& asset_root) -> std::size_t;

  [[nodiscard]] auto blob(std::uint32_t species_id,
                          Render::Creature::CreatureLOD lod) const noexcept
      -> const RiggedMeshBlob*;

  void clear();

  [[nodiscard]] auto last_error() const noexcept -> std::string_view {
    return m_last_error;
  }

private:
  RiggedMeshRegistry() = default;

  [[nodiscard]] auto slot(std::uint32_t species_id,
                          Render::Creature::CreatureLOD lod) noexcept
      -> RiggedMeshBlob*;
  [[nodiscard]] auto slot(std::uint32_t species_id,
                          Render::Creature::CreatureLOD lod) const noexcept
      -> const RiggedMeshBlob*;

  std::array<RiggedMeshBlob, Render::Creature::Bpat::k_species_count * 2U> m_blobs{};
  std::string m_last_error{};
};

} // namespace Render::Creature::Rigged
