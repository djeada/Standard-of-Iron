#pragma once

#include "../creature/species_manifest.h"

namespace Render::Wildlife {

[[nodiscard]] auto
sheep_manifest() noexcept -> const Render::Creature::SpeciesManifest&;

} // namespace Render::Wildlife
