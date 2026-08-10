#pragma once

#include "render/creature/species_manifest.h"

namespace Render::Wildlife {

[[nodiscard]] auto sheep_graze_amount(float phase) noexcept -> float;

[[nodiscard]] auto
sheep_manifest() noexcept -> const Render::Creature::SpeciesManifest&;

} // namespace Render::Wildlife
