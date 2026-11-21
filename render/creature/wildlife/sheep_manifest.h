#pragma once

#include "render/creature/bake/creature_bake_recipe.h"
#include "render/creature/schema/creature_runtime_manifest.h"

namespace Render::Wildlife {

[[nodiscard]] auto sheep_graze_amount(float phase) noexcept -> float;

[[nodiscard]] auto
sheep_runtime_manifest() noexcept -> const Render::Creature::CreatureRuntimeManifest&;

[[nodiscard]] auto
sheep_bake_recipe() noexcept -> const Render::Creature::CreatureBakeRecipe&;

} // namespace Render::Wildlife
