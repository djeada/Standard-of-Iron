#pragma once

#include "../creature/species_manifest.h"

namespace Render::Wildlife {

[[nodiscard]] auto wolf_manifest() noexcept -> const Render::Creature::SpeciesManifest&;

} // namespace Render::Wildlife
