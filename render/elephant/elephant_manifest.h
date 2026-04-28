#pragma once

#include "../creature/species_manifest.h"

namespace Render::Elephant {

[[nodiscard]] auto elephant_manifest() noexcept
    -> const Render::Creature::SpeciesManifest &;

} // namespace Render::Elephant
