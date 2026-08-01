#pragma once

#include <cstdint>
#include <span>

#include "../creature/species_manifest.h"

namespace Render::Humanoid {

enum class BakeProfile : std::uint8_t {
  Default,
  SwordReady,
  SpearReady,
  Skeleton,
  Caster,
  StaveCaster
};

[[nodiscard]] auto humanoid_manifest(BakeProfile profile) noexcept
    -> const Render::Creature::SpeciesManifest&;

[[nodiscard]] auto humanoid_bake_profiles() noexcept -> std::span<const BakeProfile>;

} // namespace Render::Humanoid
