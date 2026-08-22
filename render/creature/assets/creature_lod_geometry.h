#pragma once

#include <cstdint>
#include <string_view>

#include "render/creature/schema/creature_runtime_manifest.h"

namespace Render::Creature {

struct CreatureLodGeometry {
  CompiledWholeMeshLod minimal;
  CompiledWholeMeshLod full;
};

[[nodiscard]] auto compile_creature_lod_geometry(
    const CreatureRuntimeManifest& manifest) -> CreatureLodGeometry;

[[nodiscard]] auto creature_lod_geometry_compile_count() noexcept -> std::uint64_t;

class CreatureLodGeometrySlot {
public:
  using ManifestFn = const CreatureRuntimeManifest& (*)() noexcept;

  CreatureLodGeometrySlot(ManifestFn manifest, std::string_view species) noexcept
      : m_manifest(manifest)
      , m_species(species) {}

  void initialize();

  [[nodiscard]] auto get() -> const CreatureLodGeometry&;

  [[nodiscard]] auto initialized() const noexcept -> bool { return m_initialized; }

private:
  ManifestFn m_manifest;
  std::string_view m_species;
  bool m_initialized{false};
  CreatureLodGeometry m_geometry{};
};

} // namespace Render::Creature
