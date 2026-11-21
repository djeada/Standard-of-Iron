#pragma once

#include "render/creature/bake/creature_bake_recipe.h"
#include "render/creature/schema/creature_runtime_manifest.h"

namespace Render::Wildlife {

[[nodiscard]] auto
wolf_runtime_manifest() noexcept -> const Render::Creature::CreatureRuntimeManifest&;

[[nodiscard]] auto
wolf_bake_recipe() noexcept -> const Render::Creature::CreatureBakeRecipe&;

} // namespace Render::Wildlife
