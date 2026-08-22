#pragma once

#include "render/creature/bake/creature_bake_recipe.h"

namespace Render::Horse {

[[nodiscard]] auto
horse_bake_recipe() noexcept -> const Render::Creature::CreatureBakeRecipe&;

}
