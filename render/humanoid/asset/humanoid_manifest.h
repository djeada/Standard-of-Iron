#pragma once

#include <cstdint>
#include <span>

#include "render/creature/bake/creature_bake_recipe.h"
#include "render/creature/schema/creature_runtime_manifest.h"

namespace Render::Humanoid {

enum class BakeProfile : std::uint8_t {
  Default,
  SwordReady,
  SpearReady,
  Skeleton,
  Caster,
  StaveCaster
};

[[nodiscard]] auto humanoid_runtime_manifest(BakeProfile profile) noexcept
    -> const Render::Creature::CreatureRuntimeManifest&;

[[nodiscard]] auto humanoid_bake_recipe(BakeProfile profile) noexcept
    -> const Render::Creature::CreatureBakeRecipe&;

[[nodiscard]] auto humanoid_bake_profiles() noexcept -> std::span<const BakeProfile>;

} // namespace Render::Humanoid
