#pragma once

#include <array>
#include <map>
#include <string>
#include <string_view>

#include "rigged_mesh_asset.h"

namespace Render::Creature::Rigged {

class RiggedMeshRegistry {
public:
  [[nodiscard]] static auto instance() noexcept -> RiggedMeshRegistry&;

  auto load_species(std::uint32_t species_id,
                    Render::Creature::CreatureLOD lod,
                    const std::string& path) -> bool;

  auto load_all(const std::string& asset_root) -> std::size_t;

  [[nodiscard]] auto
  blob(std::uint32_t species_id,
       Render::Creature::CreatureLOD lod) const noexcept -> const RiggedMeshBlob*;

  auto load_body(std::string_view body_name,
                 Render::Creature::CreatureLOD lod,
                 const std::string& path) -> bool;

  [[nodiscard]] auto
  body(std::string_view body_name,
       Render::Creature::CreatureLOD lod) const noexcept -> const RiggedMeshBlob*;

  void clear();

  [[nodiscard]] auto last_error() const noexcept -> std::string_view {
    return m_last_error;
  }

private:
  RiggedMeshRegistry() = default;

  [[nodiscard]] auto
  slot(std::uint32_t species_id,
       Render::Creature::CreatureLOD lod) noexcept -> RiggedMeshBlob*;
  [[nodiscard]] auto
  slot(std::uint32_t species_id,
       Render::Creature::CreatureLOD lod) const noexcept -> const RiggedMeshBlob*;

  std::array<RiggedMeshBlob, Render::Creature::Bpat::k_species_count * 2U> m_blobs{};
  std::map<std::string, std::array<RiggedMeshBlob, 2>, std::less<>> m_bodies{};
  std::string m_last_error{};
};

} // namespace Render::Creature::Rigged
