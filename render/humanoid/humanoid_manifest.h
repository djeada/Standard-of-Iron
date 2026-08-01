#pragma once

#include <cstdint>
#include <span>

#include "../creature/species_manifest.h"

namespace Render::Humanoid {

// The humanoid ships one baked animation table per equipment stance. They share
// a skeleton, a clip list and a socket list; only the poses differ.
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

// Every profile, in the order they are baked.
[[nodiscard]] auto humanoid_bake_profiles() noexcept -> std::span<const BakeProfile>;

} // namespace Render::Humanoid
