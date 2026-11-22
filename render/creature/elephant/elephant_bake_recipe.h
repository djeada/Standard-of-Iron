#pragma once

#include "render/creature/bake/creature_bake_recipe.h"

namespace Render::Elephant {

[[nodiscard]] auto
elephant_bake_recipe() noexcept -> const Render::Creature::CreatureBakeRecipe&;

}
